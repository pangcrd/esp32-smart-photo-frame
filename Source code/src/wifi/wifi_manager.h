#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>

// Quản lý toàn bộ vòng đời WiFi: kết nối STA, fallback AP, lưu/đọc config
// trong LittleFS, mDNS, và captive-portal DNS. Không đụng tới WebServer/route.
class WiFiManager
{
public:
    
    WiFiManager();

    // filesystemReady: LittleFS đã mount thành công hay chưa (caller tự mount,
    // WiFiManager không sở hữu filesystem để tránh phụ thuộc thứ tự khởi tạo).
    void begin(bool filesystemReady, const char *mdnsHostname = "photoframe");
    void update(); // gọi trong loop(): xử lý DNS (AP mode) + timeout STA connect

    bool connect();
    bool connect(const String &ssid, const String &password);
    void disconnect();
    bool isConnected() const;

    bool saveConfiguration();
    bool saveConfiguration(const String &ssid, const String &password);
    bool loadConfiguration();
    bool clearConfiguration();

    bool isApMode() const { return _apMode; }
    const String &ssid() const { return _ssid; }
    const char *apSsid() const { return kApSsid; }
    static const char *const kApSsid;
    IPAddress currentIp() const;

    bool resetToApMode();
    bool hardwareWifiReset();
    bool filesystemReady() const { return _filesystemReady; }

private:
    static constexpr uint32_t kStaConnectTimeoutMs = 10000; // 10s theo yêu cầu
    

    void registerWiFiEvents();
    static void onWiFiEvent(WiFiEvent_t event);
    void startAPFallback();
    void startMdns();

    bool _filesystemReady = false;
    String _ssid;
    String _password;
    String _mdnsHostname;
    bool _mdnsStarted = false;
    bool _wifiEventsRegistered = false;

    bool _apMode = false;
    bool _staConnecting = false;
    uint32_t _staConnectStartMs = 0;

    static WiFiManager *_activeInstance;
    DNSServer _dnsServer;
};

extern WiFiManager wifiManager;
