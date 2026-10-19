#pragma once
#include <Arduino.h>
struct IPAddress {};
struct MockWiFi {
    bool sleeping = true;
    bool getSleep() { return sleeping; }
    void setSleep(bool value) { sleeping = value; }
    long RSSI() { return -65; }
};
inline MockWiFi WiFi;
