#pragma once
#include <Arduino.h>
#include <cassert>
#include "IPAddress.h"
constexpr int WIFI_AP = 2;
struct TestWiFi
{
    bool active = false;
    void mode(int mode) { assert(mode == WIFI_AP); }
    void softAP(const char *, const char *) { active = true; }
    IPAddress softAPIP() { return {192, 168, 4, 1}; }
    void softAPdisconnect(bool) { active = false; }
};
inline TestWiFi WiFi;
