/**
 ****************************************************************************************************
 * @file        photo_view.h
 * @brief       SD 卡 JPEG 图片全屏显示（流式逐带解码，RAM 只要 ~16 KB）
 *
 * 原理：TJpgDec 按 MCU 块（16 行）回调输出像素，我们把它拼成一条"带"，
 *       一带满了就立刻写进 LCD，然后继续解码下一带。
 *       所以**不需要** 320x240x2 = 150 KB 的整屏缓冲。
 ****************************************************************************************************
 */

#ifndef __PHOTO_VIEW_H
#define __PHOTO_VIEW_H

#include "./SYSTEM/sys/sys.h"
#include "./BSP/SD/fat_ro.h"

/* 返回值 */
#define PHOTO_OK         0
#define PHOTO_ERR_OPEN   1
#define PHOTO_ERR_PREP   2
#define PHOTO_ERR_DEC    3
#define PHOTO_ERR_SIZE   4

/**
 * @brief  把 SD 卡上的一张 JPEG 全屏居中显示（自动按 1/2、1/4、1/8 缩放以适配屏幕）
 * @param  fs    : 已挂载的文件系统
 * @param  name  : 文件名（8.3 短名）
 * @retval 0=成功，其它=错误码
 */
uint8_t photo_show(fat_fs_t *fs, const char *name);

/* 诊断信息（给屏幕上看） */
extern int16_t  photo_dbg_result;    /* jd_decomp 的返回码，0=OK */
extern uint16_t photo_dbg_bands;     /* 实际写了多少条带 */
extern uint16_t photo_dbg_rows;      /* 实际写了多少行 */

/* 上一次成功显示的实际尺寸（缩放后） */
uint16_t photo_width(void);
uint16_t photo_height(void);

#endif
