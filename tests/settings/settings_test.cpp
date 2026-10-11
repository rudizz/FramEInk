#include <cassert>
#include "WiFiAPSettings.h"
#include <iostream>

namespace {
void fields(WebServer &server)
{
    server.arguments = {{"ssid", "Test network"}, {"password", "secret-password"},
        {"latitude", "48.1"}, {"longitude", "11.5"}, {"calendarUrl", "https://example.test/calendar.ics"},
        {"nightPauseStart", "22:30"}, {"nightPauseEnd", "06:15"}, {"updateMinute", "45"}};
}
int seedLegacy()
{
    EEPROM = {};
    int offset = 0;
    for (const char *value : {"Legacy WiFi", "legacy-password", "48.1", "11.5", "https://example.test/old.ics"})
    {
        EEPROM.flash.at(offset++) = std::strlen(value);
        for (const char *p = value; *p; ++p) EEPROM.flash.at(offset++) = *p;
    }
    return offset;
}
}

int main()
{
    // Upgrade from the previous night-pause firmware, without the minute block.
    const int tail = seedLegacy();
    EEPROM.begin(EEPROM_SIZE);
    frameink::NightPause previousPause;
    previousPause.startMinute = 1320;
    previousPause.endMinute = 360;
    assert(frameink::writeNightPause(EEPROM, tail, EEPROM_SIZE, previousPause) && EEPROM.commit());
    const auto previousFlash = EEPROM.flash;
    WiFiAPSettingsClass upgraded;
    assert(upgraded.nightPause.startMinute == 1320 && upgraded.nightPause.endMinute == 360);
    assert(upgraded.updateMinute == 0 && EEPROM.flash == previousFlash);
    assert(std::string(upgraded.SSID_User.c_str()) == "Legacy WiFi");

    seedLegacy();
    WiFiAPSettingsClass portal;
    assert(std::string(portal.SSID_User.c_str()) == "Legacy WiFi");
    assert(portal.nightPause.startMinute == 1380 && portal.nightPause.endMinute == 300);
    assert(portal.updateMinute == 0);
    const auto old = EEPROM.flash;
    portal.initAP();
    WebServer &server = *WebServer::lastCreated;
    server.call("/api/settings");
    assert(std::string(server.body.c_str()).find("\"nightPauseStart\":\"23:00\"") != std::string::npos);
    assert(std::string(server.body.c_str()).find("\"nightPauseEnd\":\"05:00\"") != std::string::npos);
    assert(std::string(server.body.c_str()).find("\"updateMinute\":\"00\"") != std::string::npos);
    fields(server);
    server.call("/api/settings", HTTP_POST);
    assert(server.status == 200 && EEPROM.flash != old);
    assert(portal.nightPause.startMinute == 1350 && portal.nightPause.endMinute == 375);
    assert(portal.updateMinute == 45);

    WiFiAPSettingsClass rebooted;
    assert(std::string(rebooted.SSID_User.c_str()) == "Test network");
    assert(std::string(rebooted.PWD_User.c_str()) == "secret-password");
    assert(rebooted.nightPause.startMinute == 1350 && rebooted.nightPause.endMinute == 375);
    assert(rebooted.updateMinute == 45);
    rebooted.initAP();
    WebServer &second = *WebServer::lastCreated;
    second.call("/api/settings");
    assert(std::string(second.body.c_str()).find("\"nightPauseEnd\":\"06:15\"") != std::string::npos);
    assert(std::string(second.body.c_str()).find("\"updateMinute\":\"45\"") != std::string::npos);
    const auto saved = EEPROM.flash;
    for (const char *bad : {"", "60", "99", "100", "-1", "1.5", "1e1", " 5", "5 ", "+1", "00:00"})
    {
        fields(second);
        second.arguments["updateMinute"] = bad;
        second.call("/api/settings", HTTP_POST);
        assert(second.status == 400 && EEPROM.flash == saved && rebooted.updateMinute == 45);
    }
    for (const char *bad : {"24:00", "12:60", "1:00", "", "12:00:00"})
    {
        fields(second);
        second.arguments["nightPauseStart"] = bad;
        second.call("/api/settings", HTTP_POST);
        assert(second.status == 400 && EEPROM.flash == saved);
        assert(rebooted.nightPause.startMinute == 1350);
    }
    fields(second);
    second.arguments["nightPauseStart"] = "00:00";
    second.arguments["nightPauseEnd"] = "23:01";
    second.call("/api/settings", HTTP_POST);
    assert(second.status == 400 && EEPROM.flash == saved);
    fields(second);
    second.arguments.erase("nightPauseEnd");
    second.call("/api/settings", HTTP_POST);
    assert(second.status == 400 && EEPROM.flash == saved);

    // Failed EEPROM commit must preserve the previously loaded values and flash.
    fields(second);
    second.arguments["nightPauseStart"] = "21:00";
    second.arguments["updateMinute"] = "59";
    EEPROM.failCommit = true;
    second.call("/api/settings", HTTP_POST);
    assert(second.status == 500 && EEPROM.flash == saved);
    assert(rebooted.nightPause.startMinute == 1350);
    assert(rebooted.updateMinute == 45);
    EEPROM.failCommit = false;

    // Legacy clients without scheduling keys preserve the saved pause/minute.
    fields(second);
    second.arguments.erase("nightPauseStart");
    second.arguments.erase("nightPauseEnd");
    second.arguments.erase("updateMinute");
    second.call("/api/settings", HTTP_POST);
    assert(second.status == 200 && rebooted.nightPause.startMinute == 1350);
    assert(rebooted.updateMinute == 45);
    fields(second);
    second.arguments["nightPauseStart"] = second.arguments["nightPauseEnd"] = "00:00";
    second.call("/api/settings", HTTP_POST);
    assert(second.status == 200 && rebooted.nightPause.startMinute == rebooted.nightPause.endMinute);
    WiFiAPSettingsClass disabled;
    assert(disabled.nightPause.startMinute == 0 && disabled.nightPause.endMinute == 0);
    assert(disabled.updateMinute == 45);

    // Every supported minute survives save/reboot, with and without zero padding.
    for (unsigned minute = 0; minute < 60; ++minute)
        for (bool padded : {false, true})
        {
            fields(second);
            char text[3];
            snprintf(text, sizeof(text), padded ? "%02u" : "%u", minute);
            second.arguments["updateMinute"] = text;
            second.call("/api/settings", HTTP_POST);
            assert(second.status == 200 && rebooted.updateMinute == minute);
            WiFiAPSettingsClass reloaded;
            assert(reloaded.updateMinute == minute && reloaded.nightPause.startMinute == 1350);
        }
    std::cout << "Settings API: legacy migration, minute/pause validation, save/reload and commit failure passed\n";
}
