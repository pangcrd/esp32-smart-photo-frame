#include "ui_events.h"

#include <Arduino.h>
#include <lvgl.h>

#include "drv/gpio_conf.h"
#include "drv/bat_adc.h"
#include "jpg/jpg_gallery.h"
#include "wifi/wifi_manager.h"
#include "web/get_api_data.h"
#include "ui/screens/ui_image_gallery.h"
#include "ui/screens/ui_setup.h"

#include "ui.h"

namespace {
constexpr uint32_t kDebounceMs = 50;
constexpr uint32_t kBatteryUpdateIntervalMs = 1000;
constexpr uint32_t kWifiResetHoldMs = 5000;
constexpr uint8_t kGalleryRotationPortrait = 1;
constexpr uint8_t kGalleryRotationLandscape = 0;

const char *ap_name = WiFiManager::kApSsid;

enum class UIState {
    SETUP,
    IMAGE_GALLERY,
    IMAGE_GALLERY_WEATHER_VISIBLE,
    IMAGE_GALLERY_WEATHER_HIDDEN,
};

enum class QRState {
    INIT,
    WAIT_IP,
    MONITOR,
};

UIState current_state = UIState::SETUP;
QRState qr_state = QRState::INIT;
bool button_was_pressed = false;
uint32_t last_button_change_ms = 0;
uint32_t button_press_started_ms = 0;
bool wifi_reset_fired = false;
int last_battery_icon = -1;
uint32_t last_battery_update_ms = 0;
lv_obj_t *qr_obj = nullptr;
String last_qr_ip;
bool last_qr_ap_mode = false;
bool qr_has_value = false;
String last_weather_time;
String last_weather_day_month;
String last_weather_temperature;
int last_weather_code = -2;

void invalidate_qr_state();

bool valid_gallery_weather_object(lv_obj_t *object)
{
    return object != nullptr && lv_obj_is_valid(object) &&
           ui_image_gallery != nullptr && lv_obj_is_valid(ui_image_gallery);
}

/**
 * @brief Reset cached weather UI values after a screen rebuild.
 */
void invalidate_weather_ui_cache()
{
    last_weather_time = String();
    last_weather_day_month = String();
    last_weather_temperature = String();
    last_weather_code = -2;
}

/**
 * @brief Publish the latest weather snapshot to the gallery widgets.
 */
void update_weather_ui()
{
    const Weather::Data &weather = Weather::data();
    if (weather.timeValid) {
        ui_events_update_time();
        ui_events_update_day_month();
    }
    if (weather.weatherValid) {
        ui_events_update_temperature();
        ui_events_update_weather_image();
    }
}

void load_screen_safely(lv_obj_t *screen)
{
    if (screen != nullptr) {
        lv_disp_load_scr(screen);
    }
}

void switch_to_setup()
{
    lv_obj_t *transition_screen = lv_obj_create(nullptr);
    load_screen_safely(transition_screen);

    gallery.stop();
    ui_image_gallery_screen_destroy();
    invalidate_weather_ui_cache();
    invalidate_qr_state();
    ui_setup_screen_destroy();
    ui_setup_screen_init();
    last_battery_icon = -1;
    load_screen_safely(ui_setup);

    lv_obj_del(transition_screen);
    current_state = UIState::SETUP;
}

void switch_to_image_gallery()
{
    lv_obj_t *transition_screen = lv_obj_create(nullptr);
    load_screen_safely(transition_screen);

    gallery.stop();
    invalidate_qr_state();
    ui_setup_screen_destroy();
    ui_image_gallery_screen_destroy();
    invalidate_weather_ui_cache();
    ui_image_gallery_screen_init();
    load_screen_safely(ui_image_gallery);

    if (ui_time_weather_area != nullptr) {
        lv_obj_add_flag(ui_time_weather_area, LV_OBJ_FLAG_HIDDEN);
    }
    gallery.start(ui_image_gallery, "/", kGalleryRotationPortrait,
                  kGalleryRotationLandscape);

    lv_obj_del(transition_screen);
    current_state = UIState::IMAGE_GALLERY;
}

void set_weather_area_visible(bool visible)
{
    if (ui_time_weather_area == nullptr) return;

    if (visible) {
        lv_obj_clear_flag(ui_time_weather_area, LV_OBJ_FLAG_HIDDEN);
        current_state = UIState::IMAGE_GALLERY_WEATHER_VISIBLE;
    } else {
        lv_obj_add_flag(ui_time_weather_area, LV_OBJ_FLAG_HIDDEN);
        current_state = UIState::IMAGE_GALLERY_WEATHER_HIDDEN;
    }
}

void handle_button_press()
{
    switch (current_state) {
    case UIState::SETUP:
        switch_to_image_gallery();
        break;
    case UIState::IMAGE_GALLERY:
        set_weather_area_visible(true);
        break;
    case UIState::IMAGE_GALLERY_WEATHER_VISIBLE:
        set_weather_area_visible(false);
        break;
    case UIState::IMAGE_GALLERY_WEATHER_HIDDEN:
        switch_to_setup();
        break;
    }
}

/**
 * @brief Invalidate QR state before the setup screen is destroyed.
 */
void invalidate_qr_state()
{
    qr_obj = nullptr;
    last_qr_ip = String();
    last_qr_ap_mode = false;
    qr_has_value = false;
    qr_state = QRState::INIT;
}

/**
 * @brief Check whether an IP address contains a usable value.
 */
bool has_valid_ip(const IPAddress &ip)
{
    return ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0;
}

/**
 * @brief Update the QR code and label for the current device IP.
 */
bool update_qr_for_current_ip()
{
    if (qr_obj == nullptr || !lv_obj_is_valid(qr_obj) ||
        ui_ip_address == nullptr || !lv_obj_is_valid(ui_ip_address)) {
        return false;
    }

    if (!wifiManager.isApMode() && !wifiManager.isConnected()) {
        return false;
    }

    const IPAddress current_ip = wifiManager.currentIp();
    if (!has_valid_ip(current_ip)) {
        return false;
    }

    const String ip_text = current_ip.toString();
    const bool ap_mode = wifiManager.isApMode();
    if (qr_has_value && ip_text == last_qr_ip && ap_mode == last_qr_ap_mode) {
        return true;
    }

    // QR encode IP
    const String qr_data = String("http://") + ip_text;
    lv_qrcode_update(qr_obj, qr_data.c_str(), qr_data.length());

    if (ap_mode) {
        const String label_text = String("AP: ") + WiFiManager::kApSsid;
        lv_label_set_text(ui_ip_address, label_text.c_str());
        // if (ui_rssid != nullptr && lv_obj_is_valid(ui_rssid)) {
        //     lv_label_set_text(ui_rssid, "");
        // }
    } else {
        // hiện SSID thực tế đang dùng + RSSI.
        const String ssid_text = String("SSID: ") + WiFi.SSID();
        //const String rssi_text = String(WiFi.RSSI()) + " dBm";
        lv_label_set_text(ui_ip_address, ssid_text.c_str());
        // if (ui_rssid != nullptr && lv_obj_is_valid(ui_rssid)) {
        //     lv_label_set_text(ui_rssid, rssi_text.c_str());
        // }
    }
    lv_obj_center(qr_obj);

    last_qr_ip = ip_text;
    last_qr_ap_mode = ap_mode;
    qr_has_value = true;
    return true;
}

/**
 * @brief Process one non-blocking step of the QR update state machine.
 */
void process_qr_state()
{
    if (current_state != UIState::SETUP) {
        return;
    }

    switch (qr_state) {
    case QRState::INIT:
        if (ui_qr_aera == nullptr || !lv_obj_is_valid(ui_qr_aera)) {
            return;
        }

        qr_obj = lv_qrcode_create(ui_qr_aera);
        if (qr_obj == nullptr || !lv_obj_is_valid(qr_obj)) {
            qr_obj = nullptr;
            return;
        }

        lv_qrcode_set_size(qr_obj, 125);
        lv_qrcode_set_dark_color(
            qr_obj, lv_palette_darken(LV_PALETTE_BLUE, 4));
        lv_qrcode_set_light_color(
            qr_obj, lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5));
        lv_obj_set_style_border_color(
            qr_obj, lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5), 0);
        lv_obj_set_style_border_width(qr_obj, 5, 0);
        lv_obj_center(qr_obj);
        qr_state = QRState::WAIT_IP;
        return;

