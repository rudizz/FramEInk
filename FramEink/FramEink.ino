/*
 Name:      FramEink.ino
 Created:   10/16/2022 9:17:34 PM
 Author:    rudipirani
*/

#if !defined(ARDUINO_INKPLATE10) && !defined(ARDUINO_INKPLATE10V2)
#error "Select Inkplate 10 for this sketch. For Inkplate 6 Color use FramEink_6Color/FramEink_6Color.ino."
#endif

#include <ApplicationController.h>
#include <CalendarService.h>
#include <CommonFunctions.h>
#include <Network.h>
#include <Scheduling/InkplateClock.h>
#include <Diagnostics/DiagnosticLog.h>
#include <new>
#include <WiFiAccessPoint/SettingsService.h>

#include "FramEink10Renderer.h"

static frameink::ApplicationRuntime makeInitialRuntimeState()
{
    frameink::ApplicationRuntime runtime{};
    runtime.signature = frameink::runtimeSignature;
    runtime.settingsOk = false;
    runtime.stateCalendar = true;
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
    policy.visibleDays = 6;
    policy.defaultSleepMinutes = 60ll;
    return policy;
}

RTC_DATA_ATTR frameink::ApplicationRuntime runtimeState;

frameink::NetworkConfiguration networkConfiguration;
Network network(&networkConfiguration);

const frameink::RefreshPolicy refreshPolicy = makeRefreshPolicy();

frameink::PortalSettingsService *settingsService = nullptr;
frameink::CalendarService *calendarService = nullptr;
FramEink10Renderer *renderer = nullptr;
frameink::ApplicationController *appController = nullptr;

void setup()
{
    Serial.begin(115200);
    Serial.println(F("FRAMEINK_BOARD_V1:Inkplate10"));
    const auto boot = frameink::beginDiagnostics("Inkplate 10");

    const esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    if (wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED || runtimeState.signature != frameink::runtimeSignature)
        runtimeState = makeInitialRuntimeState();

    if (settingsService == nullptr)
        settingsService = new (std::nothrow) frameink::PortalSettingsService();
    if (calendarService == nullptr)
        calendarService = new (std::nothrow) frameink::CalendarService(&network);
    if (renderer == nullptr)
        renderer = new (std::nothrow) FramEink10Renderer();
    if (appController == nullptr)
        appController = new (std::nothrow) frameink::ApplicationController(
            &network,
            settingsService,
            calendarService,
            refreshPolicy);

    if (!settingsService || !calendarService || !renderer || !appController)
    {
        frameink::diagnosticLog("Startup allocation failed; retrying in one hour");
        esp_sleep_enable_timer_wakeup(3600ull * 1000000ull);
        esp_deep_sleep_start();
        return;
    }
    // Resume only a configured application session, not a watchdog before setup.
    // Three consecutive faults open the portal for log retrieval.
    if (boot.resumeConfiguredSession && !boot.recoveryPortal &&
        settingsService->currentConfiguration().hasWiFiCredentials())
        runtimeState.settingsOk = true;
    if (boot.recoveryPortal) runtimeState.settingsOk = false;
    frameink::diagnosticStage("display_sd_init");
    renderer->begin();
    frameink::attachDiagnosticStorage(renderer->photoUploadTarget().storage);
    frameink::restoreRtcTime(renderer->clockRtc());
    settingsService->configurePhotoUpload(renderer->photoUploadTarget());
    const auto result = appController->executeCycle(runtimeState, *renderer, boot.recoveryPortal);
    if (result.shouldPresent)
    {
        frameink::diagnosticStage("display_refresh");
        renderer->present();
        frameink::diagnosticStage("display_complete");
    }

    const frameink::DeviceConfiguration configuration = settingsService->currentConfiguration();
    frameink::sleepUntilNextRefresh(renderer->clockRtc(), runtimeState.schedule,
        runtimeState.timeZoneSeconds, configuration.nightPause, configuration.updateMinute,
        network.timeSynchronized());
}

void loop()
{
}
