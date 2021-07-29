/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "lv_ex_conf.h"
#include "spi_hard.h"

#include "esp_log.h"

#include "mpu6050.h"
#include "sdcard.h"
#include "qi.h"
/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

//-----------------------------------------------------
float fpitch_x, froll_y; //鼠标指针

#define DISP_BUF_SIZE (LV_HOR_RES_MAX * 40)

/*********************
 *      DEFINES
 *********************/
#define TAG "demo"
#define LV_TICK_PERIOD_MS 1

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);

lv_indev_t *indev_mouse;

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;
// Arc事件回调函数
static void arc_event_handler(lv_obj_t *obj, lv_event_t event)
{
    int angle = 0; // 改变的角度值
    if (event == LV_EVENT_CLICKED)
    {   // 控件上单击事件
        //ESP_LOGI(TAG, "arc_event_handler->Arc Clicked\n");
    }
    else if (event == LV_EVENT_VALUE_CHANGED)
    {                                      // 角度改变事件，手触摸/拖动进度条
        angle = lv_arc_get_angle_end(obj); // 获取事件对象改变的角度
        static char buf[8];
        lv_snprintf(buf, sizeof(buf), "Arc %d", angle);  // 将值变为字符串
        lv_obj_t *label = lv_obj_get_child(obj, NULL);   // 获取事件对象的标签子对象
        lv_label_set_text(label, buf);                   // 设置标签文本
        lv_obj_align(label, obj, LV_ALIGN_CENTER, 0, 0); // 标签文件有改变要重新设置对齐
                                                         //ESP_LOGI(TAG, "arc_event_handler->Value_Changed:%d\n",angle);
    }
}

// 动画回调事件处理函数：回调控件对象，动画的值
static void arc_anim(lv_obj_t *arc, lv_anim_value_t value)
{
    lv_arc_set_end_angle(arc, value); // 设置弧形控件的结束角度
    static char buf[64];
    lv_snprintf(buf, sizeof(buf), "%d", value);      // 字符化数值
    lv_obj_t *label = lv_obj_get_child(arc, NULL);   // 获取弧形控件中的子对象，即label对象
    lv_label_set_text(label, buf);                   // 设置label控件的文本显示
    lv_obj_align(label, arc, LV_ALIGN_CENTER, 0, 0); // 重新对齐label控件到弧形控件中心，因为值长度有变化
}

