#pragma once

#include <Arduino.h>

// A copyable snapshot keeps the monitor useful to Serial output now and to a
// dashboard or display integration later.

class SDCardManager;

struct SystemStatus
{
    bool cpuTemperatureSupported = false;
    float cpuTemperatureC = 0.0f;

    uint32_t totalHeap = 0;
    uint32_t freeHeap = 0;

    bool wifiConnected = false;
    String wifiSsid;
    int32_t wifiRssi = -127;

    bool sdCardPresent = false;
    bool sdMounted = false;
    uint64_t sdTotalBytes = 0;
    uint64_t sdUsedBytes = 0;
    uint64_t sdFreeBytes = 0;

    uint64_t uptimeMs = 0;
};

class SystemMonitor
{
public:
    explicit SystemMonitor(uint32_t updateIntervalMs = 5000);

    // Starts the monitor only. WiFi and SD initialization remain owned by the
    // existing application modules.
    void begin();

    // Call from loop(). A status report is printed at the configured interval.
    void update();

    void setUpdateInterval(uint32_t updateIntervalMs);
    uint32_t updateInterval() const;

    const SystemStatus &status() const;

    // Converts milliseconds to a compact human-readable uptime.
    static String formatUptime(uint64_t milliseconds);

    // Exposed for applications that need to print a snapshot on demand.
    void printStatus() const;

private:
    void refreshStatus();
    void updateUptime(uint32_t now);
    void networkCheckLED();

    SystemStatus _status;
    uint32_t _updateIntervalMs;
    uint32_t _lastReportMs;
    uint32_t _lastMillis;
    uint64_t _uptimeMs;
    bool _started;
    bool _clockInitialized;
};

extern SystemMonitor monitor;
extern SDCardManager sd;