/*
Network.cpp
Inkplate 6 Arduino library
David Zovko, Borna Biro, Denis Vajak, Zvonimir Haramustek @ e-radionica.com
September 24, 2020
https://github.com/e-radionicacom/Inkplate-6-Arduino-library

For support, please reach over forums: forum.e-radionica.com/en
For more info about the product, please check: www.inkplate.io

This code is released under the GNU Lesser General Public License v3.0: https://www.gnu.org/licenses/lgpl-3.0.en.html
Please review the LICENSE file included with this example.
If you have any questions about licensing, please contact techsupport@e-radionica.com
Distributed as-is; no warranty is given.
*/

#include "Network.h"
#include "Connectivity/WiFiConnection.h"
#include "Connectivity/TimeSync.h"
#include "Connectivity/BoundedHttpBody.h"
#include "Weather/WeatherParser.h"
#include "Diagnostics/DiagnosticLog.h"

namespace {
bool downloadBody(const String &url, char *data, size_t capacity, size_t &size)
{
    size = 0;
    if (!data || capacity == 0) return false;
    data[0] = 0;
    WiFiClient plain;
    WiFiClientSecure secure;
    // Preserve the existing TLS policy, but bound the TLS handshake as well.
    secure.setInsecure();
    secure.setHandshakeTimeout(15);
    HTTPClient http;
    http.useHTTP10(true); // Request identity framing; never parse chunk headers as iCalendar/JSON.
    http.setReuse(false);
    http.setConnectTimeout(10000);
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setRedirectLimit(3);
    const char *headers[] = {"Transfer-Encoding", "Content-Encoding"};
    http.collectHeaders(headers, 2);
    bool begun = false;
    if (url.startsWith("https://")) begun = http.begin(secure, url);
    else if (url.startsWith("http://")) begun = http.begin(plain, url);
    if (!begun)
    {
        frameink::diagnosticLog("HTTP begin failed or unsupported URL scheme");
        return false;
    }
    const int code = http.GET();
    bool loaded = false;
    const String transfer = http.header("Transfer-Encoding");
    const String encoding = http.header("Content-Encoding");
    if (code == HTTP_CODE_OK && (transfer.length() == 0 || transfer == "identity") &&
        (encoding.length() == 0 || encoding == "identity"))
    {
        WiFiClient *stream = http.getStreamPtr();
        if (stream)
        {
            const auto result = frameink::readHttpBody(*stream, data, capacity, http.getSize());
            size = result.size;
            loaded = result.status == frameink::BodyStatus::Ok;
            frameink::diagnosticLog("HTTP body status=%d bytes=%u declared=%d", int(result.status),
                unsigned(size), http.getSize());
        }
    }
    else frameink::diagnosticLog("HTTP rejected code=%d or unsupported response encoding", code);
    // Stop first: this core's end()/disconnect() may drain an unbounded response.
    plain.stop();
    secure.stop();
    http.end();
    return loaded;
}
} // namespace

// Declared week days
char weekDays[7][4] = {
    "Mon", "Tue", "Wed", "Thr", "Fri", "Sat", "Sun",
};

Network::Network()
{
}

Network::Network(frameink::NetworkConfiguration *configuration)
{
    setConfiguration(configuration);
}

void Network::setConfiguration(frameink::NetworkConfiguration *configuration)
{
    if (configuration != nullptr)
        configuration_ = configuration;
}

frameink::NetworkConfiguration &Network::configuration()
{
    return *configuration_;
}

const frameink::NetworkConfiguration &Network::configuration() const
{
    return *configuration_;
}

bool Network::connectWiFi()
{
    if (connectionFailed_) return false;
    frameink::diagnosticStage("wifi_connect");
    if (configuration_->device.hasWiFiCredentials() &&
        frameink::ensureWiFiConnected(configuration_->device.wifiSsid.c_str(),
                                     configuration_->device.wifiPassword.c_str()))
    {
        frameink::diagnosticLog("WiFi connected RSSI=%ld", static_cast<long>(WiFi.RSSI()));
        return true;
    }
    connectionFailed_ = true;
    frameink::diagnosticLog("WiFi unavailable; skipping network requests this cycle (no restart)");
    return false;
}

bool Network::begin()
{
    connectionFailed_ = false;
    timeSynchronized_ = false;
    if (!connectWiFi()) return false;
    frameink::diagnosticStage("ntp_sync");
    setTime();
    frameink::diagnosticLog("NTP fresh=%d", timeSynchronized_);
    return true;
}

