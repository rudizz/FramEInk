#pragma once
#include <sys/time.h>
#include <Arduino.h>

constexpr int SNTP_SYNC_MODE_IMMED = 0;
extern bool testSntpEnabled;
extern unsigned testSntpStops;
extern void (*testSyncCallback)(timeval *);
inline bool sntp_enabled() { return testSntpEnabled; }
inline void sntp_stop() { testSntpEnabled = false; ++testSntpStops; }
inline void sntp_set_time_sync_notification_cb(void (*callback)(timeval *)) { testSyncCallback = callback; }
inline void sntp_set_sync_mode(int mode) { assert(mode == SNTP_SYNC_MODE_IMMED); }
