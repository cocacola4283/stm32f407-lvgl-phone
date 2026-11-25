/**
 ****************************************************************************************************
 * @file        tp_cal.c
 * @brief       触摸校准数据存储（存 STM32 内部 Flash，替代不存在的 24C02）
 *
 * 存储位置：Sector 7，地址 0x08060000（512KB Flash 的最后 128KB 扇区）
 * 必须保证程序结束地址 < 0x08060000，否则会擦到自己的代码。
 *
 * 安全机制：用链接器符号 Image$$ER_IROM1$$Limit 取程序结束地址，
 *           一旦盖住 0x08060000 就拒绝擦写。
 ****************************************************************************************************
 */

#include "tp_cal.h"
#include "./BSP/TOUCH/touch.h"
#include <string.h>

#define TP_CAL_USE_FLASH     1
#define TP_CAL_SECTOR        FLASH_SECTOR_7
#define TP_CAL_ADDR          0x08060000UL
#define TP_CAL_MAGIC         0x5AA5C33CUL

extern uint32_t Image$$ER_IROM1$$Limit[];
#define TP_CAL_CODE_END      ((uint32_t)Image$$ER_IROM1$$Limit)

typedef struct
{
    uint32_t magic;
    float    xfac;
    float    yfac;
    int16_t  xc;
    int16_t  yc;
} tp_cal_t;

uint8_t tp_cal_load(void)
{
#if !TP_CAL_USE_FLASH
    return 0;
#else
    const tp_cal_t *p = (const tp_cal_t *)TP_CAL_ADDR;

    if (p->magic != TP_CAL_MAGIC)
    {
        return 0;
    }
    if ((p->xfac == 0.0f) || (p->yfac == 0.0f))
    {
        return 0;
    }
    if ((p->xfac > 100.0f) || (p->xfac < -100.0f))
    {
        return 0;
    }
    if ((p->yfac > 100.0f) || (p->yfac < -100.0f))
    {
        return 0;
    }

    tp_dev.xfac = p->xfac;
    tp_dev.yfac = p->yfac;
    tp_dev.xc   = p->xc;
    tp_dev.yc   = p->yc;

    return 1;
#endif
}

void tp_cal_save(void)
{
#if !TP_CAL_USE_FLASH
    return;
#else
    FLASH_EraseInitTypeDef erase;
    tp_cal_t cal;
    uint32_t sector_err = 0;
    uint32_t *src;
    uint32_t i;

    if (TP_CAL_CODE_END > TP_CAL_ADDR)
    {
        return;
    }

    cal.magic = TP_CAL_MAGIC;
    cal.xfac  = tp_dev.xfac;
    cal.yfac  = tp_dev.yfac;
    cal.xc    = tp_dev.xc;
    cal.yc    = tp_dev.yc;
    src = (uint32_t *)&cal;

    HAL_FLASH_Unlock();

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP    | FLASH_FLAG_OPERR  | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Sector       = TP_CAL_SECTOR;
    erase.NbSectors    = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&erase, &sector_err) == HAL_OK)
    {
        for (i = 0; i < (sizeof(tp_cal_t) / 4U); i++)
        {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, TP_CAL_ADDR + (i * 4U), src[i]);
        }
    }

    HAL_FLASH_Lock();
#endif
}

void tp_cal_erase(void)
{
#if !TP_CAL_USE_FLASH
    return;
#else
    FLASH_EraseInitTypeDef erase;
    uint32_t sector_err = 0;

    if (TP_CAL_CODE_END > TP_CAL_ADDR)
    {
        return;
    }

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP    | FLASH_FLAG_OPERR  | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Sector       = TP_CAL_SECTOR;
    erase.NbSectors    = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    (void)HAL_FLASHEx_Erase(&erase, &sector_err);
    HAL_FLASH_Lock();
#endif
}