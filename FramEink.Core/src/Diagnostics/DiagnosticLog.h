#pragma once

#include <cstddef>

#if defined(ESP32) || defined(FRAMEINK_DIAGNOSTICS_TEST)
#include <Inkplate.h>
#else
class SdFat;
#endif

namespace frameink {

constexpr const char *currentLogPath = "/frameink-logs/current.log";
constexpr const char *previousLogPath = "/frameink-logs/previous.log";
constexpr size_t maxLogBytes = 256 * 1024;

struct BootDiagnostics
{
    bool unexpectedReset = false;
    bool resumeConfiguredSession = false;
    bool recoveryPortal = false;
};

// These calls belong to the main Arduino task, never a Wi-Fi callback/ISR.
#if defined(ESP32) || defined(FRAMEINK_DIAGNOSTICS_TEST)
BootDiagnostics beginDiagnostics(const char *model);
void attachDiagnosticStorage(SdFat *storage);
void diagnosticLog(const char *format, ...) __attribute__((format(printf, 1, 2)));
void diagnosticStage(const char *stage);
void diagnosticConfigurationReady();
void diagnosticSleep();
#else
inline BootDiagnostics beginDiagnostics(const char *) { return {}; }
inline void attachDiagnosticStorage(SdFat *) {}
inline void diagnosticLog(const char *, ...) {}
inline void diagnosticStage(const char *) {}
inline void diagnosticConfigurationReady() {}
inline void diagnosticSleep() {}
#endif

} // namespace frameink
