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

#include <ArduinoJson.h>
#include <uICAL.h>

// Static Json from ArduinoJson library
StaticJsonDocument<6000> doc;

// Declared week days
char weekDays[7][4] = {
    "Mon", "Tue", "Wed", "Thr", "Fri", "Sat", "Sun",
};

void Network::begin()
{
    // Initiating wifi, like in BasicHttpClient example
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    int cnt = 0;
    Serial.print(F("Waiting for WiFi to connect..."));
    while ((WiFi.status() != WL_CONNECTED))
    {
        Serial.print(F("."));
        delay(1000);
        ++cnt;

        WiFi.reconnect();
        delay(5000);

        if (cnt == 10)
        {
            Serial.println("Can't connect to WIFI, restarting");
            delay(100);
            ESP.restart();
        }
    }
    Serial.println(F(" connected"));

    // Find internet time
    setTime();
}

// Gets time from ntp server
void Network::getTime(char *timeStr, long offSet)
{
    // Get seconds since 1.1.1970.
    time_t nowSecs = time(nullptr) + (long)timeZone + offSet;

    // Used to store time
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    // Copies time string into timeStr
    strcpy(timeStr, asctime(&timeinfo));
}
// Gets time struct from ntp server
void Network::getTimeHour(int *timeHour, long offSet)
{
    // Get seconds since 1.1.1970.
    time_t nowSecs = time(nullptr) + (long)timeZone + offSet;

    // Used to store time
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    // Copies time string into timeStr
    *timeHour = timeinfo.tm_hour;
}
// Function to get all war data from web
bool Network::getDataCalendar(char *data)
{
    // Variable to store fail
    bool f = 0;

    // If not connected to wifi reconnect wifi
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();

        delay(5000);

        int cnt = 0;
        Serial.println(F("Waiting for WiFi to reconnect..."));
        while ((WiFi.status() != WL_CONNECTED))
        {
            // Prints a dot every second that wifi isn't connected
            Serial.print(F("."));
            delay(1000);
            ++cnt;

            WiFi.reconnect();
            delay(5000);

            if (cnt == 10)
            {
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
    http.begin(calendarURL);

    delay(300);

    // Actually do request
    int httpCode = http.GET();

    if (httpCode == 200)
    {
        //String length = http.getStream().readStringUntil('\n');
        //Serial.println("Test1");
        //uICAL::Calendar_ptr cal = nullptr;
        //try
        //{
        //    Serial.println("Test2");
        //    uICAL::istream_Stream istm(http.getStream());
        //    Serial.println("Test3");
        //    cal = uICAL::Calendar::load(istm);
        //    Serial.println("Test4");
        //}
        //catch (uICAL::Error ex) {
        //    Serial.println(sprintf("%s: %s", ex.message.c_str(), "! Failed loading calendar"));
        //    //stop();
        //}

        //unsigned now = g_ntpClient.getUnixTime();

        //uICAL::DateTime calBegin(now);
        //uICAL::DateTime calEnd(now + 86400);

        long n = 0;
        long timeCal = millis();
        size_t conta = 0;
        // Limito il while al max size del buffer 1000000L
        while (n + 4 < 1000000L && http.getStream().available()) {
            data[n++] = http.getStream().read();
            conta = 0;
            //if (n < 1000) // print calendar data
            //    Serial.print(data[n-1]);
            while (!http.getStream().available() && conta < 10) // aggiungo questo controllo altrimenti esce prima di aver scaricato tutto
            {
                delay(10);
                Serial.println("Calendar waiting...");
                conta++;
            }
        } 
        Serial.printf("Calendar downloaded in [ms]: %d\n", millis() - timeCal);

        data[n++] = 0;
    }
    else
    {
        Serial.print("HTTP CODE: ");
        Serial.println(httpCode);
        f = 1;
    }

    // end http
    http.end();

    return !f;
}

// ====  WEATHER  =====

void formatTemp(char *str, float temp)
{
    // Built in function for float to char* conversion
    temp = roundf(temp);
    dtostrf(temp, 2, 0, str);
}

void formatWind(char *str, float wind)
{
    // Built in function for float to char* conversion
    dtostrf(wind, 2, 0, str);
}

void Network::getDataFromMetaWeather(int *timezone_offset, char *temp_min0, char *temp_min1, char *temp_min2, char *temp_min3, char *temp_min4, char *temp_min5, char *currentTemp,
                      char *temp_max0, char *temp_max1, char *temp_max2, char *temp_max3, char *temp_max4, char *temp_max5,
                      char *predictability0, char *predictability1, char *predictability2, char *predictability3, char *predictability4, char *predictability5,
                      char *currentWind, char *currentTime, char *currentWeather0, char *currentWeather1,
                      char *currentWeather2, char *currentWeather3, char *currentWeather4, char *currentWeather5,
                      char *abbr0, char *abbr1, char *abbr2, char *abbr3, char *abbr4, char *abbr5,
                      float *moon_phase0, float *moon_phase1, float *moon_phase2, float *moon_phase3, float *moon_phase4, float *moon_phase5)
{
    // If not connected to wifi reconnect wifi
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.reconnect();

        delay(5000);

        int cnt = 0;
        Serial.println(F("Waiting for WiFi to reconnect..."));
        while ((WiFi.status() != WL_CONNECTED))
        {
            // Prints a dot every second that wifi isn't connected
            Serial.print(F("."));
            delay(1000);
            ++cnt;

            if (cnt == 7)
            {
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
    sprintf(url, "https://api.openweathermap.org/data/3.0/onecall?units=metric&exclude=current,minutely,hourly,alerts&lat=%.4f&lon=%.4f&appid=136b1eed9485c22eee88ea1c437650af", latitude, longitude);
    // https://api.openweathermap.org/data/3.0/onecall?units=metric&exclude=current,minutely,hourly,alerts&lat=48.150914&lon=11.567003&appid=136b1eed9485c22eee88ea1c437650af

    // Initiate http
    http.begin(url);

    // Actually do request
    int httpCode = http.GET();
    if (httpCode == 200)
    {
        int32_t len = http.getSize();

        if (len > 0)
        {
            // Try parsing JSON object
            DeserializationError error = deserializeJson(doc, http.getStream());

            // If an error happens print it to Serial monitor
            if (error)
            {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.c_str());
            }
            else
            {
                // Set all data got from internet using formatTemp and formatWind defined above
                // This part relies heavily on ArduinoJson library
                formatTemp(currentTemp, doc["daily"][0][F("temp")]["day"].as<float>());
                formatWind(currentWind, doc["daily"][0][F("wind_speed")].as<float>());

                *timezone_offset = doc["timezone_offset"].as<int>();
                ;
                strcpy(currentWeather0, doc["daily"][0]["weather"][0]["main"].as<const char*>());
                strcpy(currentWeather1, doc["daily"][1]["weather"][0]["main"].as<const char *>());
                strcpy(currentWeather2, doc["daily"][2]["weather"][0]["main"].as<const char *>());
                strcpy(currentWeather3, doc["daily"][3]["weather"][0]["main"].as<const char *>());
                strcpy(currentWeather4, doc["daily"][4]["weather"][0]["main"].as<const char *>());
                strcpy(currentWeather5, doc["daily"][5]["weather"][0]["main"].as<const char *>());
                ;

                strcpy(abbr0, doc["daily"][0]["weather"][0]["icon"].as<const char *>());
                strcpy(abbr1, doc["daily"][1]["weather"][0]["icon"].as<const char *>());
                strcpy(abbr2, doc["daily"][2]["weather"][0]["icon"].as<const char *>());
                strcpy(abbr3, doc["daily"][3]["weather"][0]["icon"].as<const char *>());
                strcpy(abbr4, doc["daily"][4]["weather"][0]["icon"].as<const char *>());
                strcpy(abbr5, doc["daily"][5]["weather"][0]["icon"].as<const char *>());
                ;
                // Min Temp
                formatTemp(temp_min0, doc["daily"][0]["temp"][F("min")].as<float>());
                formatTemp(temp_min1, doc["daily"][1]["temp"][F("min")].as<float>());
                formatTemp(temp_min2, doc["daily"][2]["temp"][F("min")].as<float>());
                formatTemp(temp_min3, doc["daily"][3]["temp"][F("min")].as<float>());
                formatTemp(temp_min4, doc["daily"][4]["temp"][F("min")].as<float>());
                formatTemp(temp_min5, doc["daily"][5]["temp"][F("min")].as<float>());
                // Max Temp
                formatTemp(temp_max0, doc["daily"][0]["temp"][F("max")].as<float>());
                formatTemp(temp_max1, doc["daily"][1]["temp"][F("max")].as<float>());
                formatTemp(temp_max2, doc["daily"][2]["temp"][F("max")].as<float>());
                formatTemp(temp_max3, doc["daily"][3]["temp"][F("max")].as<float>());
                formatTemp(temp_max4, doc["daily"][4]["temp"][F("max")].as<float>());
                formatTemp(temp_max5, doc["daily"][5]["temp"][F("max")].as<float>());
                // Predictability
                formatTemp(predictability0, doc["daily"][0][F("pop")].as<float>() * 100);
                formatTemp(predictability1, doc["daily"][1][F("pop")].as<float>() * 100);
                formatTemp(predictability2, doc["daily"][2][F("pop")].as<float>() * 100);
                formatTemp(predictability3, doc["daily"][3][F("pop")].as<float>() * 100);
                formatTemp(predictability4, doc["daily"][4][F("pop")].as<float>() * 100);
                formatTemp(predictability5, doc["daily"][5][F("pop")].as<float>() * 100);
                // Moon Phase
                *moon_phase0 = doc["daily"][0][F("moon_phase")].as<float>();
                *moon_phase1 = doc["daily"][1][F("moon_phase")].as<float>();
                *moon_phase2 = doc["daily"][2][F("moon_phase")].as<float>();
                *moon_phase3 = doc["daily"][3][F("moon_phase")].as<float>();
                *moon_phase4 = doc["daily"][4][F("moon_phase")].as<float>();
                *moon_phase5 = doc["daily"][5][F("moon_phase")].as<float>();
            }
        }
    }

    // Clear document and end http
    doc.clear();
    http.end();

    // Return to initial state
    WiFi.setSleep(sleep);
}

void Network::setTime()
{
    // Used for setting correct time
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print(F("Waiting for NTP time sync: "));
    time_t nowSecs = time(nullptr);
    while (nowSecs < 8 * 3600 * 2)
    {
        delay(500);
        Serial.print(F("."));
        yield();
        nowSecs = time(nullptr);
    }

    Serial.println();

    // Used to store time info
    struct tm timeinfo;
    gmtime_r(&nowSecs, &timeinfo);

    Serial.print(F("Current time: "));
    Serial.print(asctime(&timeinfo));
}

void Network::getDaysLabel(char *day, char *day1, char *day2, char *day3)
{
    // Seconds since 1.1.1970.
    time_t nowSecs = time(nullptr);

    // Find weekday

    // We get seconds since 1970, add 3600 (1 hour) times the time zone and add 3 to
    // make monday the first day of the week, as 1.1.1970. was a thursday
    // finally do mod 7 to insure our day is within [0, 6]
    int dayWeek = ((long)((nowSecs + timeZone) / 86400L) + 3) % 7;

    // Copy day data to globals in main file
    strncpy(day, weekDays[dayWeek], 3);
    strncpy(day1, weekDays[(dayWeek + 1) % 7], 3);
    strncpy(day2, weekDays[(dayWeek + 2) % 7], 3);
    strncpy(day3, weekDays[(dayWeek + 3) % 7], 3);
}