    case QRState::WAIT_IP:
        if (qr_obj == nullptr || !lv_obj_is_valid(qr_obj)) {
            invalidate_qr_state();
            return;
        }
        if (update_qr_for_current_ip()) {
            qr_state = QRState::MONITOR;
        }
        return;

    case QRState::MONITOR:
        if (qr_obj == nullptr || !lv_obj_is_valid(qr_obj) ||
            ui_qr_aera == nullptr || !lv_obj_is_valid(ui_qr_aera)) {
            invalidate_qr_state();
            return;
        }
        if (!update_qr_for_current_ip()) {
            qr_state = QRState::WAIT_IP;
        }
        return;
    }
}

/**
 * @brief Select the battery image bucket for a percentage value.
 */
int battery_icon_for_percentage(int percentage)
{
    if (percentage < 20) return 20;
    if (percentage < 40) return 40;
    if (percentage < 60) return 60;
    if (percentage < 80) return 80;
    return 100;
}

/**
 * @brief Update the setup-screen battery image when its level changes.
 */
void update_battery_icon()
{
    if (ui_batt == nullptr || !lv_obj_is_valid(ui_batt)) {
        return;
    }

    const int percentage = bat_adc_get_percentage();
    const int iconLevel = battery_icon_for_percentage(percentage);
    if (iconLevel == last_battery_icon) {
        return;
    }

    switch (iconLevel) {
    case 20:
        lv_image_set_src(ui_batt, &ui_img_img_batt20_png);
        break;
    case 40:
        lv_image_set_src(ui_batt, &ui_img_img_batt40_png);
        break;
    case 60:
        lv_image_set_src(ui_batt, &ui_img_img_batt60_png);
        break;
    case 80:
        lv_image_set_src(ui_batt, &ui_img_img_batt80_png);
        break;
    case 100:
        lv_image_set_src(ui_batt, &ui_img_img_batt100_png);
        break;
    default:
        return;
    }

    last_battery_icon = iconLevel;
}
} // namespace

