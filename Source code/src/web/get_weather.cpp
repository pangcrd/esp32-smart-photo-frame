#include "get_api_data.h"

/**
 * @file get_weather.cpp
 * @brief Open-Meteo HTTP request/response handling, plus the shared
 *        weather+NTP state-machine orchestration (begin/update/data).
 *
 * Split out of the original open_meteo_weather_cfg.h — logic is unchanged,
 * only the module boundaries and comments are new.
 *
 * NOTE on why begin()/update()/data() live here and not in get_ntp.cpp:
 * update() is a single state machine that drives both the NTP flow
 * (WAIT_TIME, via startTimeSync()/updateClock() from get_ntp.cpp) and the
 * weather flow (START_WEATHER/WAIT_WEATHER, implemented below). It isn't
 * "weather-only" or "ntp-only" logic — it's the glue between the two — so
 * it was kept as one function here instead of being duplicated or split
 * in a way that would change behavior.
 */
namespace Weather
{
// ---------------------------------------------------------------------
// Shared global state.
// NOTE: this is the single definition point for every global declared
// `extern` in get_api_data.h. Defining them here (rather than in the
// header) avoids duplicate-symbol linker errors now that the header is
// included from two .cpp files.
// ---------------------------------------------------------------------
State state = State::INIT;
Data currentData;
WiFiClientSecure weatherClient;
String responseBuffer;
String requestUrl;
WebServerManager::WeatherConfig requestConfig;
uint32_t observedConfigRevision = 0;
uint32_t lastSeenConfigRevision = 0;
uint32_t lastNtpRequestMs = 0;
uint32_t ntpStartedMs = 0;
uint32_t lastWeatherSuccessMs = 0;
uint32_t nextRetryMs = 0;
uint32_t weatherRequestStartedMs = 0;
size_t expectedResponseLength = 0;
bool responseHeadersComplete = false;
bool responseStatusOk = false;
bool responseChunked = false;
bool responseContentLengthKnown = false;
bool weatherRequestActive = false;
bool started = false;

/**
 * @brief Convert a WMO weather code to one of the five available icon indexes.
 * NOTE: bucket boundaries follow the WMO weather_code table (clear / cloudy
 *       / drizzle / rain-or-snow / storm). Keep in sync with the UI icon set
 *       if that table ever changes.
 */
uint8_t iconIndexForCode(int code)
{
    if (code <= 1) return 1;
    if (code == 2 || code == 3 || code == 45 || code == 48) return 2;
    if (code >= 51 && code <= 57) return 3;
    if ((code >= 61 && code <= 67) || (code >= 71 && code <= 77) ||
        (code >= 80 && code <= 82) || (code >= 85 && code <= 86)) return 4;
    if (code >= 95 && code <= 99) return 5;
    return 2;
}

/**
 * @brief Format the configured UTC offset for the Open-Meteo request.
 * NOTE: result is URL-encoded ("+" -> %2B, ":" -> %3A) since it is spliced
 *       directly into the request query string.
 */
String formatApiTimezone(int32_t timezoneSeconds)
{
    const bool negative = timezoneSeconds < 0;
    const uint32_t absoluteSeconds = negative
                                          ? static_cast<uint32_t>(-(timezoneSeconds + 1)) + 1U
                                          : static_cast<uint32_t>(timezoneSeconds);
    const uint32_t hours = absoluteSeconds / 3600U;
    const uint32_t minutes = (absoluteSeconds % 3600U) / 60U;

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "GMT%c%02ld:%02ld", negative ? '-' : '+',
             static_cast<long>(hours), static_cast<long>(minutes));

    String encoded = buffer;
    encoded.replace("+", "%2B");
    encoded.replace(":", "%3A");
    return encoded;
}

/**
 * @brief Build the current-weather Open-Meteo URL from a configuration snapshot.
 * NOTE: only requests `temperature_2m,weather_code` — add fields here if the
 *       UI ever needs more than temperature + condition code.
 */
