#include "WiFiAPSettings.h"
#include <ArduinoJson.h>
#include <cmath>

WiFiAPSettingsClass::WiFiAPSettingsClass()
{
    EEPROM.begin(EEPROM_SIZE);
    readEEPROM();
}

void WiFiAPSettingsClass::configurePhotoUpload(const frameink::PhotoUploadTarget &target)
{
    photos_.configure(target);
}

void WiFiAPSettingsClass::initAP()
{
    WiFi.mode(WIFI_AP);
    // Only someone who can read the frame may access photos and saved settings.
    // Shortest supported WPA2 passphrase; 6 characters would prevent AP startup.
    snprintf(passwordAP, sizeof(passwordAP), "%08lx",
        static_cast<unsigned long>(esp_random()));
    WiFi.softAP(SSID_AP_Settings, passwordAP);
    serverIP = WiFi.softAPIP();
    settingsDone_ = false;

    photos_.begin(server_);
    server_.on("/api/settings", HTTP_GET, [this]() { sendSettings(); });
    server_.on("/api/settings", HTTP_POST, [this]() { saveSettings(); });
    server_.on("/api/finish", HTTP_POST, [this]() { finish(); });
    server_.begin();
}

void WiFiAPSettingsClass::loop()
{
    const unsigned long idleTimeout = static_cast<unsigned long>(settingDuration) * 60000ul;
    // Chunks and phone heartbeats refresh activity. A stalled/malformed upload
    // must still time out, rather than keeping the frame awake indefinitely.
    while (!settingsDone_ && millis() - photos_.lastActivity() < idleTimeout)
    {
        server_.handleClient();
        delay(1);
    }
    photos_.end();
    server_.stop();
    WiFi.softAPdisconnect(true);
}

void WiFiAPSettingsClass::sendSettings()
{
    photos_.touch();
    JsonDocument document;
    document["ssid"] = SSID_User;
    document["password"] = PWD_User;
    document["latitude"] = Latitude_User;
    document["longitude"] = Longitude_User;
    document["calendarUrl"] = ICALID_User;
    char start[6], end[6];
    frameink::formatClockTime(nightPause.startMinute, start);
    frameink::formatClockTime(nightPause.endMinute, end);
    document["nightPauseStart"] = start;
    document["nightPauseEnd"] = end;
    char minute[3];
    snprintf(minute, sizeof(minute), "%02u", unsigned(updateMinute));
    document["updateMinute"] = minute;
    document["photoOrder"] = frameink::photoOrderName(photoOrder);
    String body;
    serializeJson(document, body);
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", body);
}

void WiFiAPSettingsClass::saveSettings()
{
    photos_.touch();
    String ssid = server_.arg("ssid");
    String password = server_.arg("password");
    String latitude = server_.arg("latitude");
    String longitude = server_.arg("longitude");
    String calendar = server_.arg("calendarUrl");
    frameink::PhotoOrder order = photoOrder;
    if (server_.hasArg("photoOrder") &&
        !frameink::parsePhotoOrder(server_.arg("photoOrder").c_str(), order))
    {
        server_.send(400, "text/plain; charset=utf-8", "Choose Random or Alphabetical photo order.");
        return;
    }
    uint8_t minute = updateMinute;
    if (server_.hasArg("updateMinute") &&
        !frameink::parseUpdateMinute(server_.arg("updateMinute").c_str(), minute))
    {
        server_.send(400, "text/plain; charset=utf-8", "Use a whole update minute from 00 to 59.");
        return;
    }
    frameink::NightPause pause = nightPause;
    if (server_.hasArg("nightPauseStart") || server_.hasArg("nightPauseEnd"))
    {
        if (!frameink::parseClockTime(server_.arg("nightPauseStart").c_str(), pause.startMinute) ||
            !frameink::parseClockTime(server_.arg("nightPauseEnd").c_str(), pause.endMinute) || !pause.valid())
        {
            server_.send(400, "text/plain; charset=utf-8",
                "Use valid pause times (HH:MM), leaving at least one hour outside the pause. Equal times disable it.");
            return;
        }
    }
    latitude.replace(',', '.');
    longitude.replace(',', '.');
    char *latitudeEnd = nullptr;
    char *longitudeEnd = nullptr;
    const double lat = strtod(latitude.c_str(), &latitudeEnd);
    const double lon = strtod(longitude.c_str(), &longitudeEnd);
    const size_t size = 5 + frameink::nightPauseStorageBytes + frameink::updateMinuteStorageBytes + frameink::photoOrderStorageBytes +
        ssid.length() + password.length() + latitude.length() + longitude.length() + calendar.length();

    if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63 ||
        latitude.length() == 0 || longitude.length() == 0 ||
        *latitudeEnd != '\0' || *longitudeEnd != '\0' ||
        !std::isfinite(lat) || !std::isfinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180 ||
        calendar.length() > 250 || size > EEPROM_SIZE)
    {
        server_.send(400, "text/plain; charset=utf-8", "Check the WiFi name, password, coordinates and calendar URL length.");
        return;
    }

    const String previous[] = {SSID_User, PWD_User, Latitude_User, Longitude_User, ICALID_User};
    const frameink::NightPause previousPause = nightPause;
    const uint8_t previousMinute = updateMinute;
    const frameink::PhotoOrder previousOrder = photoOrder;
    SSID_User = ssid;
    PWD_User = password;
    Latitude_User = latitude;
    Longitude_User = longitude;
    ICALID_User = calendar;
    nightPause = pause;
    updateMinute = minute;
    photoOrder = order;
    if (!writeEEPROM())
    {
        SSID_User = previous[0];
        PWD_User = previous[1];
        Latitude_User = previous[2];
        Longitude_User = previous[3];
        ICALID_User = previous[4];
        nightPause = previousPause;
        updateMinute = previousMinute;
        photoOrder = previousOrder;
        server_.send(500, "text/plain; charset=utf-8", "Unable to save the settings.");
        return;
    }
    // Saving settings keeps the portal open so photos can be uploaded afterwards.
    server_.send(200, "text/plain; charset=utf-8", "Settings saved. You can upload photos or tap Finish.");
}

