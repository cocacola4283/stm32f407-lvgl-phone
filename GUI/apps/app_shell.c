/**
 ****************************************************************************************************
 * @file        app_shell.c
 * @brief       主菜单（iOS 风格圆角图标，2 行 x 3 列）+ app 切换
 ****************************************************************************************************
 */

#include "app_shell.h"
#include "gui_2048.h"
#include "app_files.h"
#include "./SYSTEM/sys/sys.h"
#include <stdint.h>

#define APP_2048        1
#define APP_FILES       2
#define APP_TODO        3
#define APP_SETTINGS    4
#define APP_MUSIC       5
#define APP_ABOUT       6

/* iOS 风格图标尺寸 */
#define ICON_W          84
#define ICON_R          19          /* 圆角 ≈ 22% */
#define COL_X0          12
#define COL_DX          106
#define ROW_Y0          34
#define ROW_DY          104

static lv_obj_t *g_home          = NULL;
static lv_obj_t *g_scr_2048      = NULL;
static lv_obj_t *g_scr_files     = NULL;
static lv_obj_t *g_scr_todo      = NULL;
static lv_obj_t *g_scr_settings  = NULL;
static lv_obj_t *g_scr_music     = NULL;
static lv_obj_t *g_scr_about     = NULL;

/* ---------------- 回调包装 ---------------- */
static void back_cb(lv_event_t *e)
{
    (void)e;
    app_shell_home();
}

static void rst_cb(lv_event_t *e)
{
    (void)e;
    lv_refr_now(NULL);
    NVIC_SystemReset();
}

/* ---------------- 屏幕切换 ---------------- */
void app_shell_open(lv_obj_t *scr)
{
    if (scr == NULL)
    {
        return;
    }
    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false);   /* 有 DMA 流水线后可以跑动了 */
}

void app_shell_home(void)
{
    if (g_home == NULL)
    {
        return;
    }
    lv_screen_load_anim(g_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 180, 0, false);
}

/* ---------------- 通用小部件 ---------------- */
lv_obj_t *app_make_back_btn(lv_obj_t *scr)
{
    lv_obj_t *b = lv_button_create(scr);
    lv_obj_t *l;

    lv_obj_set_size(b, 62, 28);
    lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -4, 2);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x5C6BC0), LV_PART_MAIN);
    lv_obj_add_event_cb(b, back_cb, LV_EVENT_PRESSED, NULL);

    l = lv_label_create(b);
    lv_label_set_text(l, "BACK");
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(l);

    return b;
}

lv_obj_t *app_make_placeholder(const char *title)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *l;

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x263238), LV_PART_MAIN);

    l = lv_label_create(scr);
    lv_label_set_text(l, title);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, -12);

    l = lv_label_create(scr);
    lv_label_set_text(l, "Coming soon...");
    lv_obj_set_style_text_color(l, lv_color_hex(0x90A4AE), LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 16);

    app_make_back_btn(scr);
    return scr;
}

/* ---------------- 图标点击 ---------------- */
static void icon_pressed(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    lv_obj_set_style_bg_opa(b, 60, LV_PART_MAIN);
}

static void icon_cb(lv_event_t *e)
{
    uint32_t id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *b = lv_event_get_target(e);

    lv_obj_set_style_bg_opa(b, 0, LV_PART_MAIN);    /* 松手/跳转前恢复 */

    switch (id)
    {
        case APP_2048:
            if (g_scr_2048 == NULL) { g_scr_2048 = app_2048_create(); }
            app_shell_open(g_scr_2048);
            break;

        case APP_FILES:
            if (g_scr_files == NULL) { g_scr_files = app_files_create(); }
            app_shell_open(g_scr_files);
            break;

        case APP_TODO:
            if (g_scr_todo == NULL) { g_scr_todo = app_make_placeholder("TO DO"); }
            app_shell_open(g_scr_todo);
            break;

        case APP_SETTINGS:
            if (g_scr_settings == NULL) { g_scr_settings = app_make_placeholder("SETTINGS"); }
            app_shell_open(g_scr_settings);
            break;

        case APP_MUSIC:
            if (g_scr_music == NULL) { g_scr_music = app_make_placeholder("MUSIC"); }
            app_shell_open(g_scr_music);
            break;

        case APP_ABOUT:
            if (g_scr_about == NULL) { g_scr_about = app_make_placeholder("ABOUT"); }
            app_shell_open(g_scr_about);
            break;

        default:
            break;
    }
}

