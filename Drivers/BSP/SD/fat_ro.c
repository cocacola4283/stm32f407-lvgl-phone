/**
 ****************************************************************************************************
 * @file        fat_ro.c
 * @brief       SD 卡 FAT16/FAT32 只读文件系统（自实现）
 *
 * 只依赖 sd_card_read_blocks()（读 512 字节扇区）。
 * 支持：MBR 分区表 / 超级软盘、FAT16、FAT32、8.3 短名、按名字查找、读整个文件。
 ****************************************************************************************************
 */

#include "fat_ro.h"
#include "./BSP/SD/sd_card.h"
#include <string.h>

static uint8_t g_sec[512] __attribute__((aligned(4)));

#define SECTOR_SIZE     512U

/* ---------------- 小工具 ---------------- */
static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t rd_sector(uint32_t lba)
{
    return sd_card_read_blocks(g_sec, lba, 1);
}

/* 由 BPB 填充 fs 结构 */
static uint8_t parse_bpb(fat_fs_t *fs, uint32_t part)
{
    uint32_t fat_size16, fat_size32, total16, total32;

    if (rd_sector(part) != 0)
    {
        return 2;
    }
    if (rd16(&g_sec[11]) != SECTOR_SIZE)
    {
        return 3;
    }

    fs->spc           = g_sec[13];
    fs->root_entries  = rd16(&g_sec[17]);
    total16           = rd16(&g_sec[19]);
    fat_size16        = rd16(&g_sec[22]);
    total32           = rd32(&g_sec[32]);
    fat_size32        = rd32(&g_sec[36]);

    fs->part_lba      = part;
    fs->total_sectors = (total16 != 0) ? total16 : total32;

    if (fs->spc == 0)
    {
        return 4;
    }

    if (fat_size16 != 0)                          /* FAT16 */
    {
        fs->type         = 16;
        fs->sec_per_fat  = fat_size16;
        fs->fat_lba      = part + rd16(&g_sec[14]);
        fs->root_lba     = fs->fat_lba + (g_sec[16] * fat_size16);
        fs->data_lba     = fs->root_lba + ((fs->root_entries * 32U + 511U) / 512U);
        fs->root_cluster = 0;
    }
    else                                          /* FAT32 */
    {
        fs->type         = 32;
        fs->sec_per_fat  = fat_size32;
        fs->fat_lba      = part + rd16(&g_sec[14]);
        fs->root_lba     = 0;
        fs->data_lba     = fs->fat_lba + (g_sec[16] * fat_size32);
        fs->root_cluster = rd32(&g_sec[44]);
        if ((fs->root_cluster < 2) || (fs->root_cluster > 0x0FFFFFFFU))
        {
            return 5;
        }
    }

    if ((fs->data_lba <= fs->part_lba) || (fs->data_lba > fs->total_sectors))
    {
        return 6;
    }

    return 0;
}

uint8_t fat_mount(fat_fs_t *fs)
{
    uint32_t part = 0;

    memset(fs, 0, sizeof(*fs));

    if (rd_sector(0) != 0)
    {
        return 1;
    }

    /* 有分区表？(0x55AA 且第 11 字节不是 512 → 说明不是 BPB) */
    if ((g_sec[510] == 0x55) && (g_sec[511] == 0xAA) && (rd16(&g_sec[11]) != SECTOR_SIZE))
    {
        uint8_t type = g_sec[0x1BE + 4];
        if ((type == 0x01) || (type == 0x04) || (type == 0x06) ||
            (type == 0x0B) || (type == 0x0C) || (type == 0x0E))
        {
            part = rd32(&g_sec[0x1BE + 8]);
        }
        else
        {
            return 7;
        }
    }

    return parse_bpb(fs, part);
}