void demo(void)
{
    lv_obj_t *scr = lv_disp_get_scr_act(NULL); // 返回屏幕的对象指针
    lv_obj_t *arc1 = lv_arc_create(scr, NULL); // 在屏幕上创建一个Arc控件
    //lv_arc_set_start_angle(arc1, 170);				// 设置Arc1控件前景开始角度		Arc 3点钟方向为0点 顺时针起
    //lv_arc_set_end_angle(arc1, 180);					// 设置Arc1控件前景停止角度
    lv_arc_set_angles(arc1, 135, 10);                  // 设置Arc1控件进度开始结束角度 同lv_arc_set_start_angle/lv_arc_set_end_angle
    lv_obj_set_size(arc1, 150, 150);                   // 设置Arc1控件大小
    lv_obj_align(arc1, NULL, LV_ALIGN_CENTER, -75, 0); // 对齐到屏幕中心，X偏移-75，靠左，Y偏移0
    lv_arc_set_adjustable(arc1, true);                 // 设置控件可触摸调整结束角度
    //lv_arc_set_bg_start_angle(arc1, 135);				// 设置Arc1控件背景开始角度		Arc 3点钟方向为0点 顺时针起
    //lv_arc_set_bg_end_angle(arc1, 45);				// 设置Arc1控件背景停止角度
    lv_arc_set_bg_angles(arc1, 135, 45); // 设置Arc1控件背景进度开始结束角度 lv_arc_set_bg_start_angle/lv_arc_set_bg_end_angle

    lv_obj_t *arc1_label = lv_label_create(arc1, NULL);    // 在Arc1控件上创建一个标签
    lv_obj_align(arc1_label, arc1, LV_ALIGN_CENTER, 0, 0); // 标签对齐到Arc1控件中心
    lv_label_set_text(arc1_label, "Arc1");                 // 设置标签文本

    lv_obj_set_event_cb(arc1, arc_event_handler); // 为Arc1控件创建一个事件回调

    // 创建一个动画
    lv_anim_t anim;
    lv_anim_init(&anim);                                      // 初始化动画
    lv_anim_set_time(&anim, 4000);                            // 设置动画时间4秒
    lv_anim_set_playback_time(&anim, 1000);                   // 设置动画回放时间1秒
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE); // 设置动画无限重复

    // 给arc2创建一个风格改变它的颜色
    static lv_style_t style_arc;                                            // 定义一个风格
    lv_style_init(&style_arc);                                              // 初始化风格
    lv_style_set_line_color(&style_arc, LV_STATE_DEFAULT, LV_COLOR_ORANGE); // 弧形背景色
    lv_style_set_line_width(&style_arc, LV_STATE_DEFAULT, 20);              // 弧形背景宽度
    lv_style_set_line_rounded(&style_arc, LV_STATE_DEFAULT, 1);             // 线头是否圆头结束
    lv_style_set_line_opa(&style_arc, LV_STATE_DEFAULT, LV_OPA_30);         // 弧形背景色透明度

    // 创建弧形控件
    lv_obj_t *arc2 = lv_arc_create(scr, NULL);        // 在屏幕上创建一个Arc控件
    lv_arc_set_angles(arc2, 45, 315);                 // 设置Arc1控件进度开始结束角度 同lv_arc_set_start_angle/lv_arc_set_end_angle
    lv_obj_set_size(arc2, 150, 150);                  // 设置Arc1控件大小
    lv_obj_align(arc2, NULL, LV_ALIGN_CENTER, 75, 0); // 对齐到屏幕中心，X偏移75，靠右，Y偏移0
    lv_arc_set_adjustable(arc2, false);               // 设置控件不可触摸调整结束角度
    lv_arc_set_bg_angles(arc2, 45, 315);              // 设置Arc1控件背景进度开始结束角度 lv_arc_set_bg_start_angle/lv_arc_set_bg_end_angle
    lv_arc_set_rotation(arc2, 90);

    lv_obj_t *arc2_label = lv_label_create(arc2, NULL);    // 在Arc1控件上创建一个标签
    lv_obj_align(arc2_label, arc2, LV_ALIGN_CENTER, 0, 0); // 标签对齐到Arc1控件中心
    lv_label_set_text(arc2_label, "Arc2");                 // 设置标签文本

    lv_obj_add_style(arc2, LV_BTN_PART_MAIN, &style_arc);                                          // 为弧形控件添加上面创建的风格效果
    lv_obj_set_style_local_line_color(arc2, LV_ARC_PART_INDIC, LV_STATE_DEFAULT, LV_COLOR_ORANGE); // 设置弧形控件前景色颜色

    // 设置动画参数
    lv_anim_set_var(&anim, arc2);                             // 设置动画对象为arc2
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)arc_anim); // 设置动画回调函数
    lv_anim_set_values(&anim, 45, 315);                       // 设置动画值范围
    lv_anim_start(&anim);                                     // 开始动画
}

bool my_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    data->point.x = fpitch_x;
    data->point.y = fpitch_x;
    data->state = LV_INDEV_STATE_REL;
    return false; /*No buffering now so no more data read*/
}
static void guiTask(void *pvParameter)
{

    (void)pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    init_spi_lcd();

    lv_color_t *buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    /* Use double buffered when not working with monochrome displays */
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    lv_color_t *buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);
#else
    static lv_color_t *buf2 = NULL;
#endif

    static lv_disp_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820 || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306

    /* Actual size in pixels, not bytes. */
    size_in_px *= 8;
#endif

    /* Initialize the working buffer depending on the selected display.
     * NOTE: buf2 == NULL when using monochrome displays. */
    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = send_lines;
    //disp_drv.flush_cb = disp_flush;
    /* When using a monochrome display we need to register the callbacks:
     * - rounder_cb
     * - set_px_cb */
#ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    disp_drv.rounder_cb = disp_driver_rounder;
    disp_drv.set_px_cb = disp_driver_set_px;
#endif

    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register an input device when enabled on the menuconfig */

#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = my_input_read;
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    lv_indev_drv_register(&indev_drv);
#endif
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = my_input_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_mouse = lv_indev_drv_register(&indev_drv);

    
    /*Set cursor. For simplicity set a HOME symbol now.*/
    lv_obj_t *mouse_cursor = lv_img_create(lv_disp_get_scr_act(NULL), NULL);
    lv_img_set_src(mouse_cursor, LV_SYMBOL_EDIT);
    lv_indev_set_cursor(indev_mouse, mouse_cursor);



