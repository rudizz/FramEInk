#pragma once
#include "../../wifi/mocks/Arduino.h"

constexpr int INPUT = 0;
constexpr int HIGH = 1;
extern int testInterruptLevel;
inline void pinMode(int pin, int mode) { assert(pin == 39 && mode == INPUT); }
inline int digitalRead(int pin) { assert(pin == 39); return testInterruptLevel; }
void configTime(long, int, const char *, const char *);
