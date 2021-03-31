/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "lv_ex_conf.h"
#include "lvgl.h"
/*
 This code displays some fancy graphics on the 320x240 LCD on an ESP-WROVER_KIT board.
 This example demonstrates the use of both spi_device_transmit as well as
 spi_device_queue_trans/spi_device_get_trans_result and pre-transmit callbacks.

 Some info about the ILI9341/ST7789V: It has an C/D line, which is connected to a GPIO here. It expects this
 line to be low for a command and high for data. We use a pre-transmit callback here to control that
 line: every transaction has as the user-definable argument the needed state of the D/C line and just
 before the transaction is sent, the callback will set this line to the correct state.
*/

#define LCD_HOST VSPI_HOST
#define DMA_CHAN 2

#define PIN_NUM_MISO 25
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK 18
#define PIN_NUM_CS 27

#define PIN_NUM_DC 2
#define PIN_NUM_RST 4
#define PIN_NUM_BCKL 5

//To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many. More means more memory use,
//but less overhead for setting up / finishing transfers. Make sure 240 is dividable by this.
#define PARALLEL_LINES 16

spi_device_handle_t spi_g;

/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct
{
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

typedef enum
{
    LCD_TYPE_ILI = 1,
    LCD_TYPE_ST,
    LCD_TYPE_MAX,
} type_lcd_t;

//Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.
DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[] = {
    /* Memory Data Access Control, MX=MV=1, MY=ML=MH=0, RGB=0 */
    {0x36, {0x00}, 1},
    /* Interface Pixel Format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x05}, 1},
    /* Porch Setting */
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    /* Gate Control, Vgh=13.65V, Vgl=-10.43V */
    {0xB7, {0x35}, 1},
    /* VCOM Setting, VCOM=1.175V */
    {0xBB, {0x19}, 1},
    /* LCM Control, XOR: BGR, MX, MH */
    {0xC0, {0x2C}, 1},
    /* VDV and VRH Command Enable, enable=1 */
    {0xC2, {0x01}, 1},
    /* VRH Set, Vap=4.4+... */
    {0xC3, {0x12}, 1},
    /* VDV Set, VDV=0 */
    {0xC4, {0x20}, 1},
    /* Frame Rate Control, 60Hz, inversion=0 */
    {0xC6, {0x0f}, 1},
    /* Power Control 1, AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V */
    {0xD0, {0xA4, 0xA1}, 2},
    /* Positive Voltage Gamma Control */
    {0xE0, {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}, 14},
    /* Negative Voltage Gamma Control */
    {0xE1, {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}, 14},
    {0x21, {0}, 0x0},
    /* Sleep Out */
    {0x11, {0}, 0x80},
    /* Display On */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff}};

/* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));                   //Zero out the transaction
    t.length = 8;                               //Command is 8 bits
    t.tx_buffer = &cmd;                         //The data is the cmd itself
    t.user = (void *)0;                         //D/C needs to be set to 0
    ret = spi_device_polling_transmit(spi, &t); //Transmit!
    assert(ret == ESP_OK);                      //Should have had no issues.
}

/* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0)
        return;                                 //no need to send anything
    memset(&t, 0, sizeof(t));                   //Zero out the transaction
    t.length = len * 8;                         //Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;                         //Data
    t.user = (void *)1;                         //D/C needs to be set to 1
    ret = spi_device_polling_transmit(spi, &t); //Transmit!
    assert(ret == ESP_OK);                      //Should have had no issues.
}

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}


//Initialize the display
void lcd_init(spi_device_handle_t spi)
{
    int cmd = 0;
    const lcd_init_cmd_t *lcd_init_cmds;

    //Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);

    //Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);

    lcd_init_cmds = st_init_cmds;
    //Send all the commands
    while (lcd_init_cmds[cmd].databytes != 0xff)
    {
        lcd_cmd(spi, lcd_init_cmds[cmd].cmd);
        lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes & 0x1F);
        if (lcd_init_cmds[cmd].databytes & 0x80)
        {
            vTaskDelay(120 / portTICK_RATE_MS);
        }
        cmd++;
    }

    ///Enable backlight
    gpio_set_level(PIN_NUM_BCKL, 0);
}

static void send_line_finish(spi_device_handle_t spi)
{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    //Wait for all 6 transactions to be done and get back the results.
    for (int x = 0; x < 6; x++)
    {
        ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret == ESP_OK);
        //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
    }
}
static void send_line_finish_lvgl(lv_disp_drv_t *disp_drv, spi_device_handle_t spi)
{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    //Wait for all 6 transactions to be done and get back the results.
    for (int x = 0; x < 6; x++)
    {
        ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret == ESP_OK);
        //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
    }
    lv_disp_flush_ready(disp_drv);
}

void send_lines(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    esp_err_t ret;
    // int sending_line=-1;
    // int calc_line=0;
    int j, x;
    //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
    //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
    static spi_transaction_t trans[6];
    //uint16_t *lines[2];
    //Allocate memory for the pixel buffer
    // for (int i=0; i<2; i++) {
    //     lines[i]=heap_caps_malloc((area->y2 - area->y1) * (area->x2 - area->x1) *2*sizeof(uint16_t), MALLOC_CAP_DMA);
    //    // assert(lines[i]!=NULL);
    // }
    //if (sending_line != -1)
    // for(int i =0;i<area->y2;i++){
    //     for(int j=0 ;j<area->x2;j++)
    //     *(lines[calc_line]++) = color_p->full;
    //     color_p++;
    // }
    //send_line_finish(spi_g);
    // lines[calc_line] = color_p;
    // sending_line=calc_line;
    // calc_line=(calc_line==1)?0:1;
    for (j = 0; j < 6; j++)
    {
        memset(&trans[j], 0, sizeof(spi_transaction_t));
        if ((j & 1) == 0)
        {
            //Even transfers are commands
            trans[j].length = 8;
            trans[j].user = (void *)0;
        }
        else
        {
            //Odd transfers are data
            trans[j].length = 8 * 4;
            trans[j].user = (void *)1;
        }
        trans[j].flags = SPI_TRANS_USE_TXDATA;
    }
    trans[0].tx_data[0] = 0x2A;                                                                       //Column Address Set
    trans[1].tx_data[0] = (area->x1) >> 8;                                                            //Start Col High
    trans[1].tx_data[1] = (area->x1) & 0xff;                                                          //Start Col Low
    trans[1].tx_data[2] = (area->x2) >> 8;                                                            //End Col High
    trans[1].tx_data[3] = (area->x2) & 0xff;                                                          //End Col Low
    trans[2].tx_data[0] = 0x2B;                                                                       //Page address set
    trans[3].tx_data[0] = (area->y1) >> 8;                                                            //Start page high
    trans[3].tx_data[1] = (area->y1) & 0xff;                                                          //start page low
    trans[3].tx_data[2] = (area->y2) >> 8;                                                            //end page high
    trans[3].tx_data[3] = (area->y2) & 0xff;                                                          //end page low
    trans[4].tx_data[0] = 0x2C;                                                                       //memory write
    trans[5].tx_buffer = color_p;                                                                     //lines[sending_line];                                       //finally send the line data
    trans[5].length = ((area->y2) - (area->y1) + 1) * ((area->x2) - (area->x1) + 1) * PARALLEL_LINES; //Data length, in bits
    trans[5].flags = 0;                                                                               //undo SPI_TRANS_USE_TXDATA flag

    //Queue all transactions.
    for (x = 0; x < 6; x++)
    {
        ret = spi_device_queue_trans(spi_g, &trans[x], portMAX_DELAY);
        assert(ret == ESP_OK);
    }
    send_line_finish_lvgl(disp_drv, spi_g);
    //In theory, it's better to initialize trans and data only once and hang on to the initialized
    //variables. We allocate them on the stack, so we need to re-init them each call.

    //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
    //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
    //finish because we may as well spend the time calculating the next line. When that is done, we can call
    //send_line_finish, which will wait for the transfers to be done and check their status.
}

void init_spi_lcd(void)
{
    esp_err_t ret;
    spi_device_handle_t spi;
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PARALLEL_LINES * 240 * 240};
    spi_device_interface_config_t devcfg = {
#ifdef CONFIG_LCD_OVERCLOCK
        .clock_speed_hz = 26 * 1000 * 1000, //Clock out at 26 MHz
#else
        .clock_speed_hz = 26 * 1000 * 1000, //Clock out at 10 MHz
#endif
        .mode = 2,                               //SPI mode 0
        .spics_io_num = -1,                      //CS pin
        .queue_size = 7,                         //We want to be able to queue 7 transactions at a time
        .pre_cb = lcd_spi_pre_transfer_callback, //Specify pre-transfer callback to handle D/C line
    };
    printf("init lcd start!\n");
    //Initialize the SPI bus
    ret = spi_bus_initialize(LCD_HOST, &buscfg, DMA_CHAN);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret = spi_bus_add_device(LCD_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
    spi_g = spi;
    //Initialize the LCD
    lcd_init(spi);
    printf("init lcd finshed!\n");
}
