/**
 * @file lv_port_disp.h
 * LVGL 9.5 显示移植头文件（STM32F407 + FSMC 并口 8080 TFT）
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * 初始化 LVGL 显示设备
 * @note  必须在 lcd_init() 之后调用（要用到 lcddev.width / lcddev.height）
 */
void lv_port_disp_init(void);
void disp_dma_irq_handler(void);
/* 等到当前这块像素被 DMA 搬完（独占屏幕前必须调用，否则会把画面写乱） */
void lv_port_disp_wait_idle(void);
/* 借一块显示缓冲当临时内存（只在 LVGL 暂停时用，比如看图片）
 * 返回缓冲地址，*size 返回字节数；LVGL 在跑的时候绝对不能用！ */
void *lv_port_disp_get_scratch(uint32_t *size);   /* 由 stm32f4xx_it.c 的 DMA2_Stream0_IRQHandler 调用 */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_PORT_DISP_H*/
