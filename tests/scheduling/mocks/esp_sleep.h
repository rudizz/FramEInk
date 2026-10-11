#pragma once
#include <Arduino.h>
constexpr int ESP_OK = 0;
constexpr int GPIO_NUM_39 = 39;
constexpr int ESP_SLEEP_WAKEUP_ALL = 0;

struct TestSleep
{
    unsigned calls = 0;
    unsigned extCalls = 0;
    unsigned timerCalls = 0;
    uint64_t timerUs = 0;
    int extResult = ESP_OK;
    int timerResult = ESP_OK;
    void (*beforeSleep)() = nullptr;
};
extern TestSleep testSleep;
inline int esp_sleep_disable_wakeup_source(int source) { assert(source == ESP_SLEEP_WAKEUP_ALL); return ESP_OK; }
inline int esp_sleep_enable_ext0_wakeup(int pin, int level)
{
    assert(pin == 39 && level == 0);
    ++testSleep.extCalls;
    return testSleep.extResult;
}
inline int esp_sleep_enable_timer_wakeup(uint64_t microseconds)
{
    ++testSleep.timerCalls;
    testSleep.timerUs = microseconds;
    return testSleep.timerResult;
}
inline void esp_deep_sleep_start()
{
    ++testSleep.calls;
    if (testSleep.beforeSleep) testSleep.beforeSleep();
}