String buildWeatherUrl(const WebServerManager::WeatherConfig &config)
{
    String url = "https://";
    url += kWeatherHost;
    url += "/v1/forecast?latitude=";
    url += String(config.latitude, 5);
    url += "&longitude=";
    url += String(config.longitude, 5);
    url += "&current=temperature_2m,weather_code&timezone=";
    url += formatApiTimezone(config.timezoneSeconds);
    return url;
}

/**
 * @brief Reset the raw HTTP response parser for a new request.
 * NOTE: must be called before every startWeatherRequest() so stale
 *       header/content-length state from the previous request can't leak
 *       into the next one.
 */
void resetResponseParser()
{
    responseBuffer = String();
    responseBuffer.reserve(kResponseBufferLimit);
    expectedResponseLength = 0;
    responseHeadersComplete = false;
    responseStatusOk = false;
    responseChunked = false;
    responseContentLengthKnown = false;
}

/**
 * @brief Extract the HTTP status and content length from received headers.
 * NOTE: minimal hand-rolled HTTP/1.x header parser — only looks at the
 *       status line, Content-Length, and Transfer-Encoding.
 */
bool parseResponseHeaders()
{
    if (responseHeadersComplete) return true;

    const int headerEnd = responseBuffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) return false;

    const String headers = responseBuffer.substring(0, headerEnd);
    const int firstLineEnd = headers.indexOf("\r\n");
    const String statusLine = firstLineEnd < 0 ? headers : headers.substring(0, firstLineEnd);
    responseStatusOk = statusLine.startsWith("HTTP/1.1 200 ") ||
                       statusLine.startsWith("HTTP/1.0 200 ");

    expectedResponseLength = 0;
    int lineStart = firstLineEnd < 0 ? headers.length() : firstLineEnd + 2;
    while (lineStart < headers.length())
    {
        const int lineEnd = headers.indexOf("\r\n", lineStart);
        const int end = lineEnd < 0 ? headers.length() : lineEnd;
        const String line = headers.substring(lineStart, end);
        const int colon = line.indexOf(':');
        if (colon > 0)
        {
            String name = line.substring(0, colon);
            name.toLowerCase();
            if (name == "content-length")
            {
                const String value = line.substring(colon + 1);
                char *valueEnd = nullptr;
                const unsigned long parsed = strtoul(value.c_str(), &valueEnd, 10);
                if (valueEnd != value.c_str())
                {
                    expectedResponseLength = static_cast<size_t>(parsed);
                    responseContentLengthKnown = true;
                }
            }
            else if (name == "transfer-encoding")
            {
                String value = line.substring(colon + 1);
                value.toLowerCase();
                responseChunked = value.indexOf("chunked") >= 0;
            }
        }
        if (lineEnd < 0) break;
        lineStart = lineEnd + 2;
    }

    responseHeadersComplete = true;
    return true;
}

/**
 * @brief Parse a complete Open-Meteo response and publish valid weather data.
 * NOTE: bails out (returns false, nothing published) on chunked transfer
 *       encoding, malformed/incomplete JSON, missing fields, or a config
 *       revision that changed while this request was in flight.
 */
bool parseWeatherResponse()
{
    if (!responseHeadersComplete || !responseStatusOk || responseChunked)
        return false;

    if (responseContentLengthKnown && expectedResponseLength == 0) return false;

    const int headerEnd = responseBuffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) return false;

    if (responseBuffer.length() < static_cast<size_t>(headerEnd + 4))
        return false;
    const size_t bodyStart = static_cast<size_t>(headerEnd + 4);
    const size_t bodyLength = responseBuffer.length() - bodyStart;
    if (responseContentLengthKnown && bodyLength < expectedResponseLength)
        return false;

    const size_t bodyEnd = responseContentLengthKnown
                               ? bodyStart + expectedResponseLength
                               : responseBuffer.length();
    String body = responseBuffer.substring(
        static_cast<unsigned int>(bodyStart),
        static_cast<unsigned int>(bodyEnd));
    JsonDocument document;
    if (deserializeJson(document, body)) return false;

    JsonObjectConst current = document["current"].as<JsonObjectConst>();
    if (current.isNull() || current["temperature_2m"].isNull() ||
        current["weather_code"].isNull()) return false;

    const float temperature = current["temperature_2m"].as<float>();
    const int code = current["weather_code"].as<int>();
    if (!std::isfinite(temperature) || code < 0) return false;

    if (requestConfig.revision != observedConfigRevision) return false;

    currentData.temperature = temperature;
    currentData.weatherCode = code;
    currentData.weatherValid = true;
    currentData.configRevision = requestConfig.revision;
    return true;
}

