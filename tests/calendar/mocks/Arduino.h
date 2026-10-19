#pragma once
#include <cstdint>
#include <cstring>
#include <ctime>
#define RTC_DATA_ATTR
struct TestSerial {
    template<class... Args> void printf(const char *, Args...) {}
};
inline TestSerial Serial;
inline void esp_sleep_enable_timer_wakeup(uint64_t) {}
inline void esp_deep_sleep_start() {}

inline uint32_t testMillis = 0;
inline uint32_t millis() { return testMillis; }
inline void delay(unsigned ms) { testMillis += ms; }
