#include "web_server_manager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include "drv/sd_card_conf.h"

#include "jpg/jpg_gallery.h"
#include "web/get_api_data.h"

#include "ui/ui_events.h"

namespace
{
String simpleResponseJson(bool ok, const char *error = nullptr, bool restarting = false)
{
    JsonDocument document;
    document["ok"] = ok;
    if (error != nullptr) document["error"] = error;
    if (restarting) document["restarting"] = true;

    String json;
    serializeJson(document, json);
    return json;
}

String systemStatusJson(const SystemStatus &status)
{
    JsonDocument document;
    document["cpuTemperatureSupported"] = status.cpuTemperatureSupported;
    if (status.cpuTemperatureSupported)
        document["cpuTemperatureC"] = status.cpuTemperatureC;
    else
        document["cpuTemperatureC"] = nullptr;
    document["totalHeap"] = status.totalHeap;
    document["freeHeap"] = status.freeHeap;

    JsonObject wifi = document["wifi"].to<JsonObject>();
    wifi["connected"] = status.wifiConnected;
    wifi["ssid"] = status.wifiSsid;
    wifi["rssi"] = status.wifiRssi;

    JsonObject sd = document["sd"].to<JsonObject>();
    sd["cardPresent"] = status.sdCardPresent;
    sd["mounted"] = status.sdMounted;
    sd["totalBytes"] = status.sdTotalBytes;
    sd["usedBytes"] = status.sdUsedBytes;
    sd["freeBytes"] = status.sdFreeBytes;

    document["uptimeMs"] = status.uptimeMs;
    document["uptime"] = SystemMonitor::formatUptime(status.uptimeMs);

    String json;
    serializeJson(document, json);
    return json;
}

constexpr const char *kGalleryConfigPath = "/gallery.json";
constexpr const char *kImageDirectory = "/image";
constexpr const char *kWeatherConfigPath = "/weather.json";
constexpr float kMinLatitude = -90.0f;
constexpr float kMaxLatitude = 90.0f;
constexpr float kMinLongitude = -180.0f;
constexpr float kMaxLongitude = 180.0f;
constexpr int32_t kMaxTimezoneSeconds = 14 * 60 * 60;
constexpr size_t kMaxCityLength = 64;

/**
 * @brief Parse and validate a finite floating-point request argument.
 */
bool parseFloatArgument(const String &value, float &result)
{
    if (value.length() == 0) return false;

    char *end = nullptr;
    const float parsed = strtof(value.c_str(), &end);
    if (end == value.c_str() || end == nullptr || *end != '\0' || !std::isfinite(parsed))
        return false;

    result = parsed;
    return true;
}

/**
 * @brief Parse a bounded integer request argument.
 */
bool parseIntegerArgument(const String &value, int32_t &result)
{
    if (value.length() == 0) return false;

    char *end = nullptr;
    const long parsed = strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || end == nullptr || *end != '\0' ||
        parsed < std::numeric_limits<int32_t>::min() ||
        parsed > std::numeric_limits<int32_t>::max())
        return false;

    result = static_cast<int32_t>(parsed);
    return true;
}
} // namespace

WebServerManager webServerManager;

WebServerManager::WebServerManager(SystemMonitor &monitorReference)
    : _monitor(monitorReference),
      _server(80),
      _filesystemReady(false),
      _serverStarted(false)
{
}

bool WebServerManager::begin(const char *mdnsHostname)
{
    if (_serverStarted) return true;

    _mdnsHostname = mdnsHostname == nullptr || mdnsHostname[0] == '\0'
                        ? "photoframe"
                        : mdnsHostname;

    _filesystemReady = LittleFS.begin(false);
    if (!_filesystemReady)
    {
        // Never format automatically: that would erase index.html and wifi.json.
        Serial.println("[LittleFS] Mount failed");
    }

    wifiManager.begin(_filesystemReady, _mdnsHostname.c_str());

    if (_filesystemReady) { loadGallerySettings(); loadWeatherSettings(); }

    registerRoutes();
    _server.begin();
    _serverStarted = true;

    Serial.println("[WebServer] HTTP server started");
    return _filesystemReady;
}

void WebServerManager::update()
{
    wifiManager.update();
    _server.handleClient();
}

