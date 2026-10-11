#include "TimeSync.h"
#include <Arduino.h>
#include <esp_sntp.h>
#include <atomic>

namespace frameink {
namespace {
std::atomic<bool> synchronized{false};
void onTimeSynchronized(struct timeval *) { synchronized.store(true); }
}

bool synchronizeNetworkTime(uint32_t timeoutMs)
{
    if (sntp_enabled())
        sntp_stop();
    synchronized.store(false);
    sntp_set_time_sync_notification_cb(onTimeSynchronized);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    const uint32_t started = millis();
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.printf("[Clock] Waiting for a fresh NTP response...\n");
    while (!synchronized.load())
    {
        const uint32_t elapsed = static_cast<uint32_t>(millis()) - started;
        if (elapsed >= timeoutMs)
        {
            // Stop late replies from changing the time after the RTC alarm has
            // been calculated. The next dashboard wake will try again.
            sntp_stop();
            Serial.printf("[Clock] NTP timed out; keeping the retained RTC time.\n");
            return false;
        }
        const uint32_t remaining = timeoutMs - elapsed;
        delay(remaining < 100 ? remaining : 100);
    }
    sntp_stop();
    Serial.printf("[Clock] Fresh NTP synchronization completed.\n");
    return true;
}
} // namespace frameink
