#pragma once

#include "../Calendar/CalendarAgenda.h"
#include "../Calendar/CalendarService.h"
#include "../Configuration/DeviceConfiguration.h"
#include "../Network.h"
#include "../Weather/WeatherForecast.h"
#include "../WiFiAccessPoint/SettingsService.h"

namespace frameink {

struct ApplicationRuntime
{
    bool settingsOk = true;
    bool stateCalendar = true;
    int timeZoneSeconds = 0;
    long long sleepMinutes = 60ll;
    time_t lastAwakeEpoch = 0;
    unsigned int counterPortrait = 0;
    unsigned int counterLandscape = 0;
};

struct RefreshPolicy
{
    int visibleDays = 6;
    long long defaultSleepMinutes = 60ll;
    long long overnightSleepMinutes = 360ll;
    int overnightStartHour = 23;
    int overnightEndHour = 2;
};

struct DashboardModel
{
    DeviceConfiguration deviceConfiguration;
    WeatherForecast weather;
    CalendarAgenda calendar;
    bool hasCalendar = false;
};

enum class RefreshMode
{
    Photo,
    Dashboard,
};

struct RefreshResult
{
    RefreshMode mode = RefreshMode::Dashboard;
    time_t startedEpoch = 0;
    long long sleepMinutes = 60ll;
};

class IApplicationRenderer
{
  public:
    virtual ~IApplicationRenderer() = default;

    virtual void renderSettingsPortal(const IDeviceConfigurationProvider &settings) = 0;
    virtual void renderSettingsSaved() = 0;
    virtual void renderPhoto(ApplicationRuntime &runtime) = 0;
    virtual void renderDashboard(const DashboardModel &model, ApplicationRuntime &runtime) = 0;
};

class ApplicationController
{
  public:
    ApplicationController() = default;
    ApplicationController(
        Network *network,
        IDeviceConfigurationProvider *settingsProvider,
        CalendarService *calendarService,
        RefreshPolicy refreshPolicy);

    void setServices(
        Network *network,
        IDeviceConfigurationProvider *settingsProvider,
        CalendarService *calendarService);

    RefreshResult executeCycle(ApplicationRuntime &runtime, IApplicationRenderer &renderer);

  private:
    long long resolveSleepMinutes() const;

    Network *network_ = nullptr;
    IDeviceConfigurationProvider *settingsProvider_ = nullptr;
    CalendarService *calendarService_ = nullptr;
    RefreshPolicy refreshPolicy_;
    DashboardModel dashboard_;
};

} // namespace frameink
