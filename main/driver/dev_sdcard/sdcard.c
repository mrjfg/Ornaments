/* SD card and FAT filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "sdcard.h"
#ifdef CONFIG_IDF_TARGET_ESP32
#include "driver/sdmmc_host.h"
#endif

static const char *TAG = "example";
#define DFS_PATH_MAX 80
#define O_RDONLY "r"
#define MOUNT_POINT "/sdcard"
#include "lvgl.h"
// This example can use SDMMC and SPI peripherals to communicate with SD card.
// By default, SDMMC peripheral is used.
// To enable SPI mode, uncomment the following line:

// #define USE_SPI_MODE

// DMA channel to be used by the SPI peripheral
#ifndef SPI_DMA_CHAN
#define SPI_DMA_CHAN 1
#endif //SPI_DMA_CHAN

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.

//#ifdef USE_SPI_MODE
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define PIN_NUM_MISO 26
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK 14
#define PIN_NUM_CS 15
//#endif //USE_SPI_MODE

void init_sdcard(void)
{
    esp_err_t ret;
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {

        .format_if_mount_failed = false,

        .max_files = 5,
        .allocation_unit_size = 64 * 1024};

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 80000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files.
    // First create a file.
    ESP_LOGI(TAG, "Opening file");
    FILE *f = fopen(MOUNT_POINT "/hello.txt", "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello %s!\n", card->cid.name);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    // Check if destination file exists before renaming
    struct stat st;
    if (stat(MOUNT_POINT "/foo.txt", &st) == 0)
    {
        // Delete it if it exists
        unlink(MOUNT_POINT "/foo.txt");
    }

    // Rename original file
    ESP_LOGI(TAG, "Renaming file");
    if (rename(MOUNT_POINT "/hello.txt", MOUNT_POINT "/foo.txt") != 0)
    {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }
// ---------------------------------------------------test code -------------------------------------------------------------------
    //     // Open renamed file for reading
    //     ESP_LOGI(TAG, "Reading file");
    //     f = fopen(MOUNT_POINT"/foo.txt", "r");
    //     if (f == NULL) {
    //         ESP_LOGE(TAG, "Failed to open file for reading");
    //         return;
    //     }
    //     char line[64];
    //     fgets(line, sizeof(line), f);
    //     fclose(f);
    //     // strip newline
    //     char* pos = strchr(line, '\n');
    //     if (pos) {
    //         *pos = '\0';
    //     }
    //     ESP_LOGI(TAG, "Read from file: '%s'", line);

    //     // All done, unmount partition and disable SDMMC or SPI peripheral
    //     esp_vfs_fat_sdcard_unmount(mount_point, card);
    //     ESP_LOGI(TAG, "Card unmounted");
    // //#ifdef USE_SPI_MODE
    //     //deinitialize the bus after all devices are removed
    //     spi_bus_free(host.slot);
    // //#endif
//-------------------------------------------------------------------------------------------------------------------------------
}

lv_fs_res_t fs_open(lv_fs_drv_t *drv, void *file_p, const char *path, lv_fs_mode_t mode)
{
    lv_fs_res_t res = LV_FS_RES_NOT_IMP;

    char posix_path[DFS_PATH_MAX] = "/sdcard/";
    const char *lvgl_dir_path = path;
    strncat(posix_path, lvgl_dir_path, DFS_PATH_MAX - 6);
    //printf("%s\n",posix_path);
    FILE *fd;
    if (mode == LV_FS_MODE_RD)
    {
        /*Open a file for read*/
        if ((fd = fopen(posix_path, O_RDONLY)) > 0)
        {
            *(file_t *)file_p = fd;
            res = LV_FS_RES_OK; // open success
        }
        else
        {
            res = LV_FS_RES_NOT_EX; // open fail
        }
    }

    return res;
}

lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    lv_fs_res_t res = LV_FS_RES_UNKNOWN;

    FILE *fd = *(file_t *)file_p;
    if (fclose(fd) == 0)
        res = LV_FS_RES_OK; // close success

    return res;
}

lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    lv_fs_res_t res = LV_FS_RES_NOT_IMP;

    FILE *fd = *(file_t *)file_p;
    int read_bytes = fread(buf, btr, 1, fd);
    //printf("readbytes:%d\n",read_bytes);
    if (read_bytes >= 0)
    {
        *br = read_bytes * btr;
        res = LV_FS_RES_OK;
    }

    return res;
}

lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos)
{
    lv_fs_res_t res = LV_FS_RES_NOT_IMP;

    FILE *fd = *(file_t *)file_p;
    if (fseek(fd, pos, SEEK_SET) >= 0)
        res = LV_FS_RES_OK;

    return res;
}

lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    lv_fs_res_t res = LV_FS_RES_NOT_IMP;

    FILE *fd = *(file_t *)file_p;
    off_t pos = fseek(fd, 0, SEEK_CUR);
    if (pos >= 0)
    {
        *pos_p = pos;
        res = LV_FS_RES_OK;
    }

    return res;
}

void test_wr(void)
{
    lv_fs_file_t f;
    lv_fs_res_t res;
    res = lv_fs_open(&f, "S:/HOO.txt", LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK)
        printf("open error\n");

    uint32_t read_num;
    uint8_t buf[8];
    res = lv_fs_read(&f, buf, 8, &read_num);
    if (res != LV_FS_RES_OK || read_num != 8)
        printf("read error\n");

    printf("res_num:%d\n", read_num);
    printf("rs:%s\n", buf);
}