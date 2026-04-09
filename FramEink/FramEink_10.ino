/*
 Name:      FramEink.ino
 Created:   10/16/2022 9:17:34 PM
 Author:    rudipirani
*/

#include <ApplicationController.h>
#include <CalendarService.h>
#include <CommonFunctions.h>
#include <Network.h>
#include <WiFiAccessPoint/SettingsService.h>

#include "FramEink10Renderer.h"

const bool DEBUG = false;

static frameink::ApplicationRuntime makeInitialRuntimeState()
{
    frameink::ApplicationRuntime runtime;
    runtime.settingsOk = false;
    runtime.stateCalendar = true;
    runtime.timeZoneSeconds = 0;
    runtime.sleepMinutes = 60ll;
    runtime.lastAwakeEpoch = 0;
    runtime.counterPortrait = 0u;
    runtime.counterLandscape = 0u;
    return runtime;
}

static frameink::RefreshPolicy makeRefreshPolicy()
{
    frameink::RefreshPolicy policy;
    policy.visibleDays = 6;
    policy.defaultSleepMinutes = 60ll;
    policy.overnightSleepMinutes = 360ll;
    policy.overnightStartHour = 23;
    policy.overnightEndHour = 2;
    return policy;
}

RTC_DATA_ATTR frameink::ApplicationRuntime runtimeState = makeInitialRuntimeState();

frameink::NetworkConfiguration networkConfiguration;
Network network(&networkConfiguration);

const frameink::RefreshPolicy refreshPolicy = makeRefreshPolicy();

frameink::PortalSettingsService *settingsService = nullptr;
frameink::CalendarService *calendarService = nullptr;
FramEink10Renderer *renderer = nullptr;
frameink::ApplicationController *appController = nullptr;

void setup()
{
    if (DEBUG)
    {
        Serial.begin(115200);
        Serial.println("Inizializzo...");
    }

    if (settingsService == nullptr)
        settingsService = new frameink::PortalSettingsService();
    if (calendarService == nullptr)
        calendarService = new frameink::CalendarService(&network);
    if (renderer == nullptr)
        renderer = new FramEink10Renderer();
    if (appController == nullptr)
        appController = new frameink::ApplicationController(
            &network,
            settingsService,
            calendarService,
            refreshPolicy);

    renderer->begin();
    const frameink::RefreshResult refreshResult = appController->executeCycle(runtimeState, *renderer);
    renderer->present();

    long long computationTime = 0;
    const time_t endEpoch = network.getNowEpoch(false);
    if (refreshResult.startedEpoch > 0)
        computationTime = static_cast<long long>(endEpoch - refreshResult.startedEpoch) * SEC_2_MICROSEC;

    esp_sleep_enable_timer_wakeup(refreshResult.sleepMinutes * MIN_2_MICROSEC - computationTime);
    (void)esp_deep_sleep_start();
}

void loop()
{
}
