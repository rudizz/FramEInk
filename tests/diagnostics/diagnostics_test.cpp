#include <Arduino.h>
#include <Diagnostics/DiagnosticLog.h>
#include <esp_system.h>
#include <cassert>
#include <iostream>
using namespace frameink;
std::string contents(SdFat &sd, const char *path = currentLogPath) {
    const auto &data = sd.files.at(path);
    return {data.begin(), data.end()};
}
int main() {
    SdFat sd;
    // Reproduce power-on -> ROM flash read error -> watchdog before setup.
    // A saved Wi-Fi configuration alone must not bypass Settings.
    testResetReason = ESP_RST_WDT;
    auto boot = beginDiagnostics("test");
    assert(boot.unexpectedReset && !boot.resumeConfiguredSession && !boot.recoveryPortal);
    attachDiagnosticStorage(&sd);
    assert(contents(sd).find("rtc_context=0 resume=0 previous_stage=unavailable") != std::string::npos);
    // A reset while Settings is still open must reopen it too.
    diagnosticStage("settings_portal");
    boot = beginDiagnostics("test");
    assert(boot.unexpectedReset && !boot.resumeConfiguredSession && !boot.recoveryPortal);
    attachDiagnosticStorage(&sd);
    assert(contents(sd).find("rtc_context=1 resume=0 previous_stage=settings_portal") != std::string::npos);
    testResetReason = ESP_RST_POWERON;
    boot = beginDiagnostics("test");
    assert(!boot.unexpectedReset && !boot.resumeConfiguredSession && !boot.recoveryPortal);
    diagnosticStage("display_sd_init");
    attachDiagnosticStorage(&sd);
    assert(contents(sd).find("reset=POWERON") != std::string::npos);
    diagnosticConfigurationReady();
    diagnosticStage("calendar_parse");
    testResetReason = ESP_RST_PANIC;
    boot = beginDiagnostics("test");
    assert(boot.unexpectedReset && boot.resumeConfiguredSession && !boot.recoveryPortal);
    attachDiagnosticStorage(&sd);
    assert(contents(sd).find("previous_stage=calendar_parse") != std::string::npos);
    boot = beginDiagnostics("test");
    assert(!boot.recoveryPortal);
    boot = beginDiagnostics("test");
    assert(boot.recoveryPortal && !boot.resumeConfiguredSession);
    attachDiagnosticStorage(&sd);
    diagnosticSleep();
    testResetReason = ESP_RST_DEEPSLEEP;
    boot = beginDiagnostics("test");
    assert(!boot.unexpectedReset && !boot.resumeConfiguredSession && !boot.recoveryPortal);
    attachDiagnosticStorage(&sd);
    assert(contents(sd).find("previous_stage=deep_sleep") != std::string::npos);
    sd.files[currentLogPath].resize(maxLogBytes, 'a');
    diagnosticLog("rotation boundary");
    assert(sd.files[previousLogPath].size() == maxLogBytes);
    assert(contents(sd).find("rotation boundary") != std::string::npos);
    sd.files[currentLogPath].assign(maxLogBytes, 'b');
    diagnosticLog("second rotation");
    assert(contents(sd, previousLogPath)[0] == 'b');
    assert(sd.files[currentLogPath].size() < maxLogBytes);
    sd.failSync = true;
    diagnosticLog("sync failure");
    const auto size = sd.files[currentLogPath].size();
    diagnosticLog("serial only after failure");
    assert(sd.files[currentLogPath].size() == size);
    assert(Serial.output.find("SD write failed") != std::string::npos);
    testResetReason = ESP_RST_BROWNOUT;
    boot = beginDiagnostics("test");
    assert(boot.unexpectedReset && boot.resumeConfiguredSession);
    attachDiagnosticStorage(nullptr);
    diagnosticLog("no SD is harmless");
    testResetReason = ESP_RST_POWERON;
    boot = beginDiagnostics("test");
    assert(!boot.unexpectedReset && !boot.resumeConfiguredSession && !boot.recoveryPortal);
    diagnosticStage("display_sd_init");
    testResetReason = ESP_RST_WDT;
    boot = beginDiagnostics("test");
    assert(boot.unexpectedReset && !boot.resumeConfiguredSession && !boot.recoveryPortal);
    // A manual external reset invalidates a previous configured session as well.
    diagnosticConfigurationReady();
    testResetReason = ESP_RST_EXT;
    boot = beginDiagnostics("test");
    assert(!boot.unexpectedReset && !boot.resumeConfiguredSession && !boot.recoveryPortal);
    testResetReason = ESP_RST_PANIC;
    boot = beginDiagnostics("test");
    assert(boot.unexpectedReset && !boot.resumeConfiguredSession && !boot.recoveryPortal);
    testResetReason = ESP_RST_POWERON;
    for (int failure = 0; failure < 4; ++failure) {
        SdFat broken;
        if (failure == 0) broken.failMkdir = true;
        if (failure == 1) broken.failOpen = true;
        if (failure == 2) broken.failWrite = true;
        if (failure == 3) broken.failRename = true;
        beginDiagnostics("test");
        attachDiagnosticStorage(&broken);
        if (failure == 3) broken.files[currentLogPath].resize(maxLogBytes, 'x');
        diagnosticLog("SD failure must not reset the device");
    }
    std::cout << "Diagnostics: ROM watchdog vs configured recovery, Settings reset, retained stage, crash loop, rotation and SD failures passed.\n";
}
