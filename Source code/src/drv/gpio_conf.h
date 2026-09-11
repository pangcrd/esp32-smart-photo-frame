// gpio_conf.h
#pragma once
#include <stdint.h>
// LCD GPIO
namespace pin_lcd {
    constexpr int8_t  sclk = 12,
                      mosi = 11,
                      miso = 13,
                      dc   = 46,
                      cs   = 10,
                      rst  = -1,
                      busy = -1,
                      bl   = 45;
}
//Sdcard SDIO MMC 4 bit GPIO
namespace sdcard_io {
    constexpr uint8_t   clk = 38,
                        cmd = 40,
                        d0  = 39,
                        d1  = 41,
                        d2  = 48,
                        d3  = 47;
    constexpr uint32_t  freqHz     = 20000000;
    constexpr bool      oneBitMode = false;
}

namespace boot_button_io {
    constexpr uint8_t pin = 0;
}

// Battery voltage ADC input. 
namespace battery_adc_io {
    constexpr uint8_t pin = 7;
}

