#include "Scheduling/InkplateClock.h"
#include "Connectivity/TimeSync.h"
#include <Inkplate.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <type_traits>

using namespace frameink;
static_assert(std::is_trivial<RefreshScheduleState>::value, "Do not reset RTC state with a constructor");

uint32_t testMilliseconds = 0;
void (*testClockAdvanced)() = nullptr;
TestSerial Serial;
int testInterruptLevel = HIGH;
TestSleep testSleep;
bool testSntpEnabled = false;
unsigned testSntpStops = 0;
void (*testSyncCallback)(timeval *) = nullptr;

// UTC base: 2026-10-11. No host wall-clock, RTC, network or board is touched.
constexpr int64_t baseEpoch = 1791676800;
int64_t testWallBase = baseEpoch;
uint32_t testNtpStarted = 0;
uint32_t testNtpDelay = std::numeric_limits<uint32_t>::max();

extern "C" time_t frameink_test_time(time_t *value)
{
    const time_t now = static_cast<time_t>(testWallBase + testMilliseconds / 1000);
    if (value) *value = now;
    return now;
}
extern "C" int frameink_test_settimeofday(const timeval *value, const struct timezone *)
{
    testWallBase = value->tv_sec - testMilliseconds / 1000;
    return 0;
}

void configTime(long offset, int dst, const char *first, const char *second)
{
    assert(offset == 0 && dst == 0);
    assert(std::string(first) == "pool.ntp.org" && std::string(second) == "time.nist.gov");
    testSntpEnabled = true;
    testNtpStarted = testMilliseconds;
    testClockAdvanced = [] {
        if (testSntpEnabled && testMilliseconds - testNtpStarted >= testNtpDelay)
        {
            timeval response{};
            testSyncCallback(&response);
        }
    };
    if (testNtpDelay == 0) testClockAdvanced();
}