void WiFiAPSettingsClass::finish()
{
    photos_.touch();
    if (photos_.busy())
    {
        server_.send(409, "text/plain; charset=utf-8", "Wait until the photo upload is complete.");
        return;
    }
    if (SSID_User.length() == 0)
    {
        server_.send(400, "text/plain; charset=utf-8", "Save the WiFi settings first.");
        return;
    }
    server_.send(200, "text/plain; charset=utf-8", "All done. FramEInk will display the calendar and go to sleep.");
    settingsDone_ = true;
}

bool WiFiAPSettingsClass::writeEEPROM()
{
    int offset = EEPROM_START_ADDR;
    const String *values[] = {&SSID_User, &PWD_User, &Latitude_User, &Longitude_User, &ICALID_User};
    for (const String *value : values)
    {
        EEPROM.write(offset++, value->length());
        for (size_t i = 0; i < value->length(); ++i)
            EEPROM.write(offset++, (*value)[i]);
    }
    return frameink::writeNightPause(EEPROM, offset, EEPROM_SIZE, nightPause) &&
        frameink::writeUpdateMinute(EEPROM, offset + frameink::nightPauseStorageBytes, EEPROM_SIZE, updateMinute) &&
        frameink::writePhotoOrder(EEPROM, offset + frameink::nightPauseStorageBytes + frameink::updateMinuteStorageBytes,
            EEPROM_SIZE, photoOrder) &&
        EEPROM.commit();
}

bool WiFiAPSettingsClass::readStringFromEEPROM(int &offset, String &value)
{
    if (offset < EEPROM_START_ADDR || offset >= EEPROM_SIZE)
        return false;
    const int length = EEPROM.read(offset++);
    if (length == 255 || offset + length > EEPROM_SIZE)
        return false;
    value = "";
    value.reserve(length);
    for (int i = 0; i < length; ++i)
        value += static_cast<char>(EEPROM.read(offset++));
    return true;
}

void WiFiAPSettingsClass::readEEPROM()
{
    int offset = EEPROM_START_ADDR;
    String values[5];
    for (String &value : values)
    {
        if (!readStringFromEEPROM(offset, value))
            return;
    }
    SSID_User = values[0];
    PWD_User = values[1];
    Latitude_User = values[2];
    Longitude_User = values[3];
    ICALID_User = values[4];
    nightPause = frameink::readNightPause(EEPROM, offset, EEPROM_SIZE);
    updateMinute = frameink::readUpdateMinute(EEPROM, offset + frameink::nightPauseStorageBytes, EEPROM_SIZE);
    photoOrder = frameink::readPhotoOrder(EEPROM, offset + frameink::nightPauseStorageBytes + frameink::updateMinuteStorageBytes,
        EEPROM_SIZE);
}
