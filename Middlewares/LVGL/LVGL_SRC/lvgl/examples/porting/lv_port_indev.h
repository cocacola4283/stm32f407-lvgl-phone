/**
 * @file lv_port_indev.h
 * LVGL 9.5 输入设备移植头文件（触摸屏）
 */

#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * 初始化 LVGL 输入设备（触摸屏）
 * @note  必须在 tp_dev.init() 之后调用
 */
/* ---- 触摸调试用（给界面显示） ---- */
extern volatile int32_t  g_tp_x;        /* 最近一次坐标 */
extern volatile int32_t  g_tp_y;
extern volatile uint8_t  g_tp_down;     /* 1 = 当前按下 */
extern volatile uint16_t g_tp_sta;      /* tp_dev.sta 原始值 */
extern volatile uint32_t g_tp_cnt;      /* 累计按下次数 */

void lv_port_indev_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_PORT_INDEV_H*/
