#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "display_conf.h"

/*Set to your screen resolution and rotation*/
#define TFT_ROTATION  LV_DISPLAY_ROTATION_270

#define TFT_HOR_RES   240
#define TFT_VER_RES   320

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))

extern LGFX tft;

/* Initialize display, touch and LVGL. Call this once in setup(). */
void lvgl_port_init();

/* Call this in loop() to let LVGL handle its work */
void lvgl_port_loop();
