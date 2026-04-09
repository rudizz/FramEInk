#pragma once

#include "../Configuration/DeviceConfiguration.h"
#include "WiFiAPSettings.h"

namespace frameink {

class IDeviceConfigurationProvider
{
  public:
    virtual ~IDeviceConfigurationProvider() = default;
    virtual void beginPortal() = 0;
    virtual void waitUntilConfigured() = 0;
    virtual DeviceConfiguration currentConfiguration() const = 0;
    virtual DeviceConfiguration ensureLoaded(bool &settingsOk) = 0;
    virtual const char *accessPointSsid() const = 0;
    virtual IPAddress accessPointIp() const = 0;
    virtual uint16_t accessPointDurationMinutes() const = 0;
};

class PortalSettingsService : public IDeviceConfigurationProvider
{
  public:
    PortalSettingsService() = default;

    void beginPortal() override;
    void waitUntilConfigured() override;
    DeviceConfiguration currentConfiguration() const override;
    DeviceConfiguration ensureLoaded(bool &settingsOk) override;
    const char *accessPointSsid() const override;
    IPAddress accessPointIp() const override;
    uint16_t accessPointDurationMinutes() const override;

  private:
    DeviceConfiguration readConfiguration() const;

    WiFiAPSettingsClass portal_;
};

} // namespace frameink