void WebServerManager::registerRoutes()
{
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/index.html", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/style.css", HTTP_GET,
                [this]() { handleWebAsset("/style.css", "text/css"); });
    _server.on("/app.js", HTTP_GET,
                [this]() { handleWebAsset("/app.js", "application/javascript"); });
    _server.on("/api/system", HTTP_GET, [this]() { handleSystem(); });
    _server.on("/api/system", HTTP_POST, [this]() { handleSystemPost(); });
    _server.on("/api/wifi/scan", HTTP_GET, [this]() { handleWiFiScan(); });
    _server.on("/api/wifi/connect", HTTP_POST, [this]() { handleWiFiConnect(); });
    _server.on("/api/wifi/reset", HTTP_POST, [this]() { handleWiFiReset(); });
    _server.on("/api/gallery", HTTP_GET, [this]() { handleGallery(); });
    _server.on("/gallery/file", HTTP_GET, [this]() { handleGalleryFile(); });
    _server.on("/api/gallery/upload", HTTP_POST,
            [this]() { handleGalleryUpload(); },      // gọi khi upload xong
            [this]() { handleGalleryUploadData(); });  // gọi liên tục lúc đang nhận chunk dữ liệu
    _server.on("/api/gallery/file", HTTP_DELETE, [this]() { handleGalleryDelete(); });
    _server.onNotFound([this]() { handleNotFound(); });
    _server.on("/api/gallery/interval", HTTP_POST, [this]() { handleGalleryInterval(); });
    _server.on("/api/system/brightness", HTTP_POST, [this]() { handleBrightness(); });
    _server.on("/api/system/reboot", HTTP_POST, [this]() { handleReboot(); });
    _server.on("/api/system/factory-reset", HTTP_POST, [this]() { handleFactoryReset(); });
    _server.on("/api/weather", HTTP_GET, [this]() { handleWeather(); });
    _server.on("/api/weather/current", HTTP_GET, [this]() { handleWeatherCurrent(); });
    _server.on("/api/weather", HTTP_POST, [this]() { handleWeather(); });
}


void WebServerManager::handleRoot()
{
    if (!_filesystemReady || !LittleFS.exists("/index.html"))
    {
        _server.send(404, "text/plain", "index.html not found");
        return;
    }

    File file = LittleFS.open("/index.html", FILE_READ);
    if (!file)
    {
        _server.send(500, "text/plain", "Unable to open index.html");
        return;
    }
    _server.streamFile(file, "text/html");
    file.close();
}

void WebServerManager::handleWebAsset(const char *path, const char *contentType)
{
    if (!_filesystemReady || path == nullptr || contentType == nullptr ||
        !LittleFS.exists(path))
    {
        _server.send(404, "text/plain", "web asset not found");
        return;
    }

    File file = LittleFS.open(path, FILE_READ);
    if (!file)
    {
        _server.send(500, "text/plain", "Unable to open web asset");
        return;
    }

    // Asset names are stable; avoid stale CSS/JS after a LittleFS update.
    _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _server.streamFile(file, contentType);
    file.close();
}

void WebServerManager::handleSystem()
{
    JsonDocument document;
    deserializeJson(document, systemStatusJson(_monitor.status()));
    document["ip"] = wifiManager.currentIp().toString();
    document["mode"] = wifiManager.isApMode() ? "ap" : "sta";
    document["brightness"] = _brightness;

    // Cờ tính năng: đổi true khi route thật đã được viết, frontend tự cập nhật UI, không cần sửa HTML
    JsonObject features = document["features"].to<JsonObject>();
    features["gallery"]       = true;  //DONE
    features["galleryUpload"] = true; // DONE
    features["galleryDelete"] = true; // DONE
    features["slideInterval"] = true; // DONE
    features["brightness"]    = true; // DONE
    features["reboot"]        = true; // DONE
    features["weather"]       = true;
    features["factoryReset"]  = true; // DONE

    String finalJson;
    serializeJson(document, finalJson);
    _server.send(200, "application/json", finalJson);
}

void WebServerManager::handleSystemPost()
{
    const String action = _server.arg("action");
    if (action == "reset_wifi")
    {
        handleWiFiReset();
        return;
    }
    if (_server.hasArg("ssid"))
    {
        handleWiFiConnect();
        return;
    }
    _server.send(400, "application/json", simpleResponseJson(false, "missing action"));
}

