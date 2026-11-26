/**
 ****************************************************************************************************
 * @file        photo_view.c
 * @brief       JPEG 流式解码 + 逐带写屏
 *
 * 内存占用：
 *   - TJpgDec 工作池   6 KB
 *   - 行带缓冲         320 x 16 x 2 = 10 KB
 *   - 合计            ~16 KB（用静态数组，不进栈）
 ****************************************************************************************************
 */

#include "photo_view.h"
#include "./BSP/LCD/lcd.h"
#include "tjpgd.h"
#include "lv_port_disp.h"
#include <string.h>

#define BAND_LINES      16
#define MAX_IMG_W       320

static uint16_t *g_band;      /* 行带缓冲（借用显示缓冲，不额外占 RAM） */
static void     *g_pool;      /* tjpgd 工作池（同上） */
static uint16_t g_img_w;                          /* 缩放后的宽 */
static uint16_t g_img_h;                          /* 缩放后的高 */
static uint16_t g_dst_x;
static uint16_t g_dst_y;
static int32_t  g_band_top = -1;
static uint8_t  g_band_rows;
int16_t  photo_dbg_result = -1;
uint16_t photo_dbg_bands  = 0;
uint16_t photo_dbg_rows   = 0;

/* TJpgDec 的输入回调：从 fat 文件句柄读数据 */
static size_t jpg_in(JDEC *jd, uint8_t *buff, size_t nbyte)
{
    fat_file_t *f = (fat_file_t *)jd->device;

    if (buff != NULL)
    {
        return (size_t)fat_read(f, buff, (uint32_t)nbyte);
    }

    /* buff == NULL 表示跳过 nbyte 字节 */
    if (fat_seek(f, f->pos + (uint32_t)nbyte) != 0)
    {
        return 0;
    }
    return nbyte;
}

/* 把当前这条带写进 LCD */
static void band_flush(void)
{
    uint32_t n;
    uint16_t *p;

    if ((g_band_top < 0) || (g_band_rows == 0))
    {
        return;
    }

    lcd_set_window(g_dst_x,
                   (uint16_t)(g_dst_y + (uint16_t)g_band_top),
                   g_img_w,
                   g_band_rows);
    lcd_write_ram_prepare();

    photo_dbg_bands++;
    photo_dbg_rows = (uint16_t)(photo_dbg_rows + g_band_rows);

    n = (uint32_t)g_img_w * g_band_rows;
    p = g_band;
    while (n--)
    {
        LCD->LCD_RAM = *p++;
    }
}

/* 解码输出回调：每次给一个 MCU 块（若干行） */
static int jpg_out(JDEC *jd, void *bitmap, JRECT *rect)
{
    uint16_t *src = (uint16_t *)bitmap;
    uint32_t  rw  = (uint32_t)rect->right - rect->left + 1U;
    uint32_t  y, i;

    (void)jd;

    if (g_band_top < 0)
    {
        g_band_top = rect->top;          /* 第一条带 */
    }
    else if (rect->top != g_band_top)
    {
        band_flush();                    /* 换带了：上一带写屏 */
        g_band_top = rect->top;
    }

    for (y = rect->top; y <= rect->bottom; y++)
    {
        uint16_t *dst = &g_band[(y - (uint32_t)g_band_top) * g_img_w + rect->left];

        for (i = 0; i < rw; i++)
        {
            dst[i] = *src++;
        }
    }

    g_band_rows = (uint8_t)(rect->bottom - (uint16_t)g_band_top + 1U);
    return 1;                            /* 继续解码 */
}

uint8_t photo_show(fat_fs_t *fs, const char *name)
{
    static JDEC jd;
    void      *scratch;
    uint32_t   scratch_sz;
    fat_file_t f;
    JRESULT    r;
    uint8_t    scale = 0;          /* jd_decomp 的参数是"指数"：0=1/1, 1=1/2, 2=1/4, 3=1/8 */

    /* 借 LVGL 的显示缓冲：前 10KB 当行带，后面 6KB 当解码工作池
     * （看图时 LVGL 是暂停的，那块缓冲正好闲着 —— 省掉 16KB 静态 RAM） */
    scratch = lv_port_disp_get_scratch(&scratch_sz);
    if ((scratch == NULL) || (scratch_sz < (MAX_IMG_W * BAND_LINES * 2U + 6144U)))
    {
        return PHOTO_ERR_SIZE;
    }
    g_band = (uint16_t *)scratch;
    g_pool = (void *)((uint8_t *)scratch + MAX_IMG_W * BAND_LINES * 2U);

    if (fat_open(fs, name, &f) != 0)
    {
        return PHOTO_ERR_OPEN;
    }

    r = jd_prepare(&jd, jpg_in, g_pool, 6144U, &f);
    if (r != JDR_OK)
    {
        return PHOTO_ERR_PREP;
    }

    /* 挑一个尽量大又能放进屏幕的缩放档位：0=1/1, 1=1/2, 2=1/4, 3=1/8 */
    while ((scale < 3U) &&
           (((uint32_t)jd.width >> scale) > lcddev.width ||
            ((uint32_t)jd.height >> scale) > lcddev.height))
    {
        scale++;
    }

    g_img_w = (uint16_t)((uint32_t)jd.width >> scale);
    g_img_h = (uint16_t)((uint32_t)jd.height >> scale);

    if ((g_img_w == 0) || (g_img_h == 0) || (g_img_w > MAX_IMG_W) || (g_img_h > lcddev.height))
    {
        return PHOTO_ERR_SIZE;
    }

    g_dst_x    = (uint16_t)((lcddev.width - g_img_w) / 2U);
    g_dst_y    = (uint16_t)((lcddev.height - g_img_h) / 2U);
    g_band_top = -1;
    g_band_rows = 0;
    photo_dbg_result = -1;
    photo_dbg_bands  = 0;
    photo_dbg_rows   = 0;

    lcd_clear(0x0000);                   /* 背景黑 */

    r = jd_decomp(&jd, jpg_out, scale);
    photo_dbg_result = (int16_t)r;
    band_flush();                        /* 最后一条带 */

    if (r != JDR_OK)
    {
        return PHOTO_ERR_DEC;
    }
    return PHOTO_OK;
}

uint16_t photo_width(void)
{
    return g_img_w;
}

uint16_t photo_height(void)
{
    return g_img_h;
}
