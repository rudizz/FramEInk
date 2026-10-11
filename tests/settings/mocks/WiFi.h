#pragma once
#include <Arduino.h>
#include <cassert>
struct IPAddress {};
constexpr int WIFI_AP = 2;
struct TestWiFi
{
    void mode(int mode) { assert(mode == WIFI_AP); }
    void softAP(const char *, const char *) {}
    IPAddress softAPIP() { return {}; }
    void softAPdisconnect(bool) {}
};
inline TestWiFi WiFi;
