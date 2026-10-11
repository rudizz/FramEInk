#pragma once

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

extern uint32_t testMilliseconds;
extern void (*testClockAdvanced)();

inline unsigned long millis() { return testMilliseconds; }
inline void delay(unsigned long milliseconds)
{
    assert(milliseconds > 0 && milliseconds <= 100);
    testMilliseconds += static_cast<uint32_t>(milliseconds);
    if (testClockAdvanced)
        testClockAdvanced();
}

struct TestSerial
{
    std::string output;

    void printf(const char *format, ...)
    {
        char buffer[1024];
        va_list arguments;
        va_start(arguments, format);
        const int length = vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);
        assert(length >= 0 && static_cast<size_t>(length) < sizeof(buffer));
        output += buffer;
    }
};

extern TestSerial Serial;
