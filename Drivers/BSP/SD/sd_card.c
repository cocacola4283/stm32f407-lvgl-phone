/**
 ****************************************************************************************************
 * @file        sd_card.c
 * @brief       SD 卡驱动（SDIO 4 位模式）
 *
 * 硬件连接（来自板载原理图）：
 *   PC8  -> SDIO_D0
 *   PC9  -> SDIO_D1
 *   PC10 -> SDIO_D2
 *   PC11 -> SDIO_D3
 *   PC12 -> SDIO_CK
 *   PD2  -> SDIO_CMD
 *
 * 时钟：SDIO 需要 48MHz。本工程 sys_stm32_clock_init(336,8,2,7) 的 PLLQ=7，
 *       336/7 = 48MHz，正好满足。
 *       实际 SD 卡时钟 = 48MHz / (ClockDiv + 2)
 ****************************************************************************************************
 */

#include "sd_card.h"
#include <stdio.h>
#include <string.h>

SD_HandleTypeDef g_sd_handle;
sd_test_result_t g_sd_test;

/* DMA/轮询读扇区用的缓冲区，必须 4 字节对齐 */
static uint8_t g_sd_buf[512] __attribute__((aligned(4)));

/**
 * @brief  SD 底层初始化（GPIO + 时钟），由 HAL_SD_Init() 内部弱函数回调
 */
void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    if (hsd->Instance == SDIO)
    {
        __HAL_RCC_SDIO_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        /* PC8~PC12 : SDIO_D0~D3 + SDIO_CK */
        gpio_init_struct.Pin       = GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 |
                                     GPIO_PIN_11 | GPIO_PIN_12;
        gpio_init_struct.Mode      = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull      = GPIO_PULLUP;
        gpio_init_struct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init_struct.Alternate = GPIO_AF12_SDIO;
        HAL_GPIO_Init(GPIOC, &gpio_init_struct);

        /* PD2 : SDIO_CMD（注意只动 PD2，不能影响 FSMC 用的 PD 其它脚） */
        gpio_init_struct.Pin       = GPIO_PIN_2;
        gpio_init_struct.Mode      = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull      = GPIO_PULLUP;
        gpio_init_struct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init_struct.Alternate = GPIO_AF12_SDIO;
        HAL_GPIO_Init(GPIOD, &gpio_init_struct);
    }
}

void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDIO)
    {
        __HAL_RCC_SDIO_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
    }
}

/**
 * @brief  初始化 SD 卡
 * @retval 0=成功，其它=失败（就是 HAL 状态码）
 */
uint8_t sd_card_init(void)
{
    uint8_t ret;

    g_sd_handle.Instance = SDIO;

    g_sd_handle.Init.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
    g_sd_handle.Init.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
    g_sd_handle.Init.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
    g_sd_handle.Init.BusWide             = SDIO_BUS_WIDE_1B;      /* 先 1 位，识别阶段最稳 */
    g_sd_handle.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    g_sd_handle.Init.ClockDiv            = 8;                     /* 48MHz/(8+2) = 4.8MHz */

    ret = (uint8_t)HAL_SD_Init(&g_sd_handle);
    if (ret != 0)
    {
        return ret;
    }

    return 0;
}

/**
 * @brief  读扇区
 * @param  buf      : 目标缓冲区（建议 4 字节对齐）
 * @param  blk_addr : 扇区地址（512 字节为单位）
 * @param  cnt      : 扇区个数
 * @retval 0=成功
 */
uint8_t sd_card_read_blocks(uint8_t *buf, uint32_t blk_addr, uint32_t cnt)
{
    return (uint8_t)HAL_SD_ReadBlocks(&g_sd_handle, buf, blk_addr, cnt, 2000);
}

/**
 * @brief  上电自检：初始化 SD 卡并把结果打印到串口（USART1，115200）
 * @note   失败也不会死循环，会正常返回，不影响后面的 LVGL
 */
void sd_card_test(void)
{
    HAL_SD_CardInfoTypeDef info;
    uint8_t  ret;
    int      i;

    memset(&g_sd_test, 0, sizeof(g_sd_test));

    printf("\r\n===== SD CARD TEST =====\r\n");

    ret = sd_card_init();
    g_sd_test.init_ret = ret;
    printf("HAL_SD_Init  : %u  (0=OK)\r\n", ret);
    if (ret != 0)
    {
        g_sd_test.inited = 0;
        printf("SD card NOT found / init failed\r\n");
        printf("===== END =====\r\n");
        return;
    }
    g_sd_test.inited = 1;

    HAL_SD_GetCardInfo(&g_sd_handle, &info);
    g_sd_test.card_type     = info.CardType;
    g_sd_test.log_block_nbr = info.LogBlockNbr;
    g_sd_test.capacity_mb   = (uint32_t)(((uint64_t)info.LogBlockNbr * (uint64_t)info.LogBlockSize) / (1024ULL * 1024ULL));

    printf("CardType     : %lu\r\n", (unsigned long)info.CardType);
    printf("LogBlockSize : %lu bytes\r\n", (unsigned long)info.LogBlockSize);
    printf("LogBlockNbr  : %lu\r\n", (unsigned long)info.LogBlockNbr);
    printf("Capacity     : %lu MB\r\n", (unsigned long)g_sd_test.capacity_mb);

    /* 识别完成后再切 4 位总线提速 */
    ret = (uint8_t)HAL_SD_ConfigWideBusOperation(&g_sd_handle, SDIO_BUS_WIDE_4B);
    g_sd_test.wide_ret = ret;
    printf("WideBus 4bit : %u\r\n", ret);

    /* 读 0 号扇区（MBR 或 FAT 引导扇区），验证真的能读到数据 */
    memset(g_sd_buf, 0, sizeof(g_sd_buf));
    ret = sd_card_read_blocks(g_sd_buf, 0, 1);
    g_sd_test.read_ok = (ret == 0) ? 1 : 0;
    printf("ReadBlock0   : %u\r\n", ret);

    if (ret == 0)
    {
        (void)HAL_SD_GetCardState(&g_sd_handle);
        g_sd_test.sig_ok = (g_sd_buf[510] == 0x55 && g_sd_buf[511] == 0xAA) ? 1 : 0;

        printf("Byte[0..15]  : ");
        for (i = 0; i < 16; i++)
        {
            printf("%02X ", g_sd_buf[i]);
        }
        printf("\r\n");
        printf("BootSig      : %s\r\n", g_sd_test.sig_ok ? "0x55AA OK" : "NO/0x55AA");
    }

    printf("===== SD END =====\r\n");
}