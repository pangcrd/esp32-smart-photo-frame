#include "get_api_data.h"

namespace Weather
{
/**
 * @brief Start asynchronous NTP configuration without waiting for the network.
 * NOTE: configTime() is fire-and-forget — success/failure of the sync is
 *       only observed later, when updateClock() sees a plausible system
 *       time (or WAIT_TIME times out after kNtpWaitTimeoutMs).
 */
void startTimeSync(uint32_t now, const WebServerManager::WeatherConfig &config)
{
    configTime(config.timezoneSeconds, 0, kNtpServer);
    lastNtpRequestMs = now;
    ntpStartedMs = now;
    currentData.timeValid = false;
    state = State::WAIT_TIME;
}

bool updateClock()
{
// vietnamese weekday
    static const char* const wdayVN[7] = {
    "CN", "T2", "T3", "T4", "T5", "T6", "T7"
};

    struct tm timeInfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeInfo);
    if (timeInfo.tm_year <= (2016 - 1900)) return false;

    char timeBuffer[8];
    char dateBuffer[24];
    //strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", &timeInfo);
    bool colonOn = (timeInfo.tm_sec % 2) == 0;
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d%c%02d",
             timeInfo.tm_hour, colonOn ? ':' : ' ', timeInfo.tm_min);

    snprintf(dateBuffer, sizeof(dateBuffer), "%s, %02d Thg %d",
         wdayVN[timeInfo.tm_wday],
         timeInfo.tm_mday,
         timeInfo.tm_mon + 1);
    
    //strftime(dateBuffer, sizeof(dateBuffer), "%a, %b %d", &timeInfo);
    currentData.currentTime = timeBuffer;
    currentData.currentDayMonth = dateBuffer;
    currentData.timeValid = true;
    return true;
}

} // namespace Weather
