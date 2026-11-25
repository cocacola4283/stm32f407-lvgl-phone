/**
 ****************************************************************************************************
 * @file        sd_card.h
 * @brief       SD 鍗￠┍鍔紙SDIO 4 浣嶆ā寮忥紝鍩轰簬 STM32 HAL 鐨?SD 椹卞姩锛? * @note        鏉胯浇 MiniSD 鍗″骇鎺ュ湪 SDIO锛歅C8~PC11 = D0~D3, PC12 = SCK, PD2 = CMD
 ****************************************************************************************************
 */

#ifndef __SD_CARD_H
#define __SD_CARD_H

#include "./SYSTEM/sys/sys.h"

/* SD 鍙ユ焺锛屽叏灞�涓�浠?*/
extern SD_HandleTypeDef g_sd_handle;

/* SD 自检结果，供界面显示 */
typedef struct
{
    uint8_t  inited;        /* 1 = 初始化成功 */
    uint8_t  init_ret;      /* HAL_SD_Init 返回值 */
    uint8_t  wide_ret;      /* 切 4 位总线返回值 */
    uint8_t  read_ok;       /* 1 = 读扇区成功 */
    uint8_t  sig_ok;        /* 1 = 0x55AA 签名正确 */
    uint32_t card_type;
    uint32_t capacity_mb;
    uint32_t log_block_nbr;
} sd_test_result_t;

extern sd_test_result_t g_sd_test;

uint8_t sd_card_init(void);                                                 /* 鍒濆鍖?SD 鍗★紝0=鎴愬姛 */
uint8_t sd_card_read_blocks(uint8_t *buf, uint32_t blk_addr, uint32_t cnt); /* 璇绘墖鍖猴紝0=鎴愬姛 */
void    sd_card_test(void);                                                 /* 涓婄數鑷锛氫覆鍙ｆ墦鍗板崱淇℃伅 */

#endif