// Gets time from ntp server
void Network::getTime(char *timeStr, long offSet)
{
    // Get seconds since 1.1.1970.
    time_t nowSecs = getNowEpoch(true) + offSet;

    // Used to store time
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    // Copies time string into timeStr
    strcpy(timeStr, asctime(&timeinfo));
}

// Gets time from ntp server
time_t Network::getNowEpoch(bool withTimezone)
{
    //return 1677327149; // 25 feb 2023
    //return 1712917792;
    if (withTimezone)
        return time(nullptr) + (long)configuration_->timeZoneSeconds;
    // Get seconds since 1.1.1970.
    return time(nullptr);
}

// Gets time struct from ntp server
void Network::getTimeHour(int *timeHour, long offSet)
{
    // Get seconds since 1.1.1970.
    time_t nowSecs = getNowEpoch(true) + offSet;

    // Used to store time
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    // Copies time string into timeStr
    *timeHour = timeinfo.tm_hour;
}
// Function to get all calendar data from web
long Network::getDataCalendar(char *data, size_t capacity)
{
    if (!data || capacity == 0) return 0;
    data[0] = 0;
    if (!connectWiFi()) return 0;
    frameink::diagnosticStage("calendar_download");
    size_t size = 0;
    if (!downloadBody(configuration_->device.calendarUrl, data, capacity, size)) return 0;
    if (size == 0 || memchr(data, 0, size) != nullptr ||
        strncmp(data, "BEGIN:VCALENDAR", 15) != 0 || strstr(data, "END:VCALENDAR") == nullptr)
    {
        frameink::diagnosticLog("Calendar response incomplete or invalid");
        data[0] = 0;
        return 0;
    }
    return static_cast<long>(size);
}

bool Network::loadWeatherForecast(frameink::WeatherForecast &forecast)
{
    if (!connectWiFi()) return false;
    frameink::diagnosticStage("weather_download");
    const bool sleep = WiFi.getSleep();
    WiFi.setSleep(false);
    constexpr size_t capacity = 32769;
    char *data = static_cast<char *>(ps_malloc(capacity));
    if (!data) data = static_cast<char *>(malloc(capacity));
    if (!data)
    {
        frameink::diagnosticLog("Weather buffer allocation failed");
        WiFi.setSleep(sleep);
        return false;
    }
    char url[256];
    snprintf(url, sizeof(url),
        "https://api.openweathermap.org/data/3.0/onecall?units=metric&exclude=current,minutely,hourly,alerts&lat=%.4f&lon=%.4f&appid=136b1eed9485c22eee88ea1c437650af",
        configuration_->device.coordinates.latitude,
        configuration_->device.coordinates.longitude);
    size_t size = 0;
    bool loaded = downloadBody(String(url), data, capacity, size);
    if (loaded)
    {
        frameink::diagnosticStage("weather_parse");
        loaded = frameink::parseWeather(data, size, forecast);
    }
    frameink::diagnosticLog("Weather loaded=%d", loaded);
    free(data);
    WiFi.setSleep(sleep);
    return loaded;
}

void Network::setTime()
{
    timeSynchronized_ = frameink::synchronizeNetworkTime();
    const time_t nowSecs = time(nullptr);

    // Used to store time info
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    if (DEBUG_PRINT)
        Serial.printf("Current time: %s\n", asctime(&timeinfo));
}

void Network::getDaysLabel(char *day, char *day1, char *day2, char *day3)
{
    // Seconds since 1.1.1970.
    time_t nowSecs = time(nullptr);

    // Find weekday

    // We get seconds since 1970, add 3600 (1 hour) times the time zone and add 3 to
    // make monday the first day of the week, as 1.1.1970. was a thursday
    // finally do mod 7 to insure our day is within [0, 6]
    int dayWeek = ((long)((nowSecs + configuration_->timeZoneSeconds) / 86400L) + 3) % 7;

    // Copy day data to globals in main file
    strncpy(day, weekDays[dayWeek], 3);
    strncpy(day1, weekDays[(dayWeek + 1) % 7], 3);
    strncpy(day2, weekDays[(dayWeek + 2) % 7], 3);
    strncpy(day3, weekDays[(dayWeek + 3) % 7], 3);
}