namespace {
int64_t at(int day, int hour, int minute, int second = 0)
{
    return baseEpoch + day * 86400ll + hour * 3600ll + minute * 60ll + second;
}
NightPause pause(uint16_t start, uint16_t end)
{
    NightPause result;
    result.startMinute = start;
    result.endMinute = end;
    return result;
}

void scheduleTests()
{
    RefreshScheduleState state{};
    const NightPause disabled = pause(0, 0);
    // User example: powering on at 09:36 must NOT establish the hourly phase.
    assert(nextRefreshEpoch(at(0, 9, 36), state, 0, disabled, 0) == at(0, 10, 0));
    assert(nextRefreshEpoch(at(0, 10, 0, 25), state, 0, disabled, 0) == at(0, 11, 0));
    assert(nextRefreshEpoch(at(0, 11, 0, 3), state, 0, disabled, 0) == at(0, 12, 0));
    assert(nextRefreshEpoch(at(0, 9, 36), state, 0, disabled, 45) == at(0, 9, 45));
    assert(nextRefreshEpoch(at(0, 9, 36), state, 0, disabled, 15) == at(0, 10, 15));
    assert(nextRefreshEpoch(at(0, 16, 5, 42), state, 0, disabled, 5) == at(0, 17, 5));
    assert(nextRefreshEpoch(at(0, 17, 5, 8), state, 0, disabled, 5) == at(0, 18, 5));
    // A late refresh skips missed slots without changing the minute.
    assert(nextRefreshEpoch(at(0, 20, 8), state, 0, disabled, 5) == at(0, 21, 5));
    state.lastTargetEpoch = at(0, 20, 5);
    assert(nextRefreshEpoch(at(0, 20, 2), state, 0, disabled, 5) == at(0, 21, 5));
    state.lastTargetEpoch = 0;

    assert(nextRefreshEpoch(at(0, 22, 5, 50), state, 0, NightPause{}, 5) == at(1, 5, 5));
    assert(nextRefreshEpoch(at(0, 23, 5), state, 0, NightPause{}, 5) == at(1, 5, 5));
    assert(nextRefreshEpoch(at(1, 5, 5), state, 0, NightPause{}, 5) == at(1, 6, 5));
    assert(nextRefreshEpoch(at(0, 21, 5), state, 2 * 3600, NightPause{}, 5) == at(1, 3, 5));
    assert(nextRefreshEpoch(at(0, 21, 5), state, -5 * 3600, NightPause{}, 5) == at(0, 22, 5));
    assert(nextRefreshEpoch(at(0, 22, 5), state, 0, pause(22 * 60 + 30, 6 * 60 + 15), 5) == at(1, 7, 5));
    assert(nextRefreshEpoch(at(0, 8, 5), state, 0, pause(9 * 60, 17 * 60), 5) == at(0, 17, 5));
    assert(nextRefreshEpoch(at(0, 22, 59), state, 0, NightPause{}, 0) == at(1, 5, 0));
    assert(nextRefreshEpoch(at(0, 23, 59), state, 0, disabled, 59) == at(1, 0, 59));
    // Invalid persisted values use :00. Half/quarter-hour zones use LOCAL minutes.
    assert(nextRefreshEpoch(at(0, 9, 36), state, 0, disabled, 255) == at(0, 10, 0));
    assert(nextRefreshEpoch(at(0, 9, 36), state, 19800, disabled, 0) == at(0, 10, 30));
    assert(nextRefreshEpoch(at(0, 9, 36), state, 20700, disabled, 0) == at(0, 10, 15));
    assert(nextRefreshEpoch(at(0, 9, 36), state, -12600, disabled, 45) == at(0, 10, 15));
    assert(!NightPause{}.contains(5 * 60) && NightPause{}.contains(23 * 60));

    // Simulate a year of variable processing times and NTP corrections. The
    // selected minute is never recomputed from a delayed/early actual wake.
    state = {};
    int64_t now = at(0, 16, 5, 20);
    for (unsigned cycle = 0; cycle < 9000; ++cycle)
    {
        const int64_t next = nextRefreshEpoch(now, state, 0, NightPause{}, 5);
        assert(next > now && next % 3600 == 5 * 60);
        assert(!NightPause{}.contains(static_cast<uint16_t>((next % 86400) / 60)));
        state.lastTargetEpoch = next;
        now = next + int(cycle % 80) - 15;
    }

    // Different offsets, phases and valid pause windows must find a slot within
    // 24h and always keep the phase, even for the maximum 23-hour pause.
    for (int offset : {-12 * 3600, -1800, 0, 19800, 20700, 14 * 3600})
        for (unsigned phase = 0; phase < 60; ++phase)
            for (unsigned start = 0; start < 1440; start += 37)
            {
                state = {};
                const NightPause night = pause(start, (start + 1380) % 1440);
                const int64_t next = nextRefreshEpoch(at(0, 16, 30), state, offset, night, phase);
                const uint16_t localMinute = static_cast<uint16_t>(((next + offset) % 86400) / 60);
                assert(((next + offset) % 3600) == phase * 60 && next > at(0, 16, 30));
                assert(next <= at(1, 16, 30) + 5 && !night.contains(localMinute));
            }
}

struct Storage
{
    std::array<uint8_t, 512> bytes{};
    Storage() { bytes.fill(255); }
    uint8_t read(int offset) { return bytes.at(offset); }
    void write(int offset, uint8_t value) { bytes.at(offset) = value; }
};

void settingsTests()
{
    uint16_t parsed = 123;
    for (const char *bad : {"", "1:00", "24:00", "23:60", "-1:05", "12:05:00", "12:0x", "12: 0"})
    { assert(!parseClockTime(bad, parsed)); assert(parsed == 123); }
    assert(!parseClockTime(nullptr, parsed));
    for (unsigned minute = 0; minute < 1440; ++minute)
    {
        char text[6];
        formatClockTime(minute, text);
        assert(parseClockTime(text, parsed) && parsed == minute);
    }
    assert(pause(0, 0).valid() && pause(0, 1380).valid());
    assert(!pause(0, 1381).valid() && !pause(1440, 10).valid());
    for (int offset : {0, 20, 300, 504})
    {
        Storage storage;
        storage.bytes.fill(42); // Existing credentials and arbitrary legacy tail.
        const auto old = storage.bytes;
        auto night = readNightPause(storage, offset, 512);
        assert(night.startMinute == 1380 && night.endMinute == 300);
        assert(writeNightPause(storage, offset, 512, pause(1350, 390)));
        night = readNightPause(storage, offset, 512);
        assert(night.startMinute == 1350 && night.endMinute == 390);
        for (int i = 0; i < 512; ++i)
            if (i < offset || i >= offset + nightPauseStorageBytes) assert(storage.bytes[i] == old[i]);
        const auto valid = storage.bytes;
        for (int index = 0; index < nightPauseStorageBytes; ++index)
            for (unsigned bit = 0; bit < 8; ++bit)
            {
                storage.bytes = valid;
                storage.bytes[offset + index] ^= 1 << bit;
                night = readNightPause(storage, offset, 512);
                assert(night.startMinute == 1380 && night.endMinute == 300);
            }
    }
    Storage storage;
    const auto empty = storage.bytes;
    assert(!writeNightPause(storage, 505, 512, NightPause{}));
    assert(!writeNightPause(storage, -1, 512, NightPause{}));
    assert(!writeNightPause(storage, 0, 512, pause(0, 1439)));
    assert(storage.bytes == empty);
    assert(readNightPause(storage, 510, 512).endMinute == 300);

    uint8_t minute = 23;
    for (const char *bad : {"", "60", "99", "100", "-1", "1.5", "1e1", " 5", "5 ", "+1", "00:00"})
    { assert(!parseUpdateMinute(bad, minute)); assert(minute == 23); }
    assert(!parseUpdateMinute(nullptr, minute));
    for (unsigned value = 0; value < 60; ++value)
    {
        char text[3];
        std::snprintf(text, sizeof(text), "%02u", value);
        assert(parseUpdateMinute(text, minute) && minute == value);
        std::snprintf(text, sizeof(text), "%u", value);
        assert(parseUpdateMinute(text, minute) && minute == value);
    }
    for (int offset : {0, 20, 300, 507})
    {
        Storage data;
        assert(readUpdateMinute(data, offset, 512) == 0);
        for (unsigned value = 0; value < 60; ++value)
        {
            const auto before = data.bytes;
            assert(writeUpdateMinute(data, offset, 512, value));
            assert(readUpdateMinute(data, offset, 512) == value);
            for (int i = 0; i < 512; ++i)
                if (i < offset || i >= offset + updateMinuteStorageBytes) assert(data.bytes[i] == before[i]);
        }
        const auto valid = data.bytes;
        for (int index = 0; index < updateMinuteStorageBytes; ++index)
            for (unsigned bit = 0; bit < 8; ++bit)
            {
                data.bytes = valid;
                data.bytes[offset + index] ^= 1 << bit;
                assert(readUpdateMinute(data, offset, 512) == 0);
            }
    }
    assert(!writeUpdateMinute(storage, 508, 512, 5));
    assert(!writeUpdateMinute(storage, -1, 512, 5));
    assert(!writeUpdateMinute(storage, 0, 512, 60));
    assert(readUpdateMinute(storage, 510, 512) == 0 && readUpdateMinute(storage, -1, 512) == 0);
    assert(storage.bytes == empty);
}

void resetClock(int64_t now = at(0, 16, 5, 20))
{
    testMilliseconds = 0;
    testWallBase = now;
    testInterruptLevel = HIGH;
    testSleep = {};
    Serial.output.clear();
}
RTC clockAt(int64_t epoch)
{
    RTC rtc;
    rtc.setEpoch(static_cast<uint32_t>(epoch));
    return rtc;
}

void hardwareTests()
{
    resetClock();
    RTC rtc;
    assert(!restoreRtcTime(rtc));
    assert(updateRtcTime(rtc));
    assert(rtc.getEpoch() == at(0, 16, 5, 20));
    testWallBase += 120;
    assert(restoreRtcTime(rtc) && frameink_test_time(nullptr) == at(0, 16, 5, 20));
    assert(enterScheduledSleep(rtc, at(0, 17, 5)));
    assert(testSleep.calls == 1 && testSleep.extCalls == 1);
    assert(rtc.alarmEpoch == at(0, 17, 5));
    assert(testSleep.timerUs == (3580ull + 300) * 1000000);

    // Verify the retained target is written before entering sleep, not after
    // esp_deep_sleep_start(), which never returns on the real hardware.
    resetClock();
    rtc = clockAt(frameink_test_time(nullptr));
    static RefreshScheduleState state{};
    state = {};
    testSleep.beforeSleep = [] { assert(state.lastTargetEpoch == at(0, 17, 5)); };
    sleepUntilNextRefresh(rtc, state, 0, NightPause{}, 5, false);
    assert(state.lastTargetEpoch == at(0, 17, 5) && rtc.timeWrites == 1);

    resetClock(at(0, 9, 36));
    rtc = clockAt(frameink_test_time(nullptr));
    state = {};
    testSleep.beforeSleep = [] { assert(state.lastTargetEpoch == at(0, 10, 0)); };
    sleepUntilNextRefresh(rtc, state, 0, NightPause{}, 0, true);
    assert(rtc.alarmEpoch == at(0, 10, 0) && rtc.timeWrites == 2);

    for (int failure = 0; failure < 5; ++failure)
    {
        resetClock();
        rtc = clockAt(frameink_test_time(nullptr));
        if (failure == 0) rtc.initialized = false;
        if (failure == 1) rtc.alarmWritesWork = false;
        if (failure == 2) testInterruptLevel = 0;
        if (failure == 3) testSleep.extResult = -1;
        if (failure == 4) rtc.value -= 60;
        assert(enterScheduledSleep(rtc, at(0, 17, 5)));
        assert(testSleep.calls == 1 && testSleep.timerUs == 3580ull * 1000000);
        assert(Serial.output.find("fallback timer") != std::string::npos);
    }
    resetClock();
    rtc = clockAt(frameink_test_time(nullptr));
    testSleep.timerResult = -1;
    assert(!enterScheduledSleep(rtc, at(0, 17, 5)) && testSleep.calls == 0);
    assert(!enterScheduledSleep(rtc, at(0, 16, 5)));
    resetClock(0);
    rtc = {};
    state = {};
    testSleep.beforeSleep = [] { assert(state.lastTargetEpoch == 0); };
    sleepUntilNextRefresh(rtc, state, 0, NightPause{}, 0, false);
    assert(testSleep.calls == 1 && testSleep.timerUs == 3600ull * 1000000);
}

void ntpTests()
{
    resetClock(); // A valid retained date must not cause immediate success.
    testNtpDelay = std::numeric_limits<uint32_t>::max();
    testSntpEnabled = false;
    assert(!synchronizeNetworkTime());
    assert(testMilliseconds == 15000 && !testSntpEnabled);
    for (uint32_t wait : {0u, 100u, 3200u, 15000u})
    {
        resetClock();
        testNtpDelay = wait;
        assert(synchronizeNetworkTime());
        assert(testMilliseconds == wait && !testSntpEnabled);
    }
    // A previous successful callback must not count as a new synchronization.
    resetClock();
    testNtpDelay = std::numeric_limits<uint32_t>::max();
    assert(!synchronizeNetworkTime(555));
    assert(testMilliseconds == 555 && !testSntpEnabled);
    resetClock();
    testMilliseconds = std::numeric_limits<uint32_t>::max() - 500;
    const uint32_t start = testMilliseconds;
    testNtpDelay = 2500;
    assert(synchronizeNetworkTime());
    assert(static_cast<uint32_t>(testMilliseconds - start) == 2500);
}
}

int main()
{
    scheduleTests();
    settingsTests();
    hardwareTests();
    ntpTests();
    std::cout << "Scheduling, night pause storage, RTC alarms/fallback and fresh NTP tests passed\n";
}
