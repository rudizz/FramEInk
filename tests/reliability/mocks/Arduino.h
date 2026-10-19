#pragma once
#include "../../photo_upload/mocks/Arduino.h"
#include <ctime>
#include <cstdlib>
#include <cstring>
#define RTC_DATA_ATTR
#define F(value) value
struct TestSerial { template<class... Args> void printf(const char *, Args...) {} };
inline TestSerial Serial;
inline bool failPsram = false;
inline void *ps_malloc(size_t size) { return failPsram ? nullptr : malloc(size); }
inline void esp_sleep_enable_timer_wakeup(uint64_t) {}
inline void esp_deep_sleep_start() {}
template<class T> T min(T a, T b) { return std::min(a, b); }