/**
 * @brief Start a raw HTTP request without blocking for its response body.
 * NOTE: uses HTTP/1.0 + "Connection: close" on purpose, so the server
 *       closes the socket at the end of the body — readWeatherResponse()
 *       relies on that to detect "response complete" when Content-Length
 *       isn't known.
 */
bool startWeatherRequest(uint32_t now)
{
    if (!wifiManager.isConnected()) return false;

    requestConfig = webServerManager.weatherConfig();
    observedConfigRevision = requestConfig.revision;
    requestUrl = buildWeatherUrl(requestConfig);
    resetResponseParser();

    weatherClient.stop();
    weatherClient.setInsecure();
    if (!weatherClient.connect(kWeatherHost, kWeatherPort, 1000)) return false;

    weatherClient.print(String("GET ") + requestUrl.substring(String("https://").length() + strlen(kWeatherHost)) +
                       " HTTP/1.0\r\nHost: " + kWeatherHost +
                       "\r\nConnection: close\r\n\r\n");
    weatherRequestStartedMs = now;
    weatherRequestActive = true;
    state = State::WAIT_WEATHER;
    return true;
}

/**
 * @brief Read currently available HTTP bytes and finish a complete response.
 * NOTE: called repeatedly from the WAIT_WEATHER state; caps each call to
 *       32 bytes read so it stays non-blocking. kResponseBufferLimit and
 *       kWeatherRequestTimeoutMs are safety nets against a runaway/stuck
 *       response.
 */
bool readWeatherResponse(uint32_t now)
{
    if (!weatherRequestActive) return false;

    for (uint8_t count = 0; count < 32 && weatherClient.available(); ++count)
    {
        const int value = weatherClient.read();
        if (value < 0) break;
        if (responseBuffer.length() >= kResponseBufferLimit - 1)
        {
            weatherClient.stop();
            weatherRequestActive = false;
            nextRetryMs = now + kRetryIntervalMs;
            state = State::WAIT_WIFI;
            return false;
        }
        responseBuffer += static_cast<char>(value);
    }

    if (!responseHeadersComplete) parseResponseHeaders();

    bool complete = false;
    if (responseHeadersComplete && responseContentLengthKnown)
    {
        const int headerEnd = responseBuffer.indexOf("\r\n\r\n");
        complete = headerEnd >= 0 &&
                   responseBuffer.length() - static_cast<size_t>(headerEnd + 4) >= expectedResponseLength;
    }
    else if (!responseChunked && !responseContentLengthKnown &&
             !weatherClient.connected() && responseHeadersComplete)
    {
        complete = true;
    }

    if (complete)
    {
        const bool parsed = parseWeatherResponse();
        weatherClient.stop();
        weatherRequestActive = false;
        if (parsed)
        {
            lastWeatherSuccessMs = now;
            nextRetryMs = 0;
            state = State::WAIT_INTERVAL;
        }
        else
        {
            nextRetryMs = now + kRetryIntervalMs;
            state = State::WAIT_WIFI;
        }
        return true;
    }

    if (!wifiManager.isConnected() || now - weatherRequestStartedMs >= kWeatherRequestTimeoutMs)
    {
        weatherClient.stop();
        weatherRequestActive = false;
        nextRetryMs = now + kRetryIntervalMs;
        state = State::WAIT_WIFI;
    }
    return false;
}

