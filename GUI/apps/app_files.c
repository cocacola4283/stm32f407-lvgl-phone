/**
 ****************************************************************************************************
 * @file        app_files.c
 * @brief       SD 卡文件浏览器（只读）
 *
 * 用 fat_ro 列根目录，显示 文件名 + 大小。可滚动。
 ****************************************************************************************************
 */

#include "app_files.h"
#include "app_shell.h"
#include "./BSP/SD/sd_card.h"
#include "./BSP/SD/fat_ro.h"
#include <stdio.h>
#include <string.h>

#define MAX_SHOW     200       /* 最多显示多少条 */
#define ROW_H        20

static lv_obj_t *g_list;
static lv_obj_t *g_status;
static fat_fs_t  g_fs;
static uint8_t   g_fs_ok;
static uint32_t  g_count;

/* 重新扫描并刷新列表 */
static void files_refresh(void)
{
    uint32_t    i, n;
    fat_entry_t ent;
    char        buf[48];
    uint8_t     sd_ret, mret;

    if (g_list == NULL)
    {
        return;
    }

    lv_obj_clean(g_list);

    sd_ret = sd_card_init();
    if (sd_ret != 0)
    {
        lv_label_set_text(g_status, "SD init FAIL");
        g_fs_ok = 0;
        return;
    }

    mret = fat_mount(&g_fs);
    if (mret != 0)
    {
        sprintf(buf, "MOUNT FAIL (err %u)", (unsigned)mret);
        lv_label_set_text(g_status, buf);
        g_fs_ok = 0;
        return;
    }

    g_fs_ok = 1;
    n = fat_root_count(&g_fs);
    g_count = n;

    sprintf(buf, "FAT%u  %lu entries", (unsigned)g_fs.type, (unsigned long)n);
    lv_label_set_text(g_status, buf);

    if (n > MAX_SHOW)
    {
        n = MAX_SHOW;
    }

    for (i = 0; i < n; i++)
    {
        lv_obj_t *row;
        lv_obj_t *lb;
        uint32_t  kb;

        if (fat_root_entry(&g_fs, i, &ent) != 0)
        {
            break;
        }

        row = lv_obj_create(g_list);
        lv_obj_set_pos(row, 0, (int32_t)(i * ROW_H));
        lv_obj_set_size(row, 300, ROW_H);
        lv_obj_set_style_radius(row, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(row, lv_color_hex(((i & 1) != 0) ? 0x2E3B46 : 0x263238), LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        /* 文件名 */
        lb = lv_label_create(row);
        lv_label_set_text(lb, ent.name);
        lv_obj_set_style_text_color(lb, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_pos(lb, 4, 3);

        /* 目录 / 大小 */
        if ((ent.attr & 0x10) != 0)
        {
            strcpy(buf, "<DIR>");
        }
        else
        {
            kb = (ent.size + 1023U) / 1024U;
            sprintf(buf, "%lu KB", (unsigned long)kb);
        }
        lb = lv_label_create(row);
        lv_label_set_text(lb, buf);
        lv_obj_set_style_text_color(lb, lv_color_hex(0x80CBC4), LV_PART_MAIN);
        lv_obj_align(lb, LV_ALIGN_RIGHT_MID, -4, 0);
    }
}

static void refresh_cb(lv_event_t *e)
{
    (void)e;
    files_refresh();
}

lv_obj_t *app_files_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *l;
    lv_obj_t *btn;
    lv_obj_t *btn_lb;

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x263238), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    l = lv_label_create(scr);
    lv_label_set_text(l, "FILES");
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_pos(l, 8, 6);

    /* BACK */
    app_make_back_btn(scr);

    /* REFRESH */
    btn = lv_button_create(scr);
    lv_obj_set_size(btn, 74, 24);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -64, 2);
    lv_obj_set_style_radius(btn, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x37474F), LV_PART_MAIN);
    lv_obj_add_event_cb(btn, refresh_cb, LV_EVENT_PRESSED, NULL);
    btn_lb = lv_label_create(btn);
    lv_label_set_text(btn_lb, "REFRESH");
    lv_obj_set_style_text_color(btn_lb, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(btn_lb);

    /* 状态行 */
    g_status = lv_label_create(scr);
    lv_label_set_text(g_status, "scanning...");
    lv_obj_set_style_text_color(g_status, lv_color_hex(0xFFD54F), LV_PART_MAIN);
    lv_obj_set_pos(g_status, 8, 28);

    /* 列表容器（可滚动） */
    g_list = lv_obj_create(scr);
    lv_obj_set_pos(g_list, 8, 48);
    lv_obj_set_size(g_list, 304, 186);
    lv_obj_set_style_radius(g_list, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_list, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_list, lv_color_hex(0x263238), LV_PART_MAIN);
    lv_obj_set_scroll_dir(g_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_list, LV_SCROLLBAR_MODE_AUTO);

    files_refresh();

    return scr;
}
