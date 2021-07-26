#ifndef __SDCARD_H
#define __SDCARD_H
#include "stdio.h"
#include "lv_misc/lv_fs.h"

typedef FILE* file_t;

void init_sdcard(void);
lv_fs_res_t fs_open(lv_fs_drv_t * drv, void * file_p, const char * path, lv_fs_mode_t mode);
lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p);
lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br);
lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos);
lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);
#endif