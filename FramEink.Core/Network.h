/*
Network.h
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

#include "Arduino.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// To get timeZone from main file
extern int timeZone;

// Wifi ssid and password
extern String ssid;
extern String pass;

extern String calendarURL;

#ifndef NETWORK_H
#define NETWORK_H

// All functions defined in Network.cpp

class Network
{
  public:
    // Functions we can access in main file
    void begin();
    void getTime(char *timeStr, long offset = 0);
    void getTimeHour(int *timeHour, long offSet);
    bool getDataCalendar(char *data);
    void getDataFromMetaWeather(int * timezone_offset, char *temp_min0, char *temp_min1, char *temp_min2, char *temp_min3, char *temp_min4, char *temp_min5, char *currentTemp,
                      char *temp_max0, char *temp_max1, char *temp_max2, char *temp_max3, char *temp_max4, char *temp_max5,
                      char *predictability0, char *predictability1, char *predictability2, char *predictability3, char *predictability4, char *predictability5,
                      char *currentWind, char *currentTime, char *currentWeather0, char *currentWeather1,
                      char *currentWeather2, char *currentWeather3, char *currentWeather4, char *currentWeather5,
                      char *abbr0, char *abbr1, char *abbr2, char *abbr3, char *abbr4, char *abbr5,
                      float* moon_phase0, float* moon_phase1, float* moon_phase2, float* moon_phase3, float* moon_phase4, float* moon_phase5);
    void getDaysLabel(char *day, char *day1, char *day2, char *day3);

    // Used to store loaction woeid (world id), set in findCity()
    // Munich
    const int location = 676757; // Woeid Munich: 676757
    float latitude; // = 48.150914;
    float longitude; // = 11.567003;
    
  private:
    // Functions called from within our class
    void setTime();
};

#endif