/**
 * @brief Update the LVGL time label when the gallery screen is valid.
 */
void ui_events_update_time()
{
    const Weather::Data &weather = Weather::data();
    if (!weather.timeValid || !valid_gallery_weather_object(ui_time)) return;
    if (last_weather_time == weather.currentTime) return;

    lv_label_set_text(ui_time, weather.currentTime.c_str());
    last_weather_time = weather.currentTime;
}

/**
 * @brief Update the LVGL weekday and month label when the screen is valid.
 */
void ui_events_update_day_month()
{
    const Weather::Data &weather = Weather::data();
    if (!weather.timeValid || !valid_gallery_weather_object(ui_week_month)) return;
    if (last_weather_day_month == weather.currentDayMonth) return;

    lv_label_set_text(ui_week_month, weather.currentDayMonth.c_str());
    last_weather_day_month = weather.currentDayMonth;
}

/**
 * @brief Update the LVGL temperature label when weather data is valid.
 */
void ui_events_update_temperature()
{
    const Weather::Data &weather = Weather::data();
    if (!weather.weatherValid || !valid_gallery_weather_object(ui_temp)) return;

    char temperature[16];
    snprintf(temperature, sizeof(temperature), "%d°",weather.temperature);
    if (last_weather_temperature == temperature) return;

    lv_label_set_text(ui_temp, temperature);
    last_weather_temperature = temperature;
}

/**
 * @brief Update the LVGL weather image using the WMO icon mapping.
 */
void ui_events_update_weather_image()
{
    const Weather::Data &weather = Weather::data();
    if (!weather.weatherValid || !valid_gallery_weather_object(ui_weather)) return;
    if (last_weather_code == weather.weatherCode) return;

    switch (Weather::iconIndexForCode(weather.weatherCode))
    {
    case 1:
        lv_image_set_src(ui_weather, &ui_img_img_weather_1_png);
        break;
    case 2:
        lv_image_set_src(ui_weather, &ui_img_img_weather_2_png);
        break;
    case 3:
        lv_image_set_src(ui_weather, &ui_img_img_weather_3_png);
        break;
    case 4:
        lv_image_set_src(ui_weather, &ui_img_img_weather_4_png);
        break;
    case 5:
        lv_image_set_src(ui_weather, &ui_img_img_weather_5_png);
        break;
    default:
        return;
    }

    last_weather_code = weather.weatherCode;
}

/**
 * @brief Reset only WiFi credentials and switch directly to the setup AP.
 * Gallery/weather config are intentionally left untouched — those are
 * managed separately via the web server. This lets the user quickly
 * reconnect to a new WiFi network without losing gallery/weather setup.
 */
bool perform_wifi_only_reset()
{
    if (!wifiManager.clearConfiguration())
    {
        Serial.println("[UI] WiFi reset: clearConfiguration failed");
        return false;
    }

    if (!wifiManager.hardwareWifiReset())
    {
        Serial.println("[UI] WiFi reset: AP start failed");
        return false;
    }

    Serial.println("[UI] WiFi reset: cleared, AP mode started");
    return true;
}

void ui_events_init()
{
    pinMode(boot_button_io::pin, INPUT_PULLUP);
    bat_adc_init();
    current_state = UIState::SETUP;
    button_was_pressed = digitalRead(boot_button_io::pin) == LOW;
    last_button_change_ms = millis();
    button_press_started_ms = millis();
    wifi_reset_fired = button_was_pressed; 
    last_battery_update_ms = millis() - kBatteryUpdateIntervalMs;
    last_battery_icon = -1;
    invalidate_qr_state();
    invalidate_weather_ui_cache();
    Weather::begin();
}

void ui_events_update()
{
    const bool button_is_pressed = digitalRead(boot_button_io::pin) == LOW;
    const uint32_t now = millis();

    if (button_is_pressed != button_was_pressed &&
        now - last_button_change_ms >= kDebounceMs) {
        button_was_pressed = button_is_pressed;
        last_button_change_ms = now;

     
        if (button_is_pressed) {
            button_press_started_ms = now;
            wifi_reset_fired = false;
        } else if (!wifi_reset_fired) {
            handle_button_press();
        }
    }
        if (button_is_pressed && !wifi_reset_fired &&
        now - button_press_started_ms >= kWifiResetHoldMs) {
        wifi_reset_fired = true;
        if (perform_wifi_only_reset()) {
            switch_to_setup();
        }
    }

    
    Weather::update();
    update_weather_ui();
    process_qr_state();

    if (now - last_battery_update_ms >= kBatteryUpdateIntervalMs) {
        last_battery_update_ms = now;
        update_battery_icon();
    }
}
