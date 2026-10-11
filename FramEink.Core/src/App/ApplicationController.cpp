#include "ApplicationController.h"

namespace frameink {

ApplicationController::ApplicationController(
    Network *network,
    IDeviceConfigurationProvider *settingsProvider,
    CalendarService *calendarService,
    RefreshPolicy refreshPolicy)
    : network_(network),
      settingsProvider_(settingsProvider),
      calendarService_(calendarService),
      refreshPolicy_(refreshPolicy)
{
}

void ApplicationController::setServices(
    Network *network,
    IDeviceConfigurationProvider *settingsProvider,
    CalendarService *calendarService)
{
    network_ = network;
    settingsProvider_ = settingsProvider;
    calendarService_ = calendarService;
}

RefreshResult ApplicationController::executeCycle(ApplicationRuntime &runtime, IApplicationRenderer &renderer)
{
    RefreshResult result;
    dashboard_.deviceConfiguration = DeviceConfiguration{};
    dashboard_.weather = WeatherForecast{};
    dashboard_.calendar.available = false;
    dashboard_.calendar.downloadSize = 0;
    dashboard_.calendar.eventCount = 0;
    dashboard_.calendar.name[0] = '\0';
    dashboard_.hasCalendar = false;

    if (network_ == nullptr || settingsProvider_ == nullptr)
        return result;

    result.startedEpoch = network_->getNowEpoch(false);

    if (!runtime.settingsOk)
    {
        settingsProvider_->beginPortal();
        renderer.renderSettingsPortal(*settingsProvider_);
        settingsProvider_->waitUntilConfigured();
        runtime.settingsOk = true;
        renderer.renderSettingsSaved();
    }

    dashboard_.deviceConfiguration = settingsProvider_->currentConfiguration();
    network_->configuration().device = dashboard_.deviceConfiguration;

    result.mode = runtime.stateCalendar ? RefreshMode::Dashboard : RefreshMode::Photo;

    if (result.mode == RefreshMode::Photo)
    {
        renderer.renderPhoto(runtime);
    }
    else
    {
        network_->begin();
        if (network_->loadWeatherForecast(dashboard_.weather))
            runtime.timeZoneSeconds = dashboard_.weather.timeZoneSeconds;
        else
            dashboard_.weather.timeZoneSeconds = runtime.timeZoneSeconds;
        network_->configuration().timeZoneSeconds = runtime.timeZoneSeconds;

        dashboard_.hasCalendar =
            dashboard_.deviceConfiguration.hasCalendarUrl() &&
            calendarService_ != nullptr &&
            calendarService_->loadAgenda(
                dashboard_.calendar,
                runtime.timeZoneSeconds,
                refreshPolicy_.visibleDays,
                network_->getNowEpoch(true));

        renderer.renderDashboard(dashboard_, runtime);
    }

    runtime.stateCalendar = !runtime.stateCalendar;
    runtime.lastAwakeEpoch = result.startedEpoch;
    runtime.sleepMinutes = refreshPolicy_.defaultSleepMinutes;
    result.sleepMinutes = runtime.sleepMinutes;

    return result;
}

} // namespace frameink