/**
 * @brief Initialize the weather state machine and clear transient request state.
 * NOTE: also resets NTP-related timing fields (lastNtpRequestMs, ntpStartedMs)
 *       since begin() owns the whole state machine, not just the weather half.
 */
void begin()
{
    state = State::INIT;
    currentData = Data();
    weatherClient.stop();
    responseBuffer = String();
    requestUrl = String();
    requestConfig = WebServerManager::WeatherConfig();
    observedConfigRevision = 0;
    lastSeenConfigRevision = 0;
    lastNtpRequestMs = 0;
    ntpStartedMs = 0;
    lastWeatherSuccessMs = 0;
    nextRetryMs = 0;
    weatherRequestActive = false;
    responseHeadersComplete = false;
    responseStatusOk = false;
    responseChunked = false;
    responseContentLengthKnown = false;
    expectedResponseLength = 0;
    started = true;
}

/**
 * @brief Execute one non-blocking weather and time state-machine step.
 * NOTE: call once per loop() tick. This is the one place that ties
 *       get_ntp.cpp's startTimeSync()/updateClock() together with this
 *       file's weather request handling — see the file-level note above.
 */
void update()
{
    if (!started) begin();

    const uint32_t now = millis();
    const WebServerManager::WeatherConfig config = webServerManager.weatherConfig();
    currentData.configRevision = config.revision;
    if (lastSeenConfigRevision != 0 && config.revision != lastSeenConfigRevision)
    {
        if (weatherRequestActive) weatherClient.stop();
        weatherRequestActive = false;
        currentData.weatherValid = false;
        lastWeatherSuccessMs = 0;
        nextRetryMs = now;
        if (wifiManager.isConnected())
        {
            startTimeSync(now, config);
        }
        else
        {
            state = State::WAIT_WIFI;
        }
    }
    lastSeenConfigRevision = config.revision;

    updateClock();

    switch (state)
    {
    case State::INIT:
        if (wifiManager.isConnected()) startTimeSync(now, config);
        else state = State::WAIT_WIFI;
        return;

    case State::WAIT_WIFI:
        if (!wifiManager.isConnected() || !deadlineReached(now, nextRetryMs)) return;
        if (!currentData.timeValid || now - lastNtpRequestMs >= kTimeUpdateIntervalMs)
            startTimeSync(now, config);
        else if (lastWeatherSuccessMs == 0 || now - lastWeatherSuccessMs >= kWeatherUpdateIntervalMs)
            state = State::START_WEATHER;
        return;

    case State::WAIT_TIME:
        if (currentData.timeValid)
        {
            if (lastWeatherSuccessMs == 0 || now - lastWeatherSuccessMs >= kWeatherUpdateIntervalMs)
                state = State::START_WEATHER;
            else
                state = State::WAIT_INTERVAL;
        }
        else if (now - ntpStartedMs >= kNtpWaitTimeoutMs)
        {
            nextRetryMs = now + kRetryIntervalMs;
            state = State::WAIT_WIFI;
        }
        return;

    case State::START_WEATHER:
        if (!wifiManager.isConnected())
        {
            state = State::WAIT_WIFI;
            return;
        }
        if (!startWeatherRequest(now))
        {
            nextRetryMs = now + kRetryIntervalMs;
            state = State::WAIT_WIFI;
        }
        return;

    case State::WAIT_WEATHER:
        readWeatherResponse(now);
        return;

    case State::WAIT_INTERVAL:
        if (!wifiManager.isConnected())
        {
            state = State::WAIT_WIFI;
            nextRetryMs = now + kRetryIntervalMs;
            return;
        }
        if (now - lastNtpRequestMs >= kTimeUpdateIntervalMs)
        {
            startTimeSync(now, config);
            return;
        }
        if (lastWeatherSuccessMs == 0 || now - lastWeatherSuccessMs >= kWeatherUpdateIntervalMs)
            state = State::START_WEATHER;
        return;
    }
}

/**
 * @brief Return the latest published time and weather snapshot.
 */
const Data &data()
{
    return currentData;
}

} // namespace Weather