void WebServerManager::handleWiFiScan()
{
    const int count = WiFi.scanNetworks();
    JsonDocument document;
    JsonArray networks = document.to<JsonArray>();

    if (count > 0)
    {
        for (int index = 0; index < count; ++index)
        {
            JsonObject network = networks.add<JsonObject>();
            network["ssid"] = WiFi.SSID(index);
            network["rssi"] = WiFi.RSSI(index);
            network["encrypted"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
        }
    }
    WiFi.scanDelete();

    String response;
    serializeJson(document, response);
    _server.send(200, "application/json", response);
}

void WebServerManager::handleWiFiConnect()
{
    const String ssid = _server.arg("ssid");
    const String password = _server.arg("password");
    const bool saved = wifiManager.saveConfiguration(ssid, password);
    if (!saved)
    {
        _server.send(400, "application/json", simpleResponseJson(false, "invalid wifi configuration"));
        return;
    }

    _server.send(200, "application/json", simpleResponseJson(true, nullptr, true));
    delay(300);
    ESP.restart();
}

void WebServerManager::handleWiFiReset()
{
    wifiManager.clearConfiguration();
    wifiManager.disconnect();
    _server.send(200, "application/json", simpleResponseJson(true, nullptr, true));
    delay(300);
    ESP.restart();
}

void WebServerManager::handleNotFound()
{
    // Captive portal probes request arbitrary paths; return the setup page in AP mode.
    if (wifiManager.isApMode() && _server.method() == HTTP_GET)
    {
        handleRoot();
        return;
    }
    _server.send(404, "text/plain", "Not found");
}
/** Add cache */
void WebServerManager::refreshGalleryCache()
{
    _galleryCache.clear();
    if (sd.isReady()) {
        const std::vector<String> files = sd.listFiles(kImageDirectory, nullptr);
        _galleryCache.reserve(files.size());
        for (const String &path : files) {
            String lower = path; lower.toLowerCase();
            if (!lower.endsWith(".jpg") && !lower.endsWith(".jpeg")) continue;
            File file = sd.open(path.c_str(), FILE_READ);
            if (!file) continue;
            _galleryCache.push_back({path, file.size()});
            file.close();
        }
    }
    _galleryCacheReady = true;
}

void WebServerManager::addGalleryCacheEntry(const String &path, size_t size)
{
    if (!_galleryCacheReady) return;
    for (GalleryEntry &entry : _galleryCache) { if (entry.path == path) { entry.sizeBytes = size; return; } }
    _galleryCache.push_back({path, size});
}

void WebServerManager::removeGalleryCacheEntry(const String &path)
{
    if (!_galleryCacheReady) return;
    for (auto it = _galleryCache.begin(); it != _galleryCache.end(); ++it) { if (it->path == path) { _galleryCache.erase(it); return; } }
}


void WebServerManager::handleGallery()
{
    const uint32_t startedAt = millis();
    if (!_galleryCacheReady) refreshGalleryCache();
    JsonDocument document; JsonArray images = document["images"].to<JsonArray>(); uint64_t totalBytes = 0;
    for (const GalleryEntry &entry : _galleryCache) { JsonObject image = images.add<JsonObject>(); image["name"] = entry.path.substring(entry.path.lastIndexOf(47) + 1); image["path"] = entry.path; image["sizeBytes"] = entry.sizeBytes; totalBytes += entry.sizeBytes; }
    document["count"] = images.size(); document["totalBytes"] = totalBytes; document["slideIntervalSec"] = _slideIntervalMs / 1000;
    String json; serializeJson(document, json); _server.send(200, "application/json", json);
}

void WebServerManager::handleGalleryFile()
{
    if (!_server.hasArg("path"))
    {
        _server.send(400, "text/plain", "missing path");
        return;
    }
    String path = _server.arg("path");
    if (path.indexOf("..") >= 0) // chặn path traversal cơ bản
    {
        _server.send(400, "text/plain", "invalid path");
        return;
    }
    if (!path.startsWith("/")) path = "/" + path;

    File file = sd.open(path.c_str(), FILE_READ);
    if (!file || file.isDirectory())
    {
        _server.send(404, "text/plain", "file not found");
        return;
    }
    _server.streamFile(file, "image/jpeg");
    file.close();
}

void WebServerManager::handleGalleryUploadData()
{
    HTTPUpload &upload = _server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        if (_uploadFile) _uploadFile.close();
        _uploadFile = File();
        _uploadOk = false;
        _uploadBytesWritten = 0;
        String filename = upload.filename;
        if (filename.indexOf("..") >= 0) filename = "upload.jpg";
        const int lastSlash = filename.lastIndexOf('/');
        if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);
        filename = String(kImageDirectory) + "/" + filename;
        _uploadPath = filename;
        if (!filename.endsWith(".jpg") && !filename.endsWith(".jpeg"))
        { Serial.println("[Gallery] Reject non-JPG upload"); return; }
        Serial.printf("[Gallery] Upload start: %s\n", filename.c_str());
        _uploadFile = sd.open(filename.c_str(), FILE_WRITE);
        if (!_uploadFile)
        { Serial.println("[Gallery] ERROR: khong mo duoc file de ghi (SD ban/day/loi)"); _uploadOk = false; }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (_uploadFile && upload.buf != nullptr && upload.currentSize > 0)
        {
            const size_t written = _uploadFile.write(upload.buf, upload.currentSize);
            _uploadBytesWritten += written;
            if (written != upload.currentSize)
            { Serial.printf("[Gallery] ERROR: ghi thieu (%u/%u byte)\n", (unsigned)written, (unsigned)upload.currentSize); _uploadOk = false; }
        }
        else if (upload.currentSize > 0) _uploadOk = false;
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (_uploadFile)
        {
            _uploadFile.close();
            _uploadFile = File();
            _uploadOk = (_uploadBytesWritten == upload.totalSize);
            Serial.printf("[Gallery] Upload end: %s, ghi=%u/%u, ok=%d\n", upload.filename.c_str(), (unsigned)_uploadBytesWritten, (unsigned)upload.totalSize, _uploadOk);
        }
        else { Serial.println("[Gallery] Upload end nhung file chua he mo -> that bai"); _uploadOk = false; }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        Serial.println("[Gallery] Upload bi huy giua chung");
        if (_uploadFile) _uploadFile.close();
        _uploadFile = File();
        _uploadOk = false;
        _uploadBytesWritten = 0;
    }
}
void WebServerManager::handleGalleryUpload()
{
    const bool uploadOk = _uploadOk;
    if (uploadOk)
    {
       // gallery.refreshFileList();
        addGalleryCacheEntry(_uploadPath, _uploadBytesWritten);
        _uploadBytesWritten = 0;
        _server.send(200, "application/json", "{\"ok\":true}");
    }
    else _server.send(500, "application/json", "{\"ok\":false,\"error\":\"write failed\"}");
    _uploadOk = false;
}

