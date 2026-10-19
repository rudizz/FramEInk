#include "InkplateClock.h"
#include "../Diagnostics/DiagnosticLog.h"
#include <Inkplate.h>
#include <esp_sleep.h>
#include <sys/time.h>
#include <cstdlib>
#include <ctime>

namespace frameink {
namespace {
constexpr int64_t minimumEpoch = 1577836800; // 2020-01-01 UTC.
constexpr int64_t maximumEpoch = 2147483647; // Installed ESP32 core has 32-bit time_t.
constexpr int rtcInterruptPin = 39;
constexpr uint64_t microsecondsPerSecond = 1000000ull;

bool validEpoch(int64_t epoch) { return epoch >= minimumEpoch && epoch < maximumEpoch; }

void useUtc()
{
    // Inkplate's RTC APIs use localtime/mktime. Keep the hardware clock in UTC;
    // only the pause/display applies the weather service's local UTC offset.
    setenv("TZ", "UTC0", 1);
    tzset();
}
}

bool restoreRtcTime(RTC &rtc)
{
    useUtc();
    rtc.disableTimer();
    rtc.clearTimerFlag();
    rtc.clearAlarmFlag(); // Release the level-triggered wake interrupt.
    if (!rtc.isSet())
        return false;
    const int64_t epoch = rtc.getEpoch();
    if (!validEpoch(epoch))
        return false;
    timeval value{};
    value.tv_sec = static_cast<time_t>(epoch);
    const bool restored = settimeofday(&value, nullptr) == 0;
    if (restored)
        Serial.printf("[Clock] Restored UTC time from the external RTC.\n");
    return restored;
}

bool updateRtcTime(RTC &rtc)
{
    useUtc();
    const int64_t now = time(nullptr);
    if (!validEpoch(now))
        return false;
    rtc.setEpoch(static_cast<uint32_t>(now));
    const int64_t readback = rtc.getEpoch();
    const int64_t difference = readback - now;
    const bool isSet = rtc.isSet();
    const bool updated = isSet && difference >= -2 && difference <= 2;
    diagnosticLog("RTC sync confirmed=%d set=%d expected=%lld read=%lld delta=%lld",
        int(updated), int(isSet), static_cast<long long>(now),
        static_cast<long long>(readback), static_cast<long long>(difference));
    Serial.printf(updated ? "[Clock] External RTC synchronized from NTP.\n"
                          : "[Clock] External RTC did not confirm the new time.\n");
    return updated;
}

bool enterScheduledSleep(RTC &rtc, int64_t targetEpoch)
{
    useUtc();
    const int64_t now = time(nullptr);
    if (!validEpoch(now) || !validEpoch(targetEpoch) || targetEpoch <= now + 2)
        return false;

    bool alarmReady = false;
    if (rtc.isSet())
    {
        const int64_t difference = int64_t(rtc.getEpoch()) - now;
        if (difference >= -3 && difference <= 3)
        {
            rtc.clearAlarmFlag();
            rtc.setAlarmEpoch(static_cast<uint32_t>(targetEpoch), RTC_ALARM_MATCH_DHHMMSS);
            rtc.readAlarm();
            const time_t target = static_cast<time_t>(targetEpoch);
            tm fields{};
            gmtime_r(&target, &fields);
            // The library exposes no I2C write result; check the alarm readback.
            alarmReady = rtc.getAlarmSecond() == fields.tm_sec &&
                         rtc.getAlarmMinute() == fields.tm_min &&
                         rtc.getAlarmHour() == fields.tm_hour && rtc.getAlarmDay() == fields.tm_mday;
            pinMode(rtcInterruptPin, INPUT); // GPIO39 has no internal pull-up.
            alarmReady = alarmReady && digitalRead(rtcInterruptPin) == HIGH;
        }
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (alarmReady)
        alarmReady = esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 0) == ESP_OK;

    // A backup timer prevents an indefinite sleep if the RTC/interrupt fails.
    // In normal operation GPIO39 wakes first, independent of the ESP32 RC clock.
    const int64_t remaining = targetEpoch - int64_t(time(nullptr));
    if (remaining <= 2)
        return false;
    const uint64_t seconds = static_cast<uint64_t>(remaining) + (alarmReady ? 300 : 0);
    if (esp_sleep_enable_timer_wakeup(seconds * microsecondsPerSecond) != ESP_OK)
        return false;
    diagnosticLog("Sleep target=%lld remaining=%lld RTC_alarm=%d", static_cast<long long>(targetEpoch),
        static_cast<long long>(remaining), alarmReady);
    Serial.printf("[Clock] Next wake UTC epoch=%lld, in %lld seconds (%s).\n",
                  static_cast<long long>(targetEpoch), static_cast<long long>(remaining),
                  alarmReady ? "RTC alarm; backup timer enabled" : "RTC unavailable; fallback timer");
    diagnosticSleep();
    esp_deep_sleep_start();
    return true; // Reached only by a test double; deep sleep does not return.
}

void sleepUntilNextRefresh(RTC &rtc, RefreshScheduleState &schedule,
                          int timeZoneSeconds, const NightPause &pause, uint8_t updateMinute,
                          bool freshNetworkTime)
{
    if (freshNetworkTime)
        updateRtcTime(rtc);
    if (validEpoch(time(nullptr)))
    {
        // Recalculate if an unusually slow I2C operation crosses the alarm time.
        for (unsigned attempt = 0; attempt < 2; ++attempt)
        {
            const int64_t target = nextRefreshEpoch(time(nullptr), schedule, timeZoneSeconds, pause, updateMinute);
            const int64_t previousTarget = schedule.lastTargetEpoch;
            schedule.lastTargetEpoch = target; // Must be retained BEFORE deep sleep, which never returns.
            if (enterScheduledSleep(rtc, target))
                return;
            schedule.lastTargetEpoch = previousTarget;
        }
    }
    diagnosticLog("No usable clock/alarm; sleeping for one hour");
    Serial.printf("[Clock] No usable clock/alarm; retrying in one hour without changing the saved minute.\n");
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup(3600ull * microsecondsPerSecond);
    diagnosticSleep();
    esp_deep_sleep_start();
}
} // namespace frameink
