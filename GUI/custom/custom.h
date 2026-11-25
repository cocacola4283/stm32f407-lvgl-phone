/*
 * Copyright 2026 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
 * terms, then you may not retain, install, activate or otherwise use the software.
 */

#ifndef CUSTOM_H
#define CUSTOM_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "../generated/gui_guider.h"

  typedef enum
  {
    GG_2048_MOVE_LEFT,
    GG_2048_MOVE_RIGHT,
    GG_2048_MOVE_UP,
    GG_2048_MOVE_DOWN,
  } gg_2048_move_direction;

  void custom_init(gg_ui_t *ui);
  void new_game(gg_ui_t *ui);
  void movement_check(gg_ui_t *ui, gg_2048_move_direction direction);

#ifdef __cplusplus
}
#endif
#endif /* CUSTOM_H */