void WebServerManager::handleGalleryDelete()
{
    const uint32_t startedAt = millis();
    if (!_server.hasArg("path"))
    {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing path\"}");
        return;
    }
    String path = _server.arg("path");
    if (path.indexOf("..") >= 0)
    {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid path\"}");
        return;
    }
    if (!path.startsWith("/")) path = "/" + path;

    const bool removed = sd.remove(path.c_str());
    if (removed) removeGalleryCacheEntry(path);
    //if (removed) gallery.refreshFileList();

    _server.send(removed ? 200 : 404, "application/json",
                 removed ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"not found\"}");
}

void WebServerManager::loadGallerySettings()
{
    _slideIntervalMs = 5000;
    if (!_filesystemReady || !LittleFS.exists(kGalleryConfigPath)) return;

    File file = LittleFS.open(kGalleryConfigPath, FILE_READ);
    if (!file) return;

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error) return;

    const int seconds = document["slideIntervalSec"] | 5;
    if (seconds >= 1 && seconds <= 3600) _slideIntervalMs = static_cast<uint32_t>(seconds) * 1000UL;
}

bool WebServerManager::saveGallerySettings()
{
    if (!_filesystemReady) return false;
    File file = LittleFS.open(kGalleryConfigPath, FILE_WRITE);
    if (!file) return false;

    JsonDocument document;
    document["slideIntervalSec"] = _slideIntervalMs / 1000;
    document["brightness"] = _brightness;
    const size_t written = serializeJson(document, file);
    file.close();
    return written > 0;
}

void WebServerManager::handleGalleryInterval()
{
    if (!_server.hasArg("seconds"))
    {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing seconds\"}");
        return;
    }
    const int seconds = _server.arg("seconds").toInt();
    if (seconds < 1 || seconds > 3600)
    {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"seconds out of range 1-3600\"}");
        return;
    }
    _slideIntervalMs = static_cast<uint32_t>(seconds) * 1000UL;
    saveGallerySettings();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebServerManager::handleBrightness()
{
    if (!_server.hasArg("value"))
    {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing value\"}");
        return;
    }
    const int value = _server.arg("value").toInt();
    if (value < 0 || value > 255)
    {
        _server.send(400, "application/json", "{\"ok\":false,\"error\":\"value out of range 0-255\"}");
        return;
    }
    tft.setBrightness(static_cast<uint8_t>(value));
    _brightness = static_cast<uint8_t>(value);
    saveGallerySettings();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebServerManager::handleReboot()
{
    _server.send(200, "application/json", simpleResponseJson(true, nullptr, true));
    delay(300);
    ESP.restart();
}

void WebServerManager::handleFactoryReset()
{
    wifiManager.clearConfiguration();
    wifiManager.disconnect();

    if (_filesystemReady) {
        LittleFS.remove(kGalleryConfigPath);
    }

    _server.send(200, "application/json", simpleResponseJson(true, nullptr, true));
    delay(300);
    ESP.restart();
}
/**
 * @brief Load persisted weather location settings from LittleFS.
 */
