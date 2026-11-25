/**
 * @file lv_port_indev.c
 * LVGL 9.5 输入移植：触摸屏（FT5206 / GT9xxx 电容屏，或 XPT2046 电阻屏）
 *
 * 说明：直接复用 BSP 里的 tp_dev（touch.c），不用关心底层是哪颗触摸芯片。
 *       tp_dev.scan(0) 之后：
 *         - tp_dev.sta & TP_PRES_DOWN 非 0 表示有按下
 *         - tp_dev.x[0] / tp_dev.y[0] 是屏幕坐标
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "./BSP/TOUCH/touch.h"

/**********************
 *  调试用全局变量
 **********************/
volatile int32_t  g_tp_x    = 0;
volatile int32_t  g_tp_y    = 0;
volatile uint8_t  g_tp_down = 0;
volatile uint16_t g_tp_sta  = 0;
volatile uint32_t g_tp_cnt  = 0;

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief  LVGL 回调：读触摸状态和坐标
 */
static void touchpad_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    static int32_t  last_x   = 0;
    static int32_t  last_y   = 0;
    static uint8_t  was_down = 0;

    /* 扫描一次触摸屏（tp_dev.scan 内部会更新 sta / x[0] / y[0]） */
    tp_dev.scan(0);

    g_tp_sta  = tp_dev.sta;
    g_tp_down = (tp_dev.sta & TP_PRES_DOWN) ? 1 : 0;

    if (g_tp_down)
    {
        /* 抖动过滤：位置变化小于 2 像素时，仍上报上一次的位置，
         * 避免电阻屏噪声被 LVGL 误判成"拖动/滚动"，导致按钮点不中。 */
        if ((tp_dev.x[0] > (last_x + 2)) || (tp_dev.x[0] + 2 < last_x) ||
            (tp_dev.y[0] > (last_y + 2)) || (tp_dev.y[0] + 2 < last_y) ||
            (g_tp_cnt == 0U))
        {
            last_x = tp_dev.x[0];
            last_y = tp_dev.y[0];
        }

        g_tp_x = last_x;
        g_tp_y = last_y;

        if (!was_down)
        {
            g_tp_cnt++;     /* 新的一次按下 */
        }

        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    was_down = g_tp_down;

    /* 松手时保持最后一次坐标，防止控件"跳"到 (0,0) */
    data->point.x = last_x;
    data->point.y = last_y;
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief  初始化 LVGL 输入设备
 * @note   必须在 tp_dev.init() 之后调用
 */
void lv_port_indev_init(void)
{
    lv_indev_t * indev;

    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read);
}
