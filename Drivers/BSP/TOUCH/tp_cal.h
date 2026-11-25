/**
 ****************************************************************************************************
 * @file        tp_cal.h
 * @brief       触摸屏校准数据存储（存 STM32 内部 Flash，不依赖外部 EEPROM）
 *
 * 背景：这块板子没有 AT24C02，原 touch.c 把校准值存 24C02，读不到就每次上电重新校准。
 *       这里改成存到 MCU 内部 Flash 的最后一个扇区，掉电不丢。
 ****************************************************************************************************
 */

#ifndef __TP_CAL_H
#define __TP_CAL_H

#include "./SYSTEM/sys/sys.h"

/**
 * @brief  从内部 Flash 读取校准数据并写入 tp_dev
 * @retval 1 = 有有效数据；0 = 没有（需要校准）
 */
uint8_t tp_cal_load(void);

/**
 * @brief  把 tp_dev 里的校准数据保存到内部 Flash
 */
void tp_cal_save(void);

/**
 * @brief  清除内部 Flash 里的校准数据（下次上电会重新校准）
 */
void tp_cal_erase(void);

#endif
