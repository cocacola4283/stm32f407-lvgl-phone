/**
 ****************************************************************************************************
 * @file        lv_fs_fat.c
 * @brief       LVGL 文件系统驱动：盘符 'S' -> fat_ro（SD 卡 FAT16/32 只读）
 *
 * LVGL 里用 "S:/I2048.BIN" 这样的路径就能打开 SD 卡根目录下的文件。
 * 只支持只读（mode 里带写标志会失败）。
 ****************************************************************************************************
 */

#include "lv_fs_fat.h"
#include "./BSP/SD/sd_card.h"
#include "./BSP/SD/fat_ro.h"
#include <string.h>

#if LV_USE_FS_FAT == 0 && !defined(LV_USE_FS_CUSTOM_S)
/* 本驱动是自定义注册，不依赖 lv_conf 里的 LV_USE_FS_xxx 开关 */
#endif

static fat_fs_t   g_fs;
static uint8_t    g_mounted = 0;
static fat_file_t g_files[8];        /* 同时最多打开 8 个文件 */
static uint8_t    g_file_used[8];

/* ---------------- lv_fs 回调 ---------------- */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    uint8_t i;

    (void)drv;

    if ((mode & LV_FS_MODE_WR) != 0)
    {
        return NULL;                  /* 只读 */
    }
    if (!g_mounted)
    {
        return NULL;
    }

    for (i = 0; i < 8; i++)
    {
        if (!g_file_used[i])
        {
            if (fat_open(&g_fs, path, &g_files[i]) == 0)
            {
                g_file_used[i] = 1;
                return &g_files[i];
            }
            return NULL;
        }
    }
    return NULL;
}

static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    uint8_t i;

    (void)drv;

    for (i = 0; i < 8; i++)
    {
        if (&g_files[i] == (fat_file_t *)file_p)
        {
            g_file_used[i] = 0;
            return LV_FS_RES_OK;
        }
    }
    return LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    (void)drv;

    *br = fat_read((fat_file_t *)file_p, buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    fat_file_t *f = (fat_file_t *)file_p;
    uint32_t    target;

    (void)drv;

    if (whence == LV_FS_SEEK_SET)
    {
        target = pos;
    }
    else if (whence == LV_FS_SEEK_CUR)
    {
        target = f->pos + pos;
    }
    else
    {
        target = f->size + pos;
    }

    if (fat_seek(f, target) != 0)
    {
        return LV_FS_RES_UNKNOWN;
    }
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv;
    *pos_p = ((fat_file_t *)file_p)->pos;
    return LV_FS_RES_OK;
}

/**
 * @brief  注册 'S' 盘
 * @retval 0=成功；其它=SD 初始化或挂载失败
 */
uint8_t lv_fs_fat_init(void)
{
    static lv_fs_drv_t drv;           /* 必须是静态的，LVGL 会保存指针 */
    uint8_t ret;

    memset(g_file_used, 0, sizeof(g_file_used));
    g_mounted = 0;

    ret = sd_card_init();
    if (ret != 0)
    {
        return 1;
    }
    ret = fat_mount(&g_fs);
    if (ret != 0)
    {
        return 2;
    }
    g_mounted = 1;

    lv_fs_drv_init(&drv);
    drv.letter  = 'S';
    drv.open_cb = fs_open;
    drv.close_cb = fs_close;
    drv.read_cb = fs_read;
    drv.seek_cb = fs_seek;
    drv.tell_cb = fs_tell;
    lv_fs_drv_register(&drv);

    return 0;
}
