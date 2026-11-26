/**
 ****************************************************************************************************
 * @file        app_photos.c
 * @brief       SD 卡图片浏览器（裸屏模式）
 *
 * 设计要点：
 *   1. 不用 LVGL 的 lv_image 显示大图 —— 它会把整图解码进 RAM（320x240x2 = 150KB，装不下）。
 *      这里用 tjpgd 流式解码（约 16KB），每解出一条带就直接写 LCD。
 *   2. 本函数**必须由主循环调用**（app_shell_poll），不能从 LVGL 事件回调里调，
 *      否则 LVGL 会把没画完的内容覆盖到图片上。
 *   3. 底部按钮条画在哪就判在哪，避免"看得见点不着"。
 ****************************************************************************************************
 */

#include "app_photos.h"
#include "app_shell.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/SD/sd_card.h"
#include "./BSP/SD/fat_ro.h"
#include "./BSP/SD/photo_view.h"
#include "./SYSTEM/delay/delay.h"
#include <stdio.h>
#include <string.h>

#define MAX_PHOTOS      24
#define NAME_LEN        16
#define BAR_H           24          /* 底部按钮条高度 */

/* 底部按钮条四个区域起点（画和判用同一组常量） */
#define ZONE_PREV_X     0
#define ZONE_INFO_X     80
#define ZONE_NEXT_X     160
#define ZONE_EXIT_X     240

typedef struct
{
    fat_fs_t  fs;
    char      names[MAX_PHOTOS][NAME_LEN];
    uint16_t  count;
    int16_t   cur;
} photo_ctx_t;

static photo_ctx_t g_ctx;

/* 文件名是不是 .jpg（不区分大小写） */
static uint8_t is_jpg(const char *name)
{
    uint32_t n = (uint32_t)strlen(name);

    if (n < 5U)
    {
        return 0;
    }
    if (name[n - 4] != '.')
    {
        return 0;
    }
    if ((name[n - 3] != 'J') && (name[n - 3] != 'j'))
    {
        return 0;
    }
    if ((name[n - 2] != 'P') && (name[n - 2] != 'p'))
    {
        return 0;
    }
    if ((name[n - 1] != 'G') && (name[n - 1] != 'g'))
    {
        return 0;
    }
    return 1;
}

/* 扫根目录收集所有 jpg */
static void scan_photos(void)
{
    uint32_t    i, n;
    fat_entry_t ent;

    g_ctx.count = 0;
    g_ctx.cur   = -1;

    if (sd_card_init() != 0)
    {
        return;
    }
    if (fat_mount(&g_ctx.fs) != 0)
    {
        return;
    }

    n = fat_root_count(&g_ctx.fs);
    for (i = 0; (i < n) && (g_ctx.count < MAX_PHOTOS); i++)
    {
        if (fat_root_entry(&g_ctx.fs, i, &ent) != 0)
        {
            break;
        }
        if ((ent.attr & 0x10U) != 0U)
        {
            continue;
        }
        if (!is_jpg(ent.name))
        {
            continue;
        }
        strncpy(g_ctx.names[g_ctx.count], ent.name, NAME_LEN - 1U);
        g_ctx.names[g_ctx.count][NAME_LEN - 1U] = '\0';
        g_ctx.count++;
    }
}

/* 底部按钮条 */
static void draw_bar(void)
{
    char     buf[24];
    uint16_t y0 = (uint16_t)(lcddev.height - BAR_H);

    lcd_fill(0, y0, (uint16_t)(lcddev.width - 1), (uint16_t)(lcddev.height - 1), 0x0000);

    lcd_show_string(ZONE_PREV_X + 26, (uint16_t)(y0 + 4), 40, 16, 16, "<", 0xFFFF);

    sprintf(buf, "%d/%d", (int)(g_ctx.cur + 1), (int)g_ctx.count);
    lcd_show_string(ZONE_INFO_X + 20, (uint16_t)(y0 + 4), 70, 16, 16, buf, 0x07E0);

    lcd_show_string(ZONE_NEXT_X + 26, (uint16_t)(y0 + 4), 40, 16, 16, ">", 0xFFFF);

    lcd_show_string(ZONE_EXIT_X + 14, (uint16_t)(y0 + 4), 70, 16, 16, "EXIT", 0xF800);
}

