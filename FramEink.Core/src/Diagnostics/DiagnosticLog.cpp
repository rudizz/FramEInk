#include "DiagnosticLog.h"

#if defined(ESP32) || defined(FRAMEINK_DIAGNOSTICS_TEST)
#include <Arduino.h>
#include <Inkplate.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace frameink {
namespace {
struct Breadcrumb
{
    uint32_t magic;
    uint32_t boots;
    uint32_t failures;
    uint32_t configurationReady;
    char stage[40];
    uint32_t checksum;
};
// Unlike RTC_DATA_ATTR, this is not reinitialized on a software/panic reset.
RTC_NOINIT_ATTR Breadcrumb retained;
constexpr uint32_t breadcrumbMagic = 0x46494c32;
SdFat *storage = nullptr;
char pending[2048] = {};
size_t pendingSize = 0;
bool storageAttached = false;

uint32_t checksum()
{
    const auto *bytes = reinterpret_cast<const uint8_t *>(&retained);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(Breadcrumb, checksum); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

const char *resetName(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
    }
}

bool append(const char *line, size_t size)
{
    if (!storage) return false;
    SdFile file;
    if (!file.open(currentLogPath, O_RDWR | O_CREAT)) return false;
    if (file.fileSize() + size > maxLogBytes)
    {
        if ((storage->exists(previousLogPath) && !storage->remove(previousLogPath)) ||
            !file.rename(previousLogPath))
        {
            file.close();
            return false;
        }
        file.close();
        if (!file.open(currentLogPath, O_RDWR | O_CREAT)) return false;
    }
    const bool ok = file.seekSet(file.fileSize()) &&
        file.write(reinterpret_cast<const uint8_t *>(line), size) == size && file.sync();
    file.close(); // No open log handle while the display, photo decoder or portal uses SD.
    return ok;
}

void persist(const char *line, size_t size)
{
    if (!storageAttached)
    {
        if (pendingSize + size < sizeof(pending))
        {
            memcpy(pending + pendingSize, line, size);
            pendingSize += size;
        }
    }
    else if (storage && !append(line, size))
    {
        storage = nullptr; // Do not keep retrying a removed/full/failing card this wake.
        Serial.printf("[Diagnostics] SD write failed; serial logging remains active.\n");
    }
}
}

BootDiagnostics beginDiagnostics(const char *model)
{
    storage = nullptr;
    storageAttached = false;
    pendingSize = 0;
    const esp_reset_reason_t reason = esp_reset_reason();
    BootDiagnostics boot;
    boot.unexpectedReset = reason != ESP_RST_POWERON && reason != ESP_RST_EXT && reason != ESP_RST_DEEPSLEEP;
    const bool valid = reason != ESP_RST_POWERON && reason != ESP_RST_EXT &&
        retained.magic == breadcrumbMagic &&
        retained.checksum == checksum() && retained.configurationReady <= 1 &&
        retained.stage[sizeof(retained.stage) - 1] == 0;
    // A ROM/bootloader watchdog can follow power-on before setup ever runs.
    // Only resume when the retained application context proves Settings completed.
    char previousStage[sizeof(retained.stage)] = "unavailable";
    if (valid) memcpy(previousStage, retained.stage, sizeof(previousStage));
    if (!valid) retained = {};
    retained.magic = breadcrumbMagic;
    ++retained.boots;
    if (boot.unexpectedReset)
        retained.failures = retained.failures < 3 ? retained.failures + 1 : 3;
    else if (reason != ESP_RST_DEEPSLEEP)
        retained.failures = 0;
    boot.recoveryPortal = retained.failures >= 3;
    boot.resumeConfiguredSession = boot.unexpectedReset && valid &&
        retained.configurationReady == 1 && !boot.recoveryPortal;
    retained.checksum = checksum();
    diagnosticLog("boot model=%s firmware=%s %s reset=%s(%d) wake=%d failures=%lu rtc_context=%d resume=%d previous_stage=%s",
        model, __DATE__, __TIME__, resetName(reason), int(reason), int(esp_sleep_get_wakeup_cause()),
        static_cast<unsigned long>(retained.failures), int(valid), int(boot.resumeConfiguredSession), previousStage);
    diagnosticStage("boot");
    return boot;
}

void attachDiagnosticStorage(SdFat *card)
{
    storageAttached = true;
    storage = card;
    if (storage && !storage->exists("/frameink-logs") && !storage->mkdir("/frameink-logs"))
        storage = nullptr;
    if (storage && pendingSize > 0 && !append(pending, pendingSize)) storage = nullptr;
    pendingSize = 0;
    diagnosticLog("SD logging %s", storage ? "ready" : "unavailable (serial only)");
}

void diagnosticLog(const char *format, ...)
{
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    char line[448];
    const int written = snprintf(line, sizeof(line),
        "utc=%lld boot=%lu ms=%lu heap=%lu min_heap=%lu max_alloc=%lu psram=%lu stack=%lu %s\n",
        static_cast<long long>(time(nullptr)), static_cast<unsigned long>(retained.boots),
        static_cast<unsigned long>(millis()), static_cast<unsigned long>(ESP.getFreeHeap()),
        static_cast<unsigned long>(ESP.getMinFreeHeap()), static_cast<unsigned long>(ESP.getMaxAllocHeap()),
        static_cast<unsigned long>(ESP.getFreePsram()),
        static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)), message);
    if (written <= 0) return;
    const size_t length = static_cast<size_t>(written) < sizeof(line) ? written : sizeof(line) - 1;
    Serial.printf("%s", line);
    persist(line, length);
}

void diagnosticStage(const char *stage)
{
    snprintf(retained.stage, sizeof(retained.stage), "%s", stage);
    retained.checksum = checksum();
    diagnosticLog("stage=%s", retained.stage);
}

void diagnosticSleep()
{
    retained.failures = 0;
    diagnosticStage("deep_sleep");
}

void diagnosticConfigurationReady()
{
    if (retained.configurationReady == 1) return;
    retained.configurationReady = 1;
    retained.checksum = checksum();
    diagnosticLog("Settings completed; application recovery enabled");
}
} // namespace frameink
#endif
