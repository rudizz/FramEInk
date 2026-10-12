#pragma once
#include "../../photo_upload/mocks/Arduino.h"
#include <cassert>
#include <functional>

using uint = unsigned int;
extern std::function<long(long)> testRandom;
inline long random(long maximum)
{
    assert(maximum > 0 && testRandom);
    const long value = testRandom(maximum);
    assert(value >= 0 && value < maximum);
    return value;
}
struct TestSerial
{
    void println(const char *) {}
    template<class... Args> void printf(const char *, Args...) {}
};
inline TestSerial Serial;