void WebServerManager::loadWeatherSettings()
{
    _weatherConfig = WeatherConfig();
    if (!_filesystemReady || !LittleFS.exists(kWeatherConfigPath)) return;

    File file = LittleFS.open(kWeatherConfigPath, FILE_READ);
    if (!file) return;

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error) return;

    const float latitude = document["latitude"] | 0.0f;
    const float longitude = document["longitude"] | 0.0f;
    const int32_t timezoneSeconds = document["timezoneSeconds"] | 0;
    const String city = document["city"] | "";
    if (!std::isfinite(latitude) || !std::isfinite(longitude) ||
        latitude < kMinLatitude || latitude > kMaxLatitude ||
        longitude < kMinLongitude || longitude > kMaxLongitude ||
        timezoneSeconds < -kMaxTimezoneSeconds ||
        timezoneSeconds > kMaxTimezoneSeconds || city.length() > kMaxCityLength)
        return;

    _weatherConfig.latitude = latitude;
    _weatherConfig.longitude = longitude;
    _weatherConfig.timezoneSeconds = timezoneSeconds;
    _weatherConfig.city = city;
    _weatherConfig.revision++;
}

/**
 * @brief Persist weather location settings to LittleFS.
 */
bool WebServerManager::saveWeatherSettings()
{
    if (!_filesystemReady) return false;

    File file = LittleFS.open(kWeatherConfigPath, FILE_WRITE);
    if (!file) return false;

    JsonDocument document;
    document["latitude"] = _weatherConfig.latitude;
    document["longitude"] = _weatherConfig.longitude;
    document["timezoneSeconds"] = _weatherConfig.timezoneSeconds;
    document["city"] = _weatherConfig.city;
    const size_t written = serializeJson(document, file);
    file.close();
    return written > 0;
}

/**
 * @brief Return or update the persisted Open-Meteo location configuration.
 */
void WebServerManager::handleWeather()
{
    if (_server.method() == HTTP_GET)
    {
        JsonDocument document;
        document["latitude"] = _weatherConfig.latitude;
        document["longitude"] = _weatherConfig.longitude;
        document["timezoneSeconds"] = _weatherConfig.timezoneSeconds;
        document["city"] = _weatherConfig.city;

        String response;
        serializeJson(document, response);
        _server.send(200, "application/json", response);
        return;
    }

    if (!_server.hasArg("lat") || !_server.hasArg("lon") ||
        !_server.hasArg("tz") || !_server.hasArg("city"))
    {
        _server.send(400, "application/json",
                     simpleResponseJson(false, "missing weather configuration"));
        return;
    }

    float latitude = 0.0f;
    float longitude = 0.0f;
    int32_t timezoneSeconds = 0;
    const String city = _server.arg("city");
    if (!parseFloatArgument(_server.arg("lat"), latitude) ||
        !parseFloatArgument(_server.arg("lon"), longitude) ||
        !parseIntegerArgument(_server.arg("tz"), timezoneSeconds) ||
        latitude < kMinLatitude || latitude > kMaxLatitude ||
        longitude < kMinLongitude || longitude > kMaxLongitude ||
        timezoneSeconds < -kMaxTimezoneSeconds ||
        timezoneSeconds > kMaxTimezoneSeconds || city.length() == 0 ||
        city.length() > kMaxCityLength)
    {
        _server.send(400, "application/json",
                     simpleResponseJson(false, "invalid weather configuration"));
        return;
    }

    const WeatherConfig previous = _weatherConfig;
    _weatherConfig.latitude = latitude;
    _weatherConfig.longitude = longitude;
    _weatherConfig.timezoneSeconds = timezoneSeconds;
    _weatherConfig.city = city;
    _weatherConfig.revision++;

    if (!_filesystemReady || !saveWeatherSettings())
    {
        _weatherConfig = previous;
        _server.send(500, "application/json",
                     simpleResponseJson(false, "unable to save weather configuration"));
        return;
    }

    _server.send(200, "application/json", simpleResponseJson(true));
}
/**
 * @brief Return the latest fetched current-weather reading.
 */
void WebServerManager::handleWeatherCurrent()
{
    const Weather::Data &data = Weather::data();

    JsonDocument document;
    document["valid"] = data.weatherValid;
    document["temperature"] = data.temperature;
    document["weatherCode"] = data.weatherCode;
    document["city"] = _weatherConfig.city;
    document["time"] = data.currentTime;
    document["timeValid"] = data.timeValid;

    String json;
    serializeJson(document, json);
    _server.send(200, "application/json", json);
}


