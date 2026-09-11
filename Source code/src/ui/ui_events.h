#pragma once


//void invalidate_qr_state();

void ui_events_update_time();

void ui_events_update_day_month();

void ui_events_update_temperature();

void ui_events_update_weather_image();


#ifdef __cplusplus
extern "C" {
#endif

void ui_events_init(void);
void ui_events_update(void);

#ifdef __cplusplus
}
#endif