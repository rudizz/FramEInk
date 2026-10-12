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
#include <Scheduling/InkplateClock.h>
#include <WiFiAccessPoint/SettingsService.h>

#include "FramEink6ColorRenderer.h"

const bool DEBUG = true;

static frameink::ApplicationRuntime makeInitialRuntimeState()
{
    frameink::ApplicationRuntime runtime;
    runtime.settingsOk = false; // true: skip settings portal at first start, false: show settings portal at first start
    runtime.stateCalendar = true; // true: show calendar dashboard, false: show photo at first start
    runtime.timeZoneSeconds = 0;
    runtime.sleepMinutes = 60ll;
    runtime.lastAwakeEpoch = 0;
    runtime.portraitPhotos = {};
    runtime.landscapePhotos = {};
    runtime.schedule = {0};
    return runtime;
}

static frameink::RefreshPolicy makeRefreshPolicy()
{
    frameink::RefreshPolicy policy;
    policy.visibleDays = 3;
    policy.defaultSleepMinutes = 60ll;
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
    frameink::restoreRtcTime(renderer->clockRtc());
    settingsService->configurePhotoUpload(renderer->photoUploadTarget());
    appController->executeCycle(runtimeState, *renderer);
    renderer->present();

    const frameink::DeviceConfiguration configuration = settingsService->currentConfiguration();
    frameink::sleepUntilNextRefresh(renderer->clockRtc(), runtimeState.schedule,
        runtimeState.timeZoneSeconds, configuration.nightPause, configuration.updateMinute,
        network.timeSynchronized());
}

void loop()
{
}