static uint32_t next_cluster(const fat_fs_t *fs, uint32_t cluster)
{
    uint32_t fat_offset, fat_sec, val;

    fat_offset = (fs->type == 16) ? (cluster * 2U) : (cluster * 4U);
    fat_sec    = fs->fat_lba + (fat_offset / SECTOR_SIZE);

    if (rd_sector(fat_sec) != 0)
    {
        return 0xFFFFFFFFU;
    }

    if (fs->type == 16)
    {
        val = rd16(&g_sec[fat_offset % SECTOR_SIZE]);
        if (val >= 0xFFF8U)
        {
            return 0xFFFFFFFFU;
        }
        return val;
    }

    val = rd32(&g_sec[fat_offset % SECTOR_SIZE]) & 0x0FFFFFFFU;
    if (val >= 0x0FFFFFF8U)
    {
        return 0xFFFFFFFFU;
    }
    return val;
}

static uint32_t cluster_to_lba(const fat_fs_t *fs, uint32_t cluster)
{
    return fs->data_lba + ((cluster - 2U) * fs->spc);
}

static void short_name_to_str(const uint8_t *e, char *out, uint32_t outlen)
{
    uint32_t i, n = 0;

    for (i = 0; i < 8 && (n + 1) < outlen; i++)
    {
        if (e[i] == ' ')
        {
            break;
        }
        out[n++] = (char)e[i];
    }
    for (i = 0; i < 3 && (n + 1) < outlen; i++)
    {
        if (e[8 + i] == ' ')
        {
            break;
        }
        if (i == 0)
        {
            out[n++] = '.';
        }
        out[n++] = (char)e[8 + i];
    }
    out[n] = '\0';
}

static void parse_entry(const uint8_t *e, fat_entry_t *out)
{
    memset(out, 0, sizeof(*out));
    short_name_to_str(e, out->name, FAT_NAME_MAX);
    out->attr          = e[11];
    out->size          = rd32(&e[28]);
    out->first_cluster = ((uint32_t)rd16(&e[20]) << 16) | rd16(&e[26]);
}

static uint8_t entry_is_valid(const uint8_t *e)
{
    if (e[0] == 0x00) return 0;
    if (e[0] == 0xE5) return 0;
    if (e[11] == 0x0F) return 0;
    if ((e[11] & 0x08) != 0) return 0;
    if (e[0] == '.') return 0;
    return 1;
}

/* 根目录遍历：cb 返回 1 表示停止 */
typedef uint8_t (*fat_visit)(const uint8_t *raw, fat_entry_t *ent, void *ctx);

static void walk_root(fat_fs_t *fs, fat_visit cb, void *ctx)
{
    uint32_t i, s;

    if (fs->type == 16)
    {
        uint32_t sec_cnt = (fs->root_entries * 32U + 511U) / 512U;
        for (s = 0; s < sec_cnt; s++)
        {
            if (rd_sector(fs->root_lba + s) != 0)
            {
                return;
            }
            for (i = 0; i < SECTOR_SIZE; i += 32)
            {
                fat_entry_t ent;
                const uint8_t *e = &g_sec[i];

                if (e[0] == 0x00)
                {
                    return;
                }
                if (!entry_is_valid(e))
                {
                    continue;
                }
                parse_entry(e, &ent);
                if (cb(e, &ent, ctx))
                {
                    return;
                }
            }
        }
        return;
    }

    {
        uint32_t cluster = fs->root_cluster;
        while ((cluster >= 2) && (cluster != 0xFFFFFFFFU))
        {
            uint32_t lba = cluster_to_lba(fs, cluster);
            for (s = 0; s < fs->spc; s++)
            {
                if (rd_sector(lba + s) != 0)
                {
                    return;
                }
                for (i = 0; i < SECTOR_SIZE; i += 32)
                {
                    fat_entry_t ent;
                    const uint8_t *e = &g_sec[i];

                    if (e[0] == 0x00)
                    {
                        return;
                    }
                    if (!entry_is_valid(e))
                    {
                        continue;
                    }
                    parse_entry(e, &ent);
                    if (cb(e, &ent, ctx))
                    {
                        return;
                    }
                }
            }
            cluster = next_cluster(fs, cluster);
        }
    }
}

/* ---- 统计 ---- */
static uint8_t cb_count(const uint8_t *raw, fat_entry_t *ent, void *ctx)
{
    (void)raw;
    (void)ent;
    (*(uint32_t *)ctx)++;
    return 0;
}

