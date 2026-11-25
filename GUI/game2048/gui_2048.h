/**
 ****************************************************************************************************
 * @file        gui_2048.h
 * @brief       带滑动动画的 2048（自己实现，不依赖 GUI Guider 的 buttonmatrix）
 ****************************************************************************************************
 */

#ifndef __GUI_2048_H
#define __GUI_2048_H

#include "lvgl.h"

/* 移动方向 */
typedef enum
{
    G2048_LEFT = 0,
    G2048_RIGHT,
    G2048_UP,
    G2048_DOWN
} g2048_dir_t;

/**
 * @brief  在当前活动屏幕上创建 2048 界面（含棋盘、分数、方向键、New Game）
 */
lv_obj_t *app_2048_create(void);

/**
 * @brief  执行一次移动（带滑动动画）
 */
void game2048_move(g2048_dir_t dir);

/**
 * @brief  重新开局
 */
void game2048_new(void);

#endif
