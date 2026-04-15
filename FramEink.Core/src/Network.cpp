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

// Shared JSON document for weather parsing.
JsonDocument doc;

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

void Network::begin()
{
    // Initiating wifi, like in BasicHttpClient example
    WiFi.mode(WIFI_STA);
    WiFi.begin(configuration_->device.wifiSsid.c_str(), configuration_->device.wifiPassword.c_str());

    int cnt = 0;
    if (DEBUG_PRINT)
        Serial.print(F("Waiting for WiFi to connect..."));
    while ((WiFi.status() != WL_CONNECTED))
    {
        if (DEBUG_PRINT)
            Serial.print(F("."));
        delay(1000);
        ++cnt;

        WiFi.reconnect();
        delay(5000);

        if (cnt == 20)
        {
            if (DEBUG_PRINT)
                Serial.println("Can't connect to WIFI, restarting");
            delay(100);
            ESP.restart();
        }
    }
    if (DEBUG_PRINT)
        Serial.println(F(" connected"));

    // Find internet time
    setTime();
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
long Network::getDataCalendar(char *data)
{
    // Variable to store fail
    bool f = 0;

    // If not connected to wifi reconnect wifi
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();

        delay(500);

        int cnt = 0;
        //if (DEBUG_PRINT)
            Serial.println(F("Waiting for WiFi to reconnect..."));
        while ((WiFi.status() != WL_CONNECTED))
        {
            // Prints a dot every second that wifi isn't connected
            //if (DEBUG_PRINT)
                Serial.print(F("."));
            delay(1000);
            ++cnt;

            WiFi.reconnect();
            delay(1000);

            if (cnt == 20)
            {
                //if (DEBUG_PRINT)
                    Serial.println("Can't connect to WIFI, restart initiated.");
                delay(100);
                ESP.restart();
            }
        }
    }

    // Http object used to make get request
    HTTPClient http;

    http.getStream().setTimeout(20);
    http.getStream().flush();

    // Begin http by passing url to it
    http.begin(configuration_->device.calendarUrl);

    delay(300);

    // Actually do request
    int httpCode = http.GET();
    long n = 0;
    if (httpCode == 200)
    {
        long timeCal = millis();
        size_t conta = 0;
        size_t contaWhile = 0;
        while ((n < 15 || strstr(data + n - 30, "END:VCALENDAR") == nullptr) && contaWhile < 20)
        {
         
            // Limito il while al max size del buffer SIZE_CALENDAR_DATA
            while (n - 4 < SIZE_CALENDAR_DATA && http.getStream().available()) {
                data[n++] = http.getStream().read();
                conta = 0;
                //if (n < 1000) // print calendar data
                    //Serial.print(data[n-1]);
                while (!http.getStream().available() && conta < 10) // aggiungo questo controllo altrimenti esce prima di aver scaricato tutto
                {
                    delay(100);
                    if (DEBUG_PRINT)
                        Serial.printf("Calendar waiting %i...\n", conta);
                    conta++;
                }
            }
            contaWhile++;
            data[n] = '\0';
        }
        if (DEBUG_PRINT)
            Serial.printf("Calendar downloaded in [ms]: %d, %i bytes\n", millis() - timeCal, n);
        data[n++] = 0;
    }
    else
    {
        if (DEBUG_PRINT)
            Serial.printf("HTTP CODE: %d\n", httpCode);
        f = 1;
        n = 1;
    }

    // end http
    http.end();

    return n;
}

// ====  WEATHER  =====
void formatTemp(char *str, float temp)
{
    // Built in function for float to char* conversion
    dtostrf(temp, 2, 0, str);
}

void formatProbOfRain(char *str, float pop)
{
    // Built in function for float to char* conversion
    dtostrf(pop, 3, 0, str);
}

void formatPrecipitationMM(char *str, float rain)
{
    // Built in function for float to char* conversion
    if (rain < 10)
        dtostrf(rain, 3, 1, str);
    else
        dtostrf(rain, 3, 0, str);
}

void formatWind(char *str, float wind)
{
    // Built in function for float to char* conversion
    dtostrf(wind, 2, 0, str);
}

void padZeros(char* charStr) //this doesn't handle negative numbers
{
    for (int i = 0; i < strlen(charStr); i++)
    {
        if (charStr[i] == ' ')
            charStr[i] = '0';
        else
            break;
    }
}

void formatSunrise(char *str, time_t epochSunrise)
{
    tm tm_sunrise;
    gmtime_r(&epochSunrise, &tm_sunrise);
    
    // Built in function for float to char* conversion
    dtostrf(tm_sunrise.tm_hour, 2, 0, str);
    strcpy(str + 2, ":");
    dtostrf(tm_sunrise.tm_min, 2, 0, str + 3);
    padZeros(str + 3);
}

