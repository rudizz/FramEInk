#pragma once
#include "../../photo_upload/mocks/Arduino.h"
#include <cstdarg>
#define RTC_NOINIT_ATTR
struct TestSerial {
    std::string output;
    void printf(const char *format, ...) {
        char line[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(line, sizeof(line), format, args);
        va_end(args);
        output += line;
    }
};
inline TestSerial Serial;
struct TestESP {
    unsigned getFreeHeap() { return 100000; }
    unsigned getMinFreeHeap() { return 90000; }
    unsigned getMaxAllocHeap() { return 80000; }
    unsigned getFreePsram() { return 3000000; }
};
inline TestESP ESP;
inline unsigned uxTaskGetStackHighWaterMark(void *) { return 4096; }
