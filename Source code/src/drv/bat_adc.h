#pragma once

/**
 * @brief Initialize the battery ADC input.
 */
void bat_adc_init();

/**
 * @brief Read the current battery voltage from the ADC.
 */
float bat_adc_read_voltage();

/**
 * @brief Convert the current battery voltage to a clamped percentage.
 */
int bat_adc_get_percentage();
