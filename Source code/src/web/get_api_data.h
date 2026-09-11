#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <time.h>

#include "web/web_server_manager.h"
#include "wifi/wifi_manager.h"

/**
 * @brief Shared types, constants, global state, and function declarations
 *        for the non-blocking Open-Meteo weather + NTP time state machine.
 *
 * This header is included by both get_weather.cpp (Open-Meteo HTTP request
 * handling + state-machine orchestration) and get_ntp.cpp (NTP time sync +
 * clock formatting). This is a pure module split: no logic was changed vs.
 * the original open_meteo_weather_cfg.h, only where each piece lives.
 */
namespace Weather
{
// ---------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------
constexpr const char *kNtpServer = "time.cloudflare.com";
constexpr const char *kWeatherHost = "api.open-meteo.com";
constexpr uint16_t kWeatherPort = 443;
constexpr uint32_t kTimeUpdateIntervalMs = 60UL * 1000UL;
constexpr uint32_t kWeatherUpdateIntervalMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kNtpWaitTimeoutMs = 15UL * 1000UL;
constexpr uint32_t kWeatherRequestTimeoutMs = 12UL * 1000UL;
constexpr uint32_t kRetryIntervalMs = 10UL * 1000UL;
constexpr size_t kResponseBufferLimit = 8192;

// ---------------------------------------------------------------------
// Shared types
// ---------------------------------------------------------------------
struct Data
{
    String currentTime;
    String currentDayMonth;
    int temperature;
    int weatherCode = -1;
    bool timeValid = false;
    bool weatherValid = false;
    uint32_t configRevision = 0;
};

enum class State
{
    INIT,
    WAIT_WIFI,
    WAIT_TIME,
    START_WEATHER,
    WAIT_WEATHER,
    WAIT_INTERVAL,
};

// ---------------------------------------------------------------------
// Shared global state.
// NOTE: defined exactly once, in get_weather.cpp (see the comment there).
// Declared here as `extern` so both .cpp files (and any external caller,
// e.g. UI code reading Weather::data()) see the same instances — same
// effect as the original `inline` globals when everything lived in one
// header-only file.
// ---------------------------------------------------------------------
extern State state;
extern Data currentData;
extern WiFiClientSecure weatherClient;
extern String responseBuffer;
extern String requestUrl;
extern WebServerManager::WeatherConfig requestConfig;
extern uint32_t observedConfigRevision;
extern uint32_t lastSeenConfigRevision;
extern uint32_t lastNtpRequestMs;
extern uint32_t ntpStartedMs;
extern uint32_t lastWeatherSuccessMs;
extern uint32_t nextRetryMs;
extern uint32_t weatherRequestStartedMs;
extern size_t expectedResponseLength;
extern bool responseHeadersComplete;
extern bool responseStatusOk;
extern bool responseChunked;
extern bool responseContentLengthKnown;
extern bool weatherRequestActive;
extern bool started;

// ---------------------------------------------------------------------
// Small shared helper — trivial enough to stay inline in the common
// header so both .cpp files can use it without a separate definition.
// ---------------------------------------------------------------------

/**
 * @brief Return true when an unsigned millisecond deadline has elapsed.
 * NOTE: wraparound-safe millis() comparison; used by both the weather
 *       retry logic and (indirectly) the time-sync flow.
 */
static inline bool deadlineReached(uint32_t now, uint32_t deadline)
{
    return static_cast<int32_t>(now - deadline) >= 0;
}

// ---------------------------------------------------------------------
// Weather-related functions -> implemented in get_weather.cpp
// ---------------------------------------------------------------------

/// @brief Convert a WMO weather code to one of the five available icon indexes.
uint8_t iconIndexForCode(int code);

/// @brief Format the configured UTC offset for the Open-Meteo request.
String formatApiTimezone(int32_t timezoneSeconds);

/// @brief Build the current-weather Open-Meteo URL from a configuration snapshot.
String buildWeatherUrl(const WebServerManager::WeatherConfig &config);

/// @brief Reset the raw HTTP response parser for a new request.
void resetResponseParser();

/// @brief Extract the HTTP status and content length from received headers.
bool parseResponseHeaders();

/// @brief Parse a complete Open-Meteo response and publish valid weather data.
bool parseWeatherResponse();

/// @brief Start a raw HTTP request without blocking for its response body.
bool startWeatherRequest(uint32_t now);

/// @brief Read currently available HTTP bytes and finish a complete response.
bool readWeatherResponse(uint32_t now);

// ---------------------------------------------------------------------
// NTP / time-related functions -> implemented in get_ntp.cpp
// ---------------------------------------------------------------------

/// @brief Start asynchronous NTP configuration without waiting for the network.
void startTimeSync(uint32_t now, const WebServerManager::WeatherConfig &config);

/// @brief Update formatted local time when the system clock is available.
bool updateClock();

// ---------------------------------------------------------------------
// State-machine orchestration -> implemented in get_weather.cpp
// NOTE: begin()/update()/data() drive BOTH the NTP and weather flows
// (see the WAIT_TIME / START_WEATHER / WAIT_WEATHER states). They are
// kept together in one place rather than split, since splitting them
// would mean duplicating the state machine itself. See get_weather.cpp
// for the full rationale.
// ---------------------------------------------------------------------

/// @brief Initialize the weather state machine and clear transient request state.
void begin();

/// @brief Execute one non-blocking weather and time state-machine step.
void update();

/// @brief Return the latest published time and weather snapshot.
const Data &data();

} // namespace Weather