/*----------------------------------------------------
     * Initialize your storage device and File System
     * -------------------------------------------------*/
    init_sdcard();
    
    /* 这个函数实现在下面，里头是空的，由你自己实现 */

    /*---------------------------------------------------
     * Register the file system interface  in LVGL
     *--------------------------------------------------*/
    
    /* Add a simple drive to open images */
    
    lv_fs_drv_t fs_drv;
	/* 上面这里默认只有一个fs_drv，如果我有多个存储设备的话，我们可以这样 */
	// lv_fs_drv_t fs_drv[FF_VOLUMES]; // FF_VOLUMES 定义在fatfs的ffconf.h文件中

    lv_fs_drv_init(&fs_drv);

    /*Set up fields...*/
    // ↓ 这里的file_t的size会被LVGL使用文件系统时调用进行内存分配，所以你上面一定要改好
    fs_drv.file_size = sizeof(file_t);	
    // ↓ 这里letter是比较重要的一个东西，比如调用SD卡时，我们直接“S:/xxx/xxx”即可
    // 如果我还用SPI Flash的话，这里的letter也可以写成F，避免和S冲突
    fs_drv.letter = 'S';

    //Use drivers for images 读取图片只需要下面几个接口，其他接口没有实现
    /* 下面就是各个接口的注册 */
    fs_drv.open_cb = fs_open;			// 打开
    fs_drv.close_cb = fs_close;			// 关闭
    fs_drv.read_cb = fs_read;			// 读
    //fs_drv.write_cb = fs_write;			// 写
    fs_drv.seek_cb = fs_seek;			// 寻址
    fs_drv.tell_cb = fs_tell;			// 获取当前文件偏移地址

	/* 注册设备 */
    lv_fs_drv_register(&fs_drv);

    
    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    /* Create the demo application */
    create_demo_application();

    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    /* A task should NEVER return */
    free(buf1);
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME 
    free(buf2);
#endif
    vTaskDelete(NULL);
}

static void create_demo_application(void)
{
    //demo();
    //lv_obj_t * icon = lv_img_create(lv_scr_act(), NULL);
//lvfl gif 使用
     //lvgl gif lv_gif_create_from_file函数中才开始创建lv_img_create 所以直接传进lv_scr_act();
//图片数组使用。
    // LV_IMG_DECLARE(cloud);
    // lv_img_set_src(icon, &cloud);


    //lv_obj_t * img = lv_gif_create_from_file(lv_scr_act(), "S/pic/example.gif");
    /*From file*/
    //lv_img_set_src(icon, "S:/pic/4.bin");
}

static void lv_tick_task(void *arg)
{
    (void)arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}

/**
 * Brief:
 * This test code shows how to configure gpio and how to use gpio interrupt.
 *
 * GPIO status:
 * GPIO18: output
 * GPIO19: output
 * GPIO4:  input, pulled up, interrupt from rising edge and falling edge
 * GPIO5:  input, pulled up, interrupt from rising edge.
 *
 * Test:
 * Connect GPIO18 with GPIO4
 * Connect GPIO19 with GPIO5
 * Generate pulses on GPIO18/19, that triggers interrupt on GPIO4/5
 *
 */

static void mpu6050_task(void *pvParameters)
{
    MPU6050(GPIO_NUM_33, GPIO_NUM_32, I2C_NUM_0);

    if (!init())
    {
        ESP_LOGE("mpu6050", "init failed!");
        vTaskDelete(0);
    }
    ESP_LOGI("mpu6050", "init success!");

    float ax, ay, az, gx, gy, gz;
    float pitch, roll;
    float fpitch, froll;

    uint32_t lasttime = 0;
    int count = 0;

    while (1)
    {
        ax = -getAccX();
        ay = -getAccY();
        az = -getAccZ();
        gx = getGyroX();
        gy = getGyroY();
        gz = getGyroZ();
        pitch = atan(ax / az) * 57.2958;
        roll = atan(ay / az) * 57.2958;
        fpitch = filter(pitch, gy, 0.005);
        froll = r_filter(roll, -gx, 0.005);
        count++;
        // if (esp_log_timestamp() / 1000 != lasttime)
        // {
        //     lasttime = esp_log_timestamp() / 1000;
        //     ESP_LOGI("mpu6050", "Samples: %d", count);
        //     count = 0;
            fpitch_x = fpitch;
            froll_y = froll;
        //     ESP_LOGI("mpu6050", "Acc: ( %.3f, %.3f, %.3f)", ax, ay, az);
        //     ESP_LOGI("mpu6050", "Gyro: ( %.3f, %.3f, %.3f)", gx, gy, gz);
        //     ESP_LOGI("mpu6050", "Pitch: %.3f", pitch);
        //     ESP_LOGI("mpu6050", "Roll: %.3f", roll);
            // ESP_LOGI("mpu6050", "FPitch: %.3f", fpitch);
            // ESP_LOGI("mpu6050", "FRoll: %.3f", froll);
        // }
    }
}

void app_main(void)
{
    qi();
    xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 2, NULL, 0, NULL, 1);

    //xTaskCreatePinnedToCore(&mpu6050_task, "mpu6050_task", 2048, NULL, 5, NULL, 0);
    while (1)
    {
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}
