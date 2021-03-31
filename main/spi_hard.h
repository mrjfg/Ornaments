#ifndef SPI_HARD_H
#define SPI_HARD_H
#include "driver/spi_master.h"
#include "lv_ex_conf.h"
#include "lvgl.h"

void init_spi_lcd(void);
void send_lines(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);






#endif