/* 诊断行：解码返回码 / 带数 / 行数 / 目标尺寸 */
static void draw_debug(void)
{
    char     buf[48];
    uint16_t y0 = (uint16_t)(lcddev.height - BAR_H - 16);

    lcd_fill(0, y0, (uint16_t)(lcddev.width - 1), (uint16_t)(y0 + 15), 0x0000);
    sprintf(buf, "R%d B%d L%d %dx%d", (int)photo_dbg_result, (int)photo_dbg_bands,
            (int)photo_dbg_rows, (int)photo_width(), (int)photo_height());
    lcd_show_string(2, y0, 300, 16, 16, buf, 0xFFE0);
}

/* 显示当前这张 */
static void show_current(void)
{
    if ((g_ctx.cur < 0) || (g_ctx.cur >= (int16_t)g_ctx.count))
    {
        return;
    }

    if (photo_show(&g_ctx.fs, g_ctx.names[g_ctx.cur]) != PHOTO_OK)
    {
        lcd_clear(0x0000);
        lcd_show_string(60, 100, 200, 16, 16, "JPEG DECODE FAIL", 0xF800);
    }
    draw_bar();
    draw_debug();
}

/* 等一次点击（松手才算），1=有效点击，0=超时 */
static uint8_t wait_tap(uint16_t *px, uint16_t *py)
{
    uint32_t t = 0;

    while (1)
    {
        tp_dev.scan(0);
        if ((tp_dev.sta & TP_PRES_DOWN) != 0U)
        {
            *px = tp_dev.x[0];
            *py = tp_dev.y[0];

            while (1)                       /* 等松手，防连击 */
            {
                delay_ms(20);
                tp_dev.scan(0);
                if ((tp_dev.sta & TP_PRES_DOWN) == 0U)
                {
                    break;
                }
            }
            return 1;
        }
        delay_ms(20);
        t++;
        if (t > 15000U)                     /* 约 5 分钟无操作自动退出 */
        {
            return 0;
        }
    }
}

void app_photos_run(void)
{
    uint16_t x = 0, y = 0;
    uint8_t  running = 1;
    uint16_t w = lcddev.width;
    uint16_t h = lcddev.height;

    g_back_color = 0x0000;                  /* 文字格子背景用黑色 */

    lcd_clear(0x0000);
    lcd_show_string(70, 110, 200, 16, 16, "SCANNING SD...", 0xFFFF);

    scan_photos();

    if (g_ctx.count == 0U)
    {
        lcd_clear(0x0000);
        lcd_show_string(40, 90, 260, 16, 16, "NO *.JPG ON SD", 0xF800);
        lcd_show_string(40, 120, 260, 16, 16, "TAP TO GO BACK", 0xFFFF);
        (void)wait_tap(&x, &y);
        return;
    }

    g_ctx.cur = 0;
    show_current();

    while (running)
    {
        if (!wait_tap(&x, &y))
        {
            break;
        }

        if (y >= (uint16_t)(h - BAR_H))
        {
            /* 底部按钮条：按 x 分区，和画出来的位置一一对应 */
            if (x < ZONE_INFO_X)                      /* "<" 上一张 */
            {
                g_ctx.cur = (int16_t)((g_ctx.cur + (int16_t)g_ctx.count - 1) % (int16_t)g_ctx.count);
                show_current();
            }
            else if (x < ZONE_NEXT_X)                 /* 页码，无动作 */
            {
            }
            else if (x < ZONE_EXIT_X)                 /* ">" 下一张 */
            {
                g_ctx.cur = (int16_t)((g_ctx.cur + 1) % (int16_t)g_ctx.count);
                show_current();
            }
            else                                      /* "EXIT" 退出 */
            {
                running = 0;
            }
        }
        else
        {
            /* 点在图区：左右 1/3 翻页 */
            if (x < (uint16_t)(w / 3U))
            {
                g_ctx.cur = (int16_t)((g_ctx.cur + (int16_t)g_ctx.count - 1) % (int16_t)g_ctx.count);
                show_current();
            }
            else if (x > (uint16_t)(w * 2U / 3U))
            {
                g_ctx.cur = (int16_t)((g_ctx.cur + 1) % (int16_t)g_ctx.count);
                show_current();
            }
        }
    }
}
