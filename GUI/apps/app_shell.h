/**
 ****************************************************************************************************
 * @file        app_shell.h
 * @brief       简易"手机桌面"：主菜单 + app 切换
 ****************************************************************************************************
 */

#ifndef __APP_SHELL_H
#define __APP_SHELL_H

#include "lvgl.h"

/* 打开某个 app 的屏幕（左滑进入） */
void app_shell_open(lv_obj_t *scr);

/* 回到主菜单（右滑返回） */
void app_shell_home(void);

/* 在指定屏幕上放一个 BACK 按钮（回到主菜单），返回按钮对象 */
lv_obj_t *app_make_back_btn(lv_obj_t *scr);

/* 生成"功能开发中"占位屏幕 */
lv_obj_t *app_make_placeholder(const char *title);

/* 开机入口：建桌面并显示 */
void app_shell_start(void);

#endif
