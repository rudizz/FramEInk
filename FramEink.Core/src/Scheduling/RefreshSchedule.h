#pragma once

#include "../Configuration/NightPause.h"
#include "../Configuration/UpdateMinute.h"
#include <cstdint>

namespace frameink {

// Retained in RTC memory during deep sleep; reset on a new power-on session.
struct RefreshScheduleState
{
    // POD: no constructor may reset retained RTC memory on a deep-sleep boot.
    int64_t lastTargetEpoch;
};

inline int64_t nextRefreshEpoch(int64_t now, RefreshScheduleState &state,
                                int timeZoneSeconds, const NightPause &pause, uint8_t updateMinute)
{
    // Leave time to program the alarm, and don't repeat a slot after NTP moves
    // the clock backwards. Late/missed updates NEVER redefine the saved minute.
    const int64_t threshold = now + 5 > state.lastTargetEpoch ? now + 5 : state.lastTargetEpoch;
    const uint8_t minute = updateMinute < 60 ? updateMinute : defaultUpdateMinute;
    // Settings use LOCAL minutes, including locations with half/quarter-hour
    // UTC offsets. Recompute the UTC phase when the weather offset changes.
    const int64_t phase = ((int64_t(minute) * 60 - timeZoneSeconds) % 3600 + 3600) % 3600;
    int64_t candidate = threshold - threshold % 3600 + phase;
    if (candidate <= threshold)
        candidate += 3600;

    const NightPause effectivePause = pause.valid() ? pause : NightPause{};
    // Validation guarantees at least one slot per day, including unusual
    // non-whole-hour UTC offsets and pauses crossing midnight.
    for (unsigned i = 0; i < 24; ++i, candidate += 3600)
    {
        const int64_t localSecond = ((candidate + timeZoneSeconds) % 86400 + 86400) % 86400;
        if (!effectivePause.contains(static_cast<uint16_t>(localSecond / 60)))
            return candidate;
    }
    return candidate; // Defensive bound; unreachable for valid minute offsets.
}

} // namespace frameink
