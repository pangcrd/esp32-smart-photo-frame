#include "sys/system_monitor.h"

#include <WiFi.h>
#include <math.h>

#include "drv/sd_card_conf.h"
extern SDCardManager sd;

#if defined(__has_include) && __has_include("soc/soc_caps.h")
#  include "soc/soc_caps.h"
#endif

#if (defined(SOC_TEMP_SENSOR_SUPPORTED) && SOC_TEMP_SENSOR_SUPPORTED) || defined(CONFIG_IDF_TARGET_ESP32)
#  define SYSTEM_MONITOR_HAS_TEMPERATURE 1
#endif

#ifndef SYSTEM_MONITOR_HAS_TEMPERATURE
#  define SYSTEM_MONITOR_HAS_TEMPERATURE 0
#endif

SystemMonitor monitor;

namespace
{
String formatBytes(uint64_t bytes)
{
    if (bytes < 1024ULL) return String(bytes) + " B";
    if (bytes < 1024ULL * 1024ULL) return String(bytes / 1024.0, 1) + " KB";
    if (bytes < 1024ULL * 1024ULL * 1024ULL) return String(bytes / (1024.0 * 1024.0), 1) + " MB";
    return String(bytes / (1024.0 * 1024.0 * 1024.0), 2) + " GB";
}

bool readCpuTemperature(float &temperatureC)
{
#if SYSTEM_MONITOR_HAS_TEMPERATURE
    temperatureC = temperatureRead();
    return !isnan(temperatureC) && temperatureC > -100.0f && temperatureC < 200.0f;
#else
    (void)temperatureC;
    return false;
#endif
}
} // namespace

SystemMonitor::SystemMonitor(uint32_t updateIntervalMs)
    : _updateIntervalMs(updateIntervalMs == 0 ? 5000 : updateIntervalMs),
      _lastReportMs(0),
      _lastMillis(0),
      _uptimeMs(0),
      _started(false),
      _clockInitialized(false)
{
}

void SystemMonitor::begin()
{
    const uint32_t now = millis();
    _lastReportMs = now - _updateIntervalMs; // First update prints immediately.
    _lastMillis = now;
    _uptimeMs = now;
    _clockInitialized = true;
    _started = true;
    refreshStatus();
    pinMode(42, OUTPUT);
}

void SystemMonitor::update()
{
    if (!_started) begin();

    const uint32_t now = millis();
    updateUptime(now);

    networkCheckLED();

    if (static_cast<uint32_t>(now - _lastReportMs) < _updateIntervalMs) return;

    _lastReportMs = now;
    refreshStatus();
    printStatus();
}

void SystemMonitor::setUpdateInterval(uint32_t updateIntervalMs)
{
    _updateIntervalMs = updateIntervalMs == 0 ? 5000 : updateIntervalMs;
}

uint32_t SystemMonitor::updateInterval() const
{
    return _updateIntervalMs;
}

const SystemStatus &SystemMonitor::status() const
{
    return _status;
}

void SystemMonitor::updateUptime(uint32_t now)
{
    if (!_clockInitialized)
    {
        _lastMillis = now;
        _clockInitialized = true;
        return;
    }

    // Unsigned subtraction remains correct across the 32-bit millis() rollover.
    _uptimeMs += static_cast<uint32_t>(now - _lastMillis);
    _lastMillis = now;
    _status.uptimeMs = _uptimeMs;
}

void SystemMonitor::refreshStatus()
{
    float temperatureC = 0.0f;
    _status.cpuTemperatureSupported = readCpuTemperature(temperatureC);
    _status.cpuTemperatureC = temperatureC;

    _status.totalHeap = ESP.getHeapSize();
    _status.freeHeap = ESP.getFreeHeap();

    _status.wifiConnected = WiFi.status() == WL_CONNECTED;
    if (_status.wifiConnected)
    {
        _status.wifiSsid = WiFi.SSID();
        _status.wifiRssi = WiFi.RSSI();
    }
    else
    {
        _status.wifiSsid = String();
        _status.wifiRssi = -127;
    }

    // cardType() is the driver's live presence check, so insertion/removal is
    // observed without rebooting or touching the SDMMC pin configuration.
    const uint8_t cardType = sd.cardType();
    _status.sdCardPresent = cardType != CARD_NONE;
    _status.sdTotalBytes = 0;
    _status.sdUsedBytes = 0;
    _status.sdFreeBytes = 0;

    if (_status.sdCardPresent)
    {
        _status.sdTotalBytes = sd.getTotalSpace();
        _status.sdUsedBytes = sd.getUsedSpace();
        _status.sdFreeBytes = _status.sdTotalBytes > _status.sdUsedBytes
                                  ? _status.sdTotalBytes - _status.sdUsedBytes
                                  : 0;
    }

    // The wrapper exposes no separate mounted flag. A live card with a valid
    // filesystem size is the observable mounted state without changing it.
    _status.sdMounted = _status.sdCardPresent && _status.sdTotalBytes > 0;
    _status.uptimeMs = _uptimeMs;
}

