#pragma once

#include <FS.h>
#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <WebServer.h>
#include "sys/system_monitor.h"
#include "wifi/wifi_manager.h"

class WebServerManager
{
public:
    /**
     * @brief Hold the persisted Open-Meteo location configuration.
     */
    struct WeatherConfig {
        float latitude = 0.0f;
        float longitude = 0.0f;
        int32_t timezoneSeconds = 0;
        String city;
        uint32_t revision = 1;
    };

    explicit WebServerManager(SystemMonitor &monitor = ::monitor);

    bool begin(const char *mdnsHostname = "photoframe");
    void update();

    uint32_t slideIntervalMs() const { return _slideIntervalMs; }
    WeatherConfig weatherConfig() const { return _weatherConfig; }

private:
    void registerRoutes();
    void handleRoot();
    void handleWebAsset(const char *path, const char *contentType);
    void handleSystem();
    void handleSystemPost();
    void handleWiFiScan();
    void handleWiFiConnect();
    void handleWiFiReset();
    void handleNotFound();
    void refreshGalleryCache();
    void addGalleryCacheEntry(const String &path, size_t size);
    void removeGalleryCacheEntry(const String &path);
    void handleGallery();
    void handleGalleryFile();
    void handleGalleryUpload();
    void handleGalleryUploadData();
    void handleGalleryDelete();
    void loadGallerySettings();
    bool saveGallerySettings();
    void handleGalleryInterval();
    void handleBrightness();
    void handleReboot();
    void handleFactoryReset();
    void handleWeather();
    void handleWeatherCurrent();
    void loadWeatherSettings();
    bool saveWeatherSettings();

    SystemMonitor &_monitor;
    WebServer _server;
    String _mdnsHostname;
    bool _filesystemReady;
    bool _serverStarted;
    File _uploadFile;
    bool _uploadOk = false;
    size_t _uploadBytesWritten = 0;

    String _uploadPath;
    struct GalleryEntry { String path; size_t sizeBytes; };
    std::vector<GalleryEntry> _galleryCache;
    bool _galleryCacheReady = false;


    uint32_t _slideIntervalMs = 5000;
    uint8_t _brightness;
    WeatherConfig _weatherConfig;
};

extern WebServerManager webServerManager;