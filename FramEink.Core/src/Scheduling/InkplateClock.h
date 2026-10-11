#pragma once

#include "RefreshSchedule.h"

class RTC;

namespace frameink {
// Both supported boards route the external RTC alarm to ESP32 GPIO39.
bool restoreRtcTime(RTC &rtc);
bool updateRtcTime(RTC &rtc);
bool enterScheduledSleep(RTC &rtc, int64_t targetEpoch);
void sleepUntilNextRefresh(RTC &rtc, RefreshScheduleState &schedule,
                          int timeZoneSeconds, const NightPause &pause, uint8_t updateMinute,
                          bool freshNetworkTime);
} // namespace frameink
