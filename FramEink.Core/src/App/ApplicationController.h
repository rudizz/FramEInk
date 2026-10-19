#pragma once

#include "../Calendar/CalendarAgenda.h"
#include "../Calendar/CalendarService.h"
#include "../Configuration/DeviceConfiguration.h"
#include "../Network.h"
#include "../Weather/WeatherForecast.h"
#include "../Scheduling/RefreshSchedule.h"
#include "../Photos/PhotoSequenceState.h"
#include "../WiFiAccessPoint/SettingsService.h"
#include <type_traits>

namespace frameink {

constexpr uint32_t runtimeSignature = 0x46495232;

struct ApplicationRuntime
{
    uint32_t signature;
    bool settingsOk;
    bool stateCalendar;
    int timeZoneSeconds;
    long long sleepMinutes;
    time_t lastAwakeEpoch;
    PhotoSequenceState portraitPhotos;
    PhotoSequenceState landscapePhotos;
    RefreshScheduleState schedule;
};
static_assert(std::is_trivial<ApplicationRuntime>::value, "RTC-retained runtime must not have a boot constructor");

struct RefreshPolicy
{
    int visibleDays = 6;
    long long defaultSleepMinutes = 60ll;
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
    bool shouldPresent = true;
};

class IApplicationRenderer
{
  public:
    virtual ~IApplicationRenderer() = default;

    virtual void renderSettingsPortal(const IDeviceConfigurationProvider &settings) = 0;
    virtual void renderSettingsSaved() = 0;
    virtual void renderPhoto(ApplicationRuntime &runtime, PhotoOrder order) = 0;
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

    RefreshResult executeCycle(ApplicationRuntime &runtime, IApplicationRenderer &renderer, bool recoveryOnly = false);

  private:
    Network *network_ = nullptr;
    IDeviceConfigurationProvider *settingsProvider_ = nullptr;
    CalendarService *calendarService_ = nullptr;
    RefreshPolicy refreshPolicy_;
    DashboardModel dashboard_;
};

} // namespace frameink
