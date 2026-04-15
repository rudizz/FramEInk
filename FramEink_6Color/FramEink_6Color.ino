/*
 Name:      FramEink_6Color.ino
 Created:   10/16/2022 9:17:34 PM
 Author:    rudipirani
*/

#ifndef ARDUINO_INKPLATECOLOR
#error "Wrong board selection for this example, please select Inkplate 6 Color in the boards menu."
#endif

#include <ApplicationController.h>
#include <CalendarService.h>
#include <CommonFunctions.h>
#include <Network.h>
#include <WiFiAccessPoint/SettingsService.h>

#include "FramEink6ColorRenderer.h"

const bool DEBUG = true;

static frameink::ApplicationRuntime makeInitialRuntimeState()
{
    frameink::ApplicationRuntime runtime;
    runtime.settingsOk = true; // true: skip settings portal at first start, false: show settings portal at first start
    runtime.stateCalendar = true; // true: show calendar dashboard, false: show photo at first start
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
    policy.visibleDays = 3;
    policy.defaultSleepMinutes = 60ll; // change this to set the sleep time between refreshes when not in overnight hours
    policy.overnightSleepMinutes = 360ll;
    policy.overnightStartHour = 23;
    policy.overnightEndHour = 2;
    return policy;
}

RTC_DATA_ATTR frameink::ApplicationRuntime runtimeState;

frameink::NetworkConfiguration networkConfiguration;
Network network(&networkConfiguration);

const frameink::RefreshPolicy refreshPolicy = makeRefreshPolicy();

frameink::PortalSettingsService *settingsService = nullptr;
frameink::CalendarService *calendarService = nullptr;
FramEink6ColorRenderer *renderer = nullptr;
frameink::ApplicationController *appController = nullptr;

void setup()
{
    if (DEBUG)
    {
        Serial.begin(115200);
        Serial.println("Inizializzo...");
    }

    const esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED)
        runtimeState = makeInitialRuntimeState();

    if (settingsService == nullptr)
        settingsService = new frameink::PortalSettingsService();
    if (calendarService == nullptr)
        calendarService = new frameink::CalendarService(&network);
    if (renderer == nullptr)
        renderer = new FramEink6ColorRenderer();
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
        computationTime = static_cast<long long>(endEpoch - refreshResult.startedEpoch + 1) * SEC_2_MICROSEC;

    esp_sleep_enable_timer_wakeup(refreshResult.sleepMinutes * MIN_2_MICROSEC - computationTime);
    (void)esp_deep_sleep_start();
}

void loop()
{
}
