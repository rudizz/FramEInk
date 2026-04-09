#include "SettingsService.h"

namespace frameink {

void PortalSettingsService::beginPortal()
{
    portal_.initAP();
}

void PortalSettingsService::waitUntilConfigured()
{
    portal_.loop();
}

DeviceConfiguration PortalSettingsService::currentConfiguration() const
{
    return readConfiguration();
}

DeviceConfiguration PortalSettingsService::ensureLoaded(bool &settingsOk)
{
    if (!settingsOk)
    {
        beginPortal();
        waitUntilConfigured();
        settingsOk = true;
    }

    return currentConfiguration();
}

const char *PortalSettingsService::accessPointSsid() const
{
    return portal_.SSID_AP_Settings;
}

IPAddress PortalSettingsService::accessPointIp() const
{
    return portal_.serverIP;
}

uint16_t PortalSettingsService::accessPointDurationMinutes() const
{
    return portal_.settingDuration;
}

DeviceConfiguration PortalSettingsService::readConfiguration() const
{
    DeviceConfiguration configuration;
    configuration.wifiSsid = portal_.SSID_User;
    configuration.wifiPassword = portal_.PWD_User;
    configuration.calendarUrl = portal_.ICALID_User;
    configuration.coordinates.latitude = atof(portal_.Latitude_User.c_str());
    configuration.coordinates.longitude = atof(portal_.Longitude_User.c_str());
    return configuration;
}

} // namespace frameink