bool Network::loadWeatherForecast(frameink::WeatherForecast &forecast)
{
    // If not connected to wifi reconnect wifi
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();

        delay(5000);

        int cnt = 0;
        if (DEBUG_PRINT)
            Serial.println(F("Waiting for WiFi to reconnect..."));
        while ((WiFi.status() != WL_CONNECTED))
        {
            // Prints a dot every second that wifi isn't connected
            if (DEBUG_PRINT)
                Serial.print(F("."));
            delay(1000);
            ++cnt;

            if (cnt == 7)
            {
                if (DEBUG_PRINT)
                    Serial.println("Can't connect to WIFI, restart initiated.");
                delay(100);
                ESP.restart();
            }
        }
    }

    // Wake up if sleeping and save initial state
    bool sleep = WiFi.getSleep();
    WiFi.setSleep(false);

    // Http object used to make get request
    HTTPClient http;

    http.getStream().setNoDelay(true);
    http.getStream().setTimeout(5);

    // Add woeid to api call
    char url[256];
    //sprintf(url, "https://www.metaweather.com/api/location/%d/", location);
    sprintf(url,
        "https://api.openweathermap.org/data/3.0/onecall?units=metric&exclude=current,minutely,hourly,alerts&lat=%.4f&lon=%.4f&appid=136b1eed9485c22eee88ea1c437650af",
        configuration_->device.coordinates.latitude,
        configuration_->device.coordinates.longitude);
    // https://api.openweathermap.org/data/3.0/onecall?units=metric&exclude=current,minutely,hourly,alerts&lat=48.150914&lon=11.567003&appid=136b1eed9485c22eee88ea1c437650af

    // Initiate http
    http.begin(url);

    // Actually do request
    int httpCode = http.GET();
    bool loaded = false;
    if (httpCode == 200)
    {
        int32_t len = http.getSize();

        if (len > 0)
        {
            doc.clear();
            // Try parsing JSON object
            DeserializationError error = deserializeJson(doc, http.getStream());

            // If an error happens print it to Serial monitor
            if (error)
            {
                if (DEBUG_PRINT)
                    Serial.printf("deserializeJson() failed: %s\n", error.c_str());
            }
            else
            {
                // Set all data got from internet using formatTemp and formatWind defined above
                // This part relies heavily on ArduinoJson library
                formatTemp(forecast.currentTemperature, doc["daily"][0][F("temp")]["day"].as<float>());
                formatWind(forecast.currentWind, doc["daily"][0][F("wind_speed")].as<float>());
                forecast.timeZoneSeconds = doc["timezone_offset"].as<int>();

                for (size_t day = 0; day < frameink::WeatherForecast::DayCount; ++day)
                {
                    frameink::WeatherDayForecast &dayForecast = forecast.days[day];

                    strcpy(dayForecast.condition, doc["daily"][day]["weather"][0]["main"].as<const char *>());
                    strcpy(dayForecast.iconCode, doc["daily"][day]["weather"][0]["icon"].as<const char *>());

                    formatTemp(dayForecast.minimumTemperature, doc["daily"][day]["temp"][F("min")].as<float>());
                    formatTemp(dayForecast.maximumTemperature, doc["daily"][day]["temp"][F("max")].as<float>());
                    formatProbOfRain(dayForecast.precipitationProbability, doc["daily"][day][F("pop")].as<float>() * 100);
                    dayForecast.hasPrecipitation = doc["daily"][day][F("pop")].as<float>() > 0.0f;
                    formatPrecipitationMM(
                        dayForecast.precipitationMillimeters,
                        getRainSnowPrecipitationMM(strcmp(dayForecast.iconCode, "13d") == 0, day));
                    dayForecast.moonPhase = doc["daily"][day][F("moon_phase")].as<float>();
                    formatSunrise(dayForecast.sunrise, doc["daily"][day][F("sunrise")].as<time_t>() + forecast.timeZoneSeconds);
                    formatSunrise(dayForecast.sunset, doc["daily"][day][F("sunset")].as<time_t>() + forecast.timeZoneSeconds);
                }

                loaded = true;
            }
        }
    }

    // Clear document and end http
    doc.clear();
    http.end();

    // Return to initial state
    WiFi.setSleep(sleep);
    return loaded;
}

float Network::getRainSnowPrecipitationMM(bool isSnowing, int day)
{
    if (isSnowing)
        return doc["daily"][day][F("snow")].as<float>();
    else
        return doc["daily"][day][F("rain")].as<float>();
}

void Network::setTime()
{
    // Used for setting correct time
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    if (DEBUG_PRINT)
        Serial.print(F("Waiting for NTP time sync: "));
    time_t nowSecs = time(nullptr);
    while (nowSecs < 8 * 3600 * 2)
    {
        delay(500);
        if (DEBUG_PRINT)
            Serial.print(F("."));
        yield();
        nowSecs = time(nullptr);
    }

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