String SystemMonitor::formatUptime(uint64_t milliseconds)
{
    const uint64_t totalSeconds = milliseconds / 1000ULL;
    const uint64_t days = totalSeconds / 86400ULL;
    const uint64_t hours = (totalSeconds / 3600ULL) % 24ULL;
    const uint64_t minutes = (totalSeconds / 60ULL) % 60ULL;
    const uint64_t seconds = totalSeconds % 60ULL;

    if (days > 0)
    {
        String result = String(days) + (days == 1 ? " day" : " days");
        if (hours > 0) result += " " + String(hours) + (hours == 1 ? " hour" : " hours");
        return result;
    }
    if (hours > 0) return String(hours) + (hours == 1 ? " hour" : " hours");
    if (minutes > 0) return String(minutes) + (minutes == 1 ? " minute" : " minutes");
    return String(seconds) + (seconds == 1 ? " second" : " seconds");
}

void SystemMonitor::networkCheckLED(){
    _status.wifiConnected = WiFi.status() == WL_CONNECTED;

    const uint8_t LED_PIN = 42;
    const unsigned long CYCLE_INTERVAL = 5000;  
    const unsigned long BLINK_ON_TIME  = 60;     
    const unsigned long BLINK_OFF_TIME = 100;    
    const uint8_t BLINK_COUNT = 2;              

    static unsigned long cycleStart = 0;
    static uint8_t blinkStep = 0;   // 0..(BLINK_COUNT*2 - 1): chẵn = ON, lẻ = OFF
    static unsigned long stepStart = 0;
    static bool initialized = false;

    if (!_status.wifiConnected)
    {
        digitalWrite(LED_PIN, LOW);
        initialized = false; // reset để lần connect sau chớp lại từ đầu
        return;
    }

    unsigned long now = millis();

    if (!initialized)
    {
        cycleStart = now;
        stepStart = now;
        blinkStep = 0;
        digitalWrite(LED_PIN, HIGH);
        initialized = true;
        return;
    }
    // Đang trong giai đoạn chớp (double-blink)
    if (blinkStep < BLINK_COUNT * 2)
    {
        bool isOnStep = (blinkStep % 2 == 0);
        unsigned long stepDuration = isOnStep ? BLINK_ON_TIME : BLINK_OFF_TIME;

        if (now - stepStart >= stepDuration)
        {
            blinkStep++;
            stepStart = now;

            if (blinkStep < BLINK_COUNT * 2)
            {
                bool nextOn = (blinkStep % 2 == 0);
                digitalWrite(LED_PIN, nextOn ? HIGH : LOW);
            }
            else
            {
                digitalWrite(LED_PIN, LOW);
            }
        }
    }
    else
    {
        if (now - cycleStart >= CYCLE_INTERVAL)
        {
            cycleStart = now;
            stepStart = now;
            blinkStep = 0;
            digitalWrite(LED_PIN, HIGH);
        }
    }
}
void SystemMonitor::printStatus() const
{
    Serial.println();
    Serial.println("----------------------------------------");
    Serial.println("System Status");
    Serial.println("----------------------------------------");

    Serial.print("CPU Temperature: ");
    if (_status.cpuTemperatureSupported) Serial.printf("%.1f C\n", _status.cpuTemperatureC);
    else Serial.println("Not Supported");

    Serial.printf("Total Heap: %u bytes\n", _status.totalHeap);
    Serial.printf("Free Heap: %u bytes\n", _status.freeHeap);

    Serial.print("WiFi Status: ");
    Serial.println(_status.wifiConnected ? "Connected" : "Disconnected");
    Serial.print("SSID: ");
    Serial.println(_status.wifiConnected ? _status.wifiSsid : "N/A");
    Serial.print("WiFi RSSI: ");
    if (_status.wifiConnected) Serial.printf("%d dBm\n", _status.wifiRssi);
    else Serial.println("N/A");

    Serial.println("SD Card Status:");
    Serial.print("  Card Present: ");
    Serial.println(_status.sdCardPresent ? "Yes" : "No");
    Serial.print("  Mounted: ");
    Serial.println(_status.sdMounted ? "Yes" : "No");
    Serial.print("  Total Size: ");
    Serial.println(formatBytes(_status.sdTotalBytes));
    Serial.print("  Used Space: ");
    Serial.println(formatBytes(_status.sdUsedBytes));
    Serial.print("  Free Space: ");
    Serial.println(formatBytes(_status.sdFreeBytes));

    Serial.print("Uptime: ");
    Serial.println(formatUptime(_status.uptimeMs));
    Serial.println("----------------------------------------");
}
