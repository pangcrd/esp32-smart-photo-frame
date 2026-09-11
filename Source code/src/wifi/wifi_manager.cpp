#include "wifi_manager.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

namespace
{
constexpr const char *kWifiConfigPath = "/wifi.json";
} // namespace

WiFiManager wifiManager;
WiFiManager *WiFiManager::_activeInstance = nullptr;
const char *const WiFiManager::kApSsid = "PhotoFrame-Setup";

WiFiManager::WiFiManager() {}

void WiFiManager::begin(bool filesystemReady, const char *mdnsHostname)
{
    _filesystemReady = filesystemReady;
    _mdnsHostname = mdnsHostname == nullptr || mdnsHostname[0] == '\0'
                        ? "photoframe"
                        : mdnsHostname;

    registerWiFiEvents();

    if (_filesystemReady) loadConfiguration();

    if (_ssid.length() > 0)
    {
        // Có config sẵn -> thử STA, có timeout fallback trong update()
        connect();
    }
    else
    {
        // Chưa từng cấu hình -> phát AP luôn, không đụng STA
        startAPFallback();
    }

    startMdns(); // chỉ start khi STA hoặc AP đã có IP hợp lệ
}

void WiFiManager::update()
{
    if (_apMode) _dnsServer.processNextRequest();

    if (_staConnecting && !_apMode)
    {
        if (isConnected())
        {
            _staConnecting = false;
        }
        else if (millis() - _staConnectStartMs >= kStaConnectTimeoutMs)
        {
            Serial.println("[WiFi] STA connect timeout (10s), falling back to AP");
            startAPFallback();
        }
    }
}

void WiFiManager::registerWiFiEvents()
{
    if (_wifiEventsRegistered) return;

    _activeInstance = this;
    WiFi.onEvent(&WiFiManager::onWiFiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    _wifiEventsRegistered = true;
}

void WiFiManager::onWiFiEvent(WiFiEvent_t event)
{
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP && _activeInstance != nullptr)
        _activeInstance->startMdns();
}

bool WiFiManager::connect()
{
    if (_ssid.length() == 0) return false;

    registerWiFiEvents();
    if (_apMode) _dnsServer.stop();
    _apMode = false;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    _staConnecting = true;
    _staConnectStartMs = millis();
    return true;
}

void WiFiManager::startAPFallback()
{
    if (_apMode) return;

    if (_mdnsStarted)
    {
        MDNS.end();
        _mdnsStarted = false;
    }
    WiFi.disconnect(true);
    // Keep the AP alive while scanning nearby networks.
    WiFi.mode(WIFI_AP_STA);
    const IPAddress apIp(192, 168, 4, 1);
    const IPAddress apGateway(192, 168, 4, 1);
    const IPAddress apSubnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, apGateway, apSubnet);
    WiFi.softAP(kApSsid);
    _apMode = true;
    _staConnecting = false;

    _dnsServer.start(53, "*", apIp);
    startMdns();

    Serial.printf("[WiFi] AP fallback: SSID=%s IP=%s\n",
                  kApSsid, WiFi.softAPIP().toString().c_str());
}

bool WiFiManager::connect(const String &ssid, const String &password)
{
    if (ssid.length() == 0) return false;
    _ssid = ssid;
    _password = password;
    return connect();
}

void WiFiManager::disconnect()
{
    if (_mdnsStarted)
    {
        MDNS.end();
        _mdnsStarted = false;
    }
    WiFi.disconnect(true);
}

bool WiFiManager::isConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiManager::currentIp() const
{
    return _apMode ? WiFi.softAPIP() : WiFi.localIP();
}

bool WiFiManager::saveConfiguration()
{
    if (!_filesystemReady || _ssid.length() == 0) return false;

    File file = LittleFS.open(kWifiConfigPath, FILE_WRITE);
    if (!file) return false;

    JsonDocument document;
    document["ssid"] = _ssid;
    document["password"] = _password;

    const size_t written = serializeJson(document, file);
    file.close();
    return written > 0;
}

bool WiFiManager::saveConfiguration(const String &ssid, const String &password)
{
    if (ssid.length() == 0) return false;
    _ssid = ssid;
    _password = password;
    return saveConfiguration();
}

bool WiFiManager::loadConfiguration()
{
    _ssid = String();
    _password = String();
    if (!_filesystemReady || !LittleFS.exists(kWifiConfigPath)) return false;

    File file = LittleFS.open(kWifiConfigPath, FILE_READ);
    if (!file) return false;

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error) return false;

    const char *ssid = document["ssid"] | "";
    const char *password = document["password"] | "";
    if (ssid[0] == '\0') return false;

    _ssid = ssid;
    _password = password;
    return true;
}

bool WiFiManager::clearConfiguration()
{
    const bool removed = _filesystemReady &&
                         (!LittleFS.exists(kWifiConfigPath) || LittleFS.remove(kWifiConfigPath));
    _ssid = String();
    _password = String();
    return removed;
}

/**
 * @brief Clear saved WiFi credentials and switch immediately to setup AP mode.
 */
bool WiFiManager::resetToApMode()
{
        if (!clearConfiguration()) return false;
    return hardwareWifiReset();
}

/**
 * @brief Stop station mode and start the setup AP without using the web server.
 */
bool WiFiManager::hardwareWifiReset()
{
    if (_apMode) {
        _dnsServer.stop();
        _apMode = false;
    }
    disconnect();
    _apMode = false;
    _staConnecting = false;
    startAPFallback();

    const IPAddress apIp = WiFi.softAPIP();
    return _apMode &&
           (apIp[0] != 0 || apIp[1] != 0 || apIp[2] != 0 || apIp[3] != 0);
}


void WiFiManager::startMdns()
{
    if (_mdnsStarted) return;

    // STA mDNS waits for GOT_IP; AP mDNS is safe after softAPConfig/softAP.
    IPAddress ip(0, 0, 0, 0);
    if (isConnected())
        ip = WiFi.localIP();
    else if (_apMode)
        ip = WiFi.softAPIP();
    else
        return;

    const bool hasIp = !(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
    if (!hasIp) return;

    _mdnsStarted = MDNS.begin(_mdnsHostname.c_str());
    if (_mdnsStarted) MDNS.addService("http", "tcp", 80);
    if (_mdnsStarted)
    {
        Serial.printf("[mDNS] http://%s.local\n", _mdnsHostname.c_str());
    }
}
