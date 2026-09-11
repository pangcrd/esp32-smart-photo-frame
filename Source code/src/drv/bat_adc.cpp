#include "bat_adc.h"

#include <Arduino.h>

#include "gpio_conf.h"

namespace {
constexpr float kBatteryDividerRatio = 2.0f;
constexpr float kBatteryEmptyVoltage = 3.30f;
constexpr float kBatteryFullVoltage = 4.20f;
constexpr float kAdcMillivoltsToVolts = 0.001f;
constexpr int kMinimumBatteryPercentage = 0;
constexpr int kMaximumBatteryPercentage = 100;

bool adc_initialized = false;

int clamp_percentage(int percentage)
{
    if (percentage < kMinimumBatteryPercentage) {
        return kMinimumBatteryPercentage;
    }
    if (percentage > kMaximumBatteryPercentage) {
        return kMaximumBatteryPercentage;
    }
    return percentage;
}
}

/**
 * @brief Initialize the battery ADC input.
 */
void bat_adc_init()
{
    if (adc_initialized) {
        return;
    }

    pinMode(battery_adc_io::pin, INPUT);
    analogSetPinAttenuation(battery_adc_io::pin, ADC_11db);
    adc_initialized = true;
}

/**
 * @brief Read the current battery voltage from the ADC.
 */
float bat_adc_read_voltage()
{
    if (!adc_initialized) {
        bat_adc_init();
    }

    const uint32_t adcMillivolts = analogReadMilliVolts(battery_adc_io::pin);
    const float adcVoltage = static_cast<float>(adcMillivolts) * kAdcMillivoltsToVolts;
    return adcVoltage * kBatteryDividerRatio;
}

/**
 * @brief Convert the current battery voltage to a clamped percentage.
 */
int bat_adc_get_percentage()
{
    const float batteryVoltage = bat_adc_read_voltage();
    const float voltageRange = kBatteryFullVoltage - kBatteryEmptyVoltage;
    const float normalizedVoltage =
        (batteryVoltage - kBatteryEmptyVoltage) / voltageRange;
    const int percentage = static_cast<int>(normalizedVoltage * 100.0f + 0.5f);
    return clamp_percentage(percentage);
}
