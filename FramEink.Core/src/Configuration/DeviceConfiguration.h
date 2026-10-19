#pragma once

#include <Arduino.h>
#include "NightPause.h"
#include "UpdateMinute.h"
#include "PhotoOrder.h"

namespace frameink {

struct GeoCoordinates
{
    float latitude = 0.0f;
    float longitude = 0.0f;
};

struct DeviceConfiguration
{
    String wifiSsid;
    String wifiPassword;
    String calendarUrl;
    GeoCoordinates coordinates;
    NightPause nightPause;
    uint8_t updateMinute = defaultUpdateMinute;
    PhotoOrder photoOrder = defaultPhotoOrder;

    bool hasWiFiCredentials() const
    {
        return wifiSsid.length() > 0;
    }

    bool hasCalendarUrl() const
    {
        return calendarUrl.length() > 0;
    }
};

struct NetworkConfiguration
{
    DeviceConfiguration device;
    int timeZoneSeconds = 0;
};

} // namespace frameink