/* 造一个 iOS 风格图标：圆角方块 + 上下渐变 + 投影 + 白色大符号 + 下方名称 */
static void make_icon(lv_obj_t *parent, int32_t x, int32_t y,
                      const char *sym, const char *name, uint32_t id,
                      uint32_t c_top, uint32_t c_bottom)
{
    lv_obj_t *b;
    lv_obj_t *q;
    lv_obj_t *l;

    /* 可点击区域（含下方文字） */
    b = lv_button_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, ICON_W, ICON_W + 20);
    lv_obj_set_style_bg_opa(b, 0, LV_PART_MAIN);          /* 自身透明，视觉由内层负责 */
    lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, icon_pressed, LV_EVENT_PRESSED, (void *)(uintptr_t)id);
    lv_obj_add_event_cb(b, icon_cb, LV_EVENT_RELEASED, (void *)(uintptr_t)id);

    /* 内层：圆角方块 + 渐变 + 投影 */
    q = lv_obj_create(b);
    lv_obj_set_pos(q, 0, 0);
    lv_obj_set_size(q, ICON_W, ICON_W);
    lv_obj_set_style_radius(q, ICON_R, LV_PART_MAIN);
    lv_obj_set_style_border_width(q, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(q, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(q, lv_color_hex(c_top), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(q, lv_color_hex(c_bottom), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(q, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(q, 7, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(q, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(q, 60, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(q, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(q, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(q, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_opa(q, 40, LV_PART_MAIN);
    lv_obj_clear_flag(q, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(q, LV_OBJ_FLAG_CLICKABLE);      /* 点击交给外层按钮 */

    l = lv_label_create(q);
    lv_label_set_text(l, sym);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(l);

    /* 下方名称 */
    l = lv_label_create(b);
    lv_label_set_text(l, name);
    lv_obj_set_style_text_color(l, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_BOTTOM_MID, 0, 0);
}

/* ---------------- 桌面 ---------------- */
static void build_home(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *l;
    lv_obj_t *rst;
    lv_obj_t *r;

    g_home = scr;

    /* iOS 风格深色壁纸：上深下浅 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B2A38), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x0E1620), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 状态栏：时间 + 标题 + RST */
    l = lv_label_create(scr);
    lv_label_set_text(l, "9:41");
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_pos(l, 10, 6);

    l = lv_label_create(scr);
    lv_label_set_text(l, "MY PHONE");
    lv_obj_set_style_text_color(l, lv_color_hex(0xB0BEC5), LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 6);

    rst = lv_button_create(scr);
    lv_obj_set_size(rst, 52, 24);
    lv_obj_align(rst, LV_ALIGN_TOP_RIGHT, -6, 4);
    lv_obj_set_style_radius(rst, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(rst, lv_color_hex(0xB71C1C), LV_PART_MAIN);
    lv_obj_add_event_cb(rst, rst_cb, LV_EVENT_PRESSED, NULL);
    r = lv_label_create(rst);
    lv_label_set_text(r, "RST");
    lv_obj_set_style_text_color(r, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(r);

    /* 2 行 x 3 列 */
    make_icon(scr, COL_X0,            ROW_Y0,          LV_SYMBOL_VIDEO,     "2048",  APP_2048,      0xFFB74D, 0xF57C00);
    make_icon(scr, COL_X0 + COL_DX,   ROW_Y0,          LV_SYMBOL_DIRECTORY, "FILES", APP_FILES,     0x4FC3F7, 0x0288D1);
    make_icon(scr, COL_X0 + COL_DX*2, ROW_Y0,          LV_SYMBOL_LIST,      "TO DO", APP_TODO,      0x81C784, 0x388E3C);
    make_icon(scr, COL_X0,            ROW_Y0 + ROW_DY, LV_SYMBOL_SETTINGS,  "SETUP", APP_SETTINGS,  0xB0BEC5, 0x607D8B);
    make_icon(scr, COL_X0 + COL_DX,   ROW_Y0 + ROW_DY, LV_SYMBOL_AUDIO,     "MUSIC", APP_MUSIC,     0xF06292, 0xC2185B);
    make_icon(scr, COL_X0 + COL_DX*2, ROW_Y0 + ROW_DY, LV_SYMBOL_FILE,      "ABOUT", APP_ABOUT,     0x9575CD, 0x5E35B1);
}

void app_shell_start(void)
{
    build_home();
    lv_screen_load_anim(g_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 180, 0, false);
}
