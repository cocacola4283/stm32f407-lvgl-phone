/**
 ****************************************************************************************************
 * @file        main.c
 * @author      Mr.Mr.
 * @version     V1.0
 * @date        2023-04-23
 * @brief       触摸屏实验
 * @license     Copyright (c) 2020-2032,  
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:ST-1 STM32F407核心板
 *  
 *  
 * 公司网址:www.genbotter.com
 * 购买地址:makerbase.taobao.com
 * 
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/tp_cal.h"
#include "./BSP/SRAM/sram.h"
#include "./BSP/TIMER/btim.h"
#include "./BSP/SD/sd_card.h"
#include "./BSP/SD/fat_ro.h"

/* ===================== LVGL ===================== */
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "gui_guider.h"
#include "gg_utils.h"
#include "custom.h"
#include "gui_2048.h"
#include "app_shell.h"

gg_ui_t guider_ui;
/* 触摸 RST 按钮的回调：软复位 */
static void reboot_cb(lv_event_t *e)
{
    (void)e;
    lv_refr_now(NULL);
    NVIC_SystemReset();
}   /* GUI Guider 生成的界面对象，必须由应用层定义 */

int main(void)
{
    HAL_Init();
    sys_stm32_clock_init(336, 8, 2, 7);
    delay_init(168);
    usart_init(115200);
    led_init();
    key_init();
    sram_init();

    lcd_init();
    lcd_display_dir(1);                 /* 横屏 320x240 */

    /* 上电按住 KEY0：清除触摸校准数据（下次上电会重新校准） */
    if (key_scan(0) == KEY0_PRES)
    {
        tp_cal_erase();
        lcd_clear(0x0000);
        delay_ms(1500);
    }

    tp_dev.init();                      /* 触摸初始化（首次会进校准） */

    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_port_disp_init();
    lv_port_indev_init();

    /* 手机桌面：主菜单 + app 切换 */
    app_shell_start();


    /* 主循环：跑 LVGL + 两个键都能软复位（板载 RESET 键是坏的） */
    {
        uint8_t armed = 0;

        while (1)
        {
            uint8_t key = key_scan(0);

            if (key == 0)
            {
                armed = 1;                       /* 先松手，之后按下才复位 */
            }
            else if (armed)
            {
                lcd_clear(0xF800);               /* 红屏 = 收到复位 */
                delay_ms(200);
                NVIC_SystemReset();
            }

            lv_timer_handler();
            delay_ms(5);
        }
    }
}