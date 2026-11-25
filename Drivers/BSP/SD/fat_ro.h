/**
 ****************************************************************************************************
 * @file        fat_ro.h
 * @brief       SD 卡上的 FAT16 / FAT32 只读文件系统（自己实现，不依赖 FatFs / RL-FlashFS）
 *
 * 为什么不用 FatFs：Keil 自带的 RL-FlashFS 需要整套 RTE 配置，而且会接管 C 库的
 * fopen/fread，和本工程 usart.c 里的 printf 重定向冲突。
 *
 * 本模块只做"读"：挂载 → 列目录 → 按文件名读整个文件。足够用于读图片、音乐、字体。
 ****************************************************************************************************
 */

#ifndef __FAT_RO_H
#define __FAT_RO_H

#include "./SYSTEM/sys/sys.h"

#define FAT_NAME_MAX     64      /* 文件名最长（含长文件名） */

typedef struct
{
    uint8_t  type;              /* 16 或 32 */
    uint32_t part_lba;          /* 分区起始扇区 */
    uint32_t fat_lba;           /* FAT 表起始扇区 */
    uint32_t data_lba;          /* 数据区起始扇区（簇 2） */
    uint32_t root_lba;          /* FAT16 根目录起始扇区 */
    uint32_t root_cluster;      /* FAT32 根目录簇号 */
    uint32_t root_entries;      /* FAT16 根目录项数 */
    uint32_t spc;               /* 每簇扇区数 */
    uint32_t total_sectors;
    uint32_t sec_per_fat;
} fat_fs_t;

typedef struct
{
    char     name[FAT_NAME_MAX];  /* 文件名（有长文件名就用长名） */
    uint32_t size;                /* 字节数 */
    uint8_t  attr;                /* 0x10 = 目录 */
    uint32_t first_cluster;       /* 起始簇号 */
} fat_entry_t;

/**
 * @brief  挂载文件系统
 * @retval 0=成功，其它=错误码
 */
uint8_t fat_mount(fat_fs_t *fs);

/**
 * @brief  统计根目录下的有效目录项数量
 */
uint32_t fat_root_count(fat_fs_t *fs);

/**
 * @brief  取根目录第 index 个有效目录项
 * @retval 0=成功，其它=越界/失败
 */
uint8_t fat_root_entry(fat_fs_t *fs, uint32_t index, fat_entry_t *out);

/**
 * @brief  在根目录按文件名（8.3 短名，不区分大小写）查找
 * @retval 0=找到
 */
uint8_t fat_find(fat_fs_t *fs, const char *name, fat_entry_t *out);

/**
 * @brief  读取整个文件到 buf
 * @param  buf     : 目标缓冲区
 * @param  maxlen  : 缓冲区大小
 * @param  out_len : 实际读到的字节数
 * @retval 0=成功
 */
uint8_t fat_read_file(fat_fs_t *fs, const fat_entry_t *e, uint8_t *buf, uint32_t maxlen, uint32_t *out_len);

#endif
