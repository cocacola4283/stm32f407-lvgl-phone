/**
 * @file lv_port_disp.c
 * LVGL 9.5 显示移植：STM32F407 + FSMC 并口 8080
 *
 * 【DMA 流水线版】
 *   LVGL 用双缓冲局部渲染：
 *     - 画满 buf1 → 调 disp_flush() → 这里只做"设窗口 + 启动 DMA"，立刻返回
 *     - DMA 在后台把 buf1 搬进屏幕 GRAM 的同时，LVGL 已经在渲染 buf2
 *     - DMA 传输完成中断里调 lv_display_flush_ready()，放行下一块
 *   这样"刷屏"和"渲染"真正并行，CPU 不再被搬像素拖死。
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include "./BSP/LCD/lcd.h"

/*********************
 *      DEFINES
 *********************/
#define DISP_BUF_LINES      32      /* 每块 320*32*2 = 20480 字节，两块共 40 KB（腾出 16K RAM 给 LVGL 堆） */
#define DISP_BUF_MAX_WIDTH  320

#define DISP_USE_DMA        1       /* 1 = 用 DMA 搬像素；0 = 退回 CPU 死循环 */

/**********************
 *  STATIC VARIABLES
 **********************/
static uint8_t disp_buf1[DISP_BUF_MAX_WIDTH * DISP_BUF_LINES * 2];
static uint8_t disp_buf2[DISP_BUF_MAX_WIDTH * DISP_BUF_LINES * 2];

#if DISP_USE_DMA
static DMA_HandleTypeDef g_dma;
static lv_display_t     *g_disp_flushing = NULL;
static volatile uint8_t  g_dma_busy      = 0;

/* DMA 传输完成（或出错）回调 */
static void dma_done_cb(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    g_dma_busy = 0;
    if (g_disp_flushing != NULL)
    {
        lv_display_flush_ready(g_disp_flushing);
        g_disp_flushing = NULL;
    }
}

/* 初始化 DMA2_Stream0：内存 -> 固定地址（LCD GRAM） */
static void disp_dma_init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    g_dma.Instance                 = DMA2_Stream0;
    g_dma.Init.Channel             = DMA_CHANNEL_0;            /* M2M 模式不吃通道请求，随便选 */
    g_dma.Init.Direction           = DMA_MEMORY_TO_MEMORY;
    g_dma.Init.PeriphInc           = DMA_PINC_ENABLE;          /* 源地址（LVGL 缓冲）递增 */
    g_dma.Init.MemInc              = DMA_MINC_DISABLE;         /* 目标地址（LCD GRAM）固定 */
    g_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;  /* 16 位 */
    g_dma.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    g_dma.Init.Mode                = DMA_NORMAL;
    g_dma.Init.Priority            = DMA_PRIORITY_HIGH;
    g_dma.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&g_dma);

    g_dma.XferCpltCallback  = dma_done_cb;
    g_dma.XferErrorCallback = dma_done_cb;                     /* 出错也放行，避免卡死 */

    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/* 把一块显示缓冲借出去当临时内存（调用方必须保证 LVGL 已暂停） */
void *lv_port_disp_get_scratch(uint32_t *size)
{
    if (size != NULL)
    {
        *size = (uint32_t)sizeof(disp_buf1);
    }
    return (void *)disp_buf1;
}

/* 等到 DMA 搬完当前这块（独占屏幕的应用在动手前必须调） */
void lv_port_disp_wait_idle(void)
{
    while (g_dma_busy)
    {
    }
}

/* 给 stm32f4xx_it.c 里的 DMA2_Stream0_IRQHandler 调用 */
void disp_dma_irq_handler(void)
{
    HAL_DMA_IRQHandler(&g_dma);
}
#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint16_t w = (uint16_t)(area->x2 - area->x1 + 1);
    uint16_t h = (uint16_t)(area->y2 - area->y1 + 1);
    uint32_t n = (uint32_t)w * (uint32_t)h;

    /* 1) 设窗口 + 发写 GRAM 命令（CPU，很短） */
    lcd_set_window((uint16_t)area->x1, (uint16_t)area->y1, w, h);
    lcd_write_ram_prepare();

#if DISP_USE_DMA
    /* 2) 上一块还在搬就等它（正常情况下不会等） */
    while (g_dma_busy)
    {
    }

    g_disp_flushing = disp;
    g_dma_busy      = 1;

    /* 3) 启动 DMA：源 = LVGL 缓冲，目标 = LCD->LCD_RAM（固定地址） */
    if (HAL_DMA_Start_IT(&g_dma, (uint32_t)px_map, (uint32_t)&LCD->LCD_RAM, n) != HAL_OK)
    {
        /* 启动失败就退回 CPU 搬，绝不卡死 */
        uint16_t *p = (uint16_t *)px_map;

        g_dma_busy      = 0;
        g_disp_flushing = NULL;
        while (n--)
        {
            LCD->LCD_RAM = *p++;
        }
        lv_display_flush_ready(disp);
    }
    /* 成功启动时**不**调 flush_ready，由 DMA 完成中断来调 */
#else
    {
        uint16_t *p = (uint16_t *)px_map;
        while (n--)
        {
            LCD->LCD_RAM = *p++;
        }
    }
    lv_display_flush_ready(disp);
#endif
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    lv_display_t *disp;
    uint32_t buf_size;

    disp = lv_display_create(lcddev.width, lcddev.height);

    buf_size = (uint32_t)lcddev.width * DISP_BUF_LINES * 2U;
    if (buf_size > sizeof(disp_buf1))
    {
        buf_size = sizeof(disp_buf1);
    }

    lv_display_set_buffers(disp, disp_buf1, disp_buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, disp_flush);

#if DISP_USE_DMA
    disp_dma_init();
#endif
}