uint32_t fat_root_count(fat_fs_t *fs)
{
    uint32_t cnt = 0;
    walk_root(fs, cb_count, &cnt);
    return cnt;
}

/* ---- 取第 index 个 ---- */
typedef struct
{
    uint32_t    want;
    uint32_t    cur;
    fat_entry_t out;
    uint8_t     got;
} one_ctx_t;

static uint8_t cb_one(const uint8_t *raw, fat_entry_t *ent, void *ctx)
{
    one_ctx_t *c = (one_ctx_t *)ctx;
    (void)raw;

    if (c->cur == c->want)
    {
        c->out = *ent;
        c->got = 1;
        return 1;
    }
    c->cur++;
    return 0;
}

uint8_t fat_root_entry(fat_fs_t *fs, uint32_t index, fat_entry_t *out)
{
    one_ctx_t c;

    memset(&c, 0, sizeof(c));
    c.want = index;
    walk_root(fs, cb_one, &c);

    if (c.got)
    {
        *out = c.out;
        return 0;
    }
    return 1;
}

/* ---- 按名字查（8.3 短名） ---- */
typedef struct
{
    char        want[11];
    fat_entry_t out;
    uint8_t     got;
} find_ctx_t;

static void name_to_short(const char *name, char out[11])
{
    uint32_t i, n;

    for (i = 0; i < 11; i++)
    {
        out[i] = ' ';
    }

    n = 0;
    for (i = 0; (name[i] != '\0') && (name[i] != '.') && (n < 8); i++)
    {
        char ch = name[i];
        if ((ch >= 'a') && (ch <= 'z'))
        {
            ch = (char)(ch - 'a' + 'A');
        }
        out[n++] = ch;
    }
    while ((name[i] != '\0') && (name[i] != '.'))
    {
        i++;
    }
    if (name[i] == '.')
    {
        uint32_t k;

        i++;
        k = 8;
        while ((name[i] != '\0') && (k < 11))
        {
            char ch = name[i++];
            if ((ch >= 'a') && (ch <= 'z'))
            {
                ch = (char)(ch - 'a' + 'A');
            }
            out[k++] = ch;
        }
    }
}

static uint8_t cb_find(const uint8_t *raw, fat_entry_t *ent, void *ctx)
{
    find_ctx_t *c = (find_ctx_t *)ctx;

    if (memcmp(raw, c->want, 11) == 0)
    {
        c->out = *ent;
        c->got = 1;
        return 1;
    }
    return 0;
}

uint8_t fat_find(fat_fs_t *fs, const char *name, fat_entry_t *out)
{
    find_ctx_t c;

    memset(&c, 0, sizeof(c));
    name_to_short(name, c.want);
    walk_root(fs, cb_find, &c);

    if (c.got)
    {
        *out = c.out;
        return 0;
    }
    return 1;
}

/**
 * @brief  读整个文件
 */
uint8_t fat_read_file(fat_fs_t *fs, const fat_entry_t *e, uint8_t *buf, uint32_t maxlen, uint32_t *out_len)
{
    uint32_t cluster = e->first_cluster;
    uint32_t remain  = e->size;
    uint32_t done    = 0;

    *out_len = 0;

    if ((e->attr & 0x10) != 0)
    {
        return 1;
    }
    if ((cluster < 2) && (e->size > 0))
    {
        return 2;
    }

    while ((remain > 0) && (cluster >= 2) && (cluster != 0xFFFFFFFFU))
    {
        uint32_t lba = cluster_to_lba(fs, cluster);
        uint32_t s;

        for (s = 0; (s < fs->spc) && (remain > 0); s++)
        {
            uint32_t n = (remain > SECTOR_SIZE) ? SECTOR_SIZE : remain;

            if (rd_sector(lba + s) != 0)
            {
                return 3;
            }
            if ((done + n) > maxlen)
            {
                n = maxlen - done;
            }
            memcpy(&buf[done], g_sec, n);
            done   += n;
            remain -= n;

            if (done >= maxlen)
            {
                *out_len = done;
                return 0;
            }
        }
        cluster = next_cluster(fs, cluster);
    }

    *out_len = done;
    return 0;
}
