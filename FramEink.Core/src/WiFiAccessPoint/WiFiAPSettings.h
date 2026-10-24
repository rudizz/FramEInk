#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <EEPROM.h>
#include "PhotoUploadService.h"
#include "CaptivePortalDns.h"
#include "../Configuration/NightPause.h"
#include "../Configuration/UpdateMinute.h"
#include "../Configuration/PhotoOrder.h"

#define EEPROM_START_ADDR 0
#define EEPROM_SIZE 512

class WiFiAPSettingsClass
{
  public:
    WiFiAPSettingsClass();
    void configurePhotoUpload(const frameink::PhotoUploadTarget &target);
    void initAP();
    void loop();

    const char *SSID_AP_Settings = "FramEInk";
    static constexpr size_t accessPointPasswordLength = 8; // WPA2 requires at least 8.
    char passwordAP[accessPointPasswordLength + 1] = {};
    IPAddress serverIP;
    const uint16_t settingDuration = 10; // Active uploads/phone conversion refresh the idle timer.
    String SSID_User;
    String PWD_User;
    String Latitude_User;
    String Longitude_User;
    String ICALID_User;
    frameink::NightPause nightPause;
    uint8_t updateMinute = frameink::defaultUpdateMinute;
    frameink::PhotoOrder photoOrder = frameink::defaultPhotoOrder;

  private:
    void sendSettings();
    void saveSettings();
    void finish();
    void redirectToPortal();
    bool readStringFromEEPROM(int &offset, String &value);
    void readEEPROM();
    bool writeEEPROM();

    WebServer server_{80};
    frameink::CaptivePortalDns dns_;
    frameink::PhotoUploadService photos_;
    bool settingsDone_ = false;
};

