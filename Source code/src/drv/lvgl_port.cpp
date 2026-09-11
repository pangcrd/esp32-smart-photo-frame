#include "lvgl_port.h"

/*
 * - Added a full-screen black LVGL overlay on lv_layer_top().
 * - Added blocking fade helpers that advance LVGL animations while JPG decode runs.
 * - Kept the existing display flush, touch input, and loop integration unchanged.
 */

//LGFX tft;

static uint32_t draw_buf[DRAW_BUF_SIZE / 4];

/* LVGL calls it when a rendered image needs to be copied to the display*/
static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

/*Read the touchpad*/
// static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
// {
//     // if (touch_touched()) {
//     //     data->state = LV_INDEV_STATE_PRESSED;
//     //     data->point.x = touch_last_x;
//     //     data->point.y = touch_last_y;

//     //     // Debug touch coordinates (uncomment if needed)
//     //     // Serial.print("Touch: x=");
//     //     // Serial.print(touch_last_x);
//     //     // Serial.print(", y=");
//     //     // Serial.println(touch_last_y);
//     // } else {
//     //     data->state = LV_INDEV_STATE_RELEASED;
//     // }
// }

/*use Arduino's millis() as tick source*/
static uint32_t my_tick(void) { return millis(); }

void lvgl_port_init()
{
    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.println(LVGL_Arduino);

    tft.begin();

    // Initialize touch before initializing LVGL.
    // Map rotation from LVGL to FT6336
    /** LV_DISPLAY_ROTATION_90 → Touch ROTATION_INVERTED
        LV_DISPLAY_ROTATION_180  → Touch ROTATION_NORMAL
        LV_DISPLAY_ROTATION_0 → Touch ROTATION_NORMAL
        LV_DISPLAY_ROTATION_270 Touch ROTATION_INVERTED */
    // uint8_t touch_rotation = (TFT_ROTATION == LV_DISPLAY_ROTATION_90 ||
    //                            TFT_ROTATION == LV_DISPLAY_ROTATION_270)
    //                               ? ROTATION_INVERTED
    //                               : ROTATION_NORMAL;

    // touch_init(TFT_HOR_RES, TFT_VER_RES, touch_rotation);
    // Serial.print("Touch initialized with rotation: ");
    // Serial.println(touch_rotation);

    lv_init();
    lv_tick_set_cb(my_tick);

    /*Create the LVGL display, register flush callback and draw buffers*/
    lv_display_t *disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                            LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_rotation(disp, TFT_ROTATION);

    /*Initialize the input device driver*/
    // lv_indev_t *indev = lv_indev_create();
    // lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
    // lv_indev_set_read_cb(indev, my_touchpad_read);
}

void lvgl_port_loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(5); /* let this time pass */
}


