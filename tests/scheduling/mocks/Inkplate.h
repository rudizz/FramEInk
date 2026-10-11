#pragma once
#include <Arduino.h>
#include <ctime>

constexpr uint8_t RTC_ALARM_MATCH_DHHMMSS = 0b1111;
class RTC
{
  public:
    bool initialized = false;
    bool writesWork = true;
    bool alarmWritesWork = true;
    unsigned cleared = 0;
    unsigned timeWrites = 0;
    unsigned alarmWrites = 0;
    uint32_t value = 0;
    uint32_t writtenAt = 0;
    uint32_t alarmEpoch = 0;
    unsigned alarmWriteDelayMs = 0;
    tm alarm{};

    bool isSet() { return initialized; }
    uint32_t getEpoch() { return value + (testMilliseconds - writtenAt) / 1000; }
    void setEpoch(uint32_t epoch)
    {
        ++timeWrites;
        if (writesWork) { initialized = true; value = epoch; writtenAt = testMilliseconds; }
    }
    void disableTimer() {}
    void clearTimerFlag() {}
    void clearAlarmFlag() { ++cleared; }
    void setAlarmEpoch(uint32_t epoch, uint8_t mask)
    {
        assert(mask == RTC_ALARM_MATCH_DHHMMSS);
        ++alarmWrites;
        if (alarmWritesWork)
        {
            alarmEpoch = epoch;
            const time_t stamp = epoch;
            gmtime_r(&stamp, &alarm);
        }
        testMilliseconds += alarmWriteDelayMs;
    }
    void readAlarm() {}
    uint8_t getAlarmSecond() { return alarm.tm_sec; }
    uint8_t getAlarmMinute() { return alarm.tm_min; }
    uint8_t getAlarmHour() { return alarm.tm_hour; }
    uint8_t getAlarmDay() { return alarm.tm_mday; }
};
