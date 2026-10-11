#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace frameink {

struct NightPause
{
    uint16_t startMinute = 23 * 60;
    uint16_t endMinute = 5 * 60;

    bool valid() const
    {
        // At least one full hour must remain awake, so any hourly phase has an
        // available slot. Equal start/end disables the pause, not a 24h sleep.
        return startMinute < 1440 && endMinute < 1440 &&
               (endMinute + 1440 - startMinute) % 1440 <= 1380;
    }

    bool contains(uint16_t minute) const
    {
        if (startMinute == endMinute)
            return false;
        return startMinute < endMinute ? minute >= startMinute && minute < endMinute
                                       : minute >= startMinute || minute < endMinute;
    }
};

inline bool parseClockTime(const char *text, uint16_t &minute)
{
    if (text == nullptr || std::strlen(text) != 5 || text[2] != ':')
        return false;
    for (size_t i = 0; i < 5; ++i)
        if (i != 2 && (text[i] < '0' || text[i] > '9'))
            return false;
    const unsigned hour = (text[0] - '0') * 10 + text[1] - '0';
    const unsigned minutes = (text[3] - '0') * 10 + text[4] - '0';
    if (hour > 23 || minutes > 59)
        return false;
    minute = static_cast<uint16_t>(hour * 60 + minutes);
    return true;
}

inline void formatClockTime(uint16_t minute, char (&text)[6])
{
    std::snprintf(text, sizeof(text), "%02u:%02u", unsigned(minute / 60), unsigned(minute % 60));
}

// Optional, versioned extension AFTER the five existing EEPROM strings.
// Existing installations retain their credentials; missing/invalid extensions
// use the six-hour default. No changes to the legacy string representation.
constexpr int nightPauseStorageBytes = 8;

template<class Storage>
NightPause readNightPause(Storage &storage, int offset, int capacity)
{
    NightPause pause;
    if (offset < 0 || offset > capacity - nightPauseStorageBytes ||
        storage.read(offset) != 'F' || storage.read(offset + 1) != 'N' || storage.read(offset + 2) != 1)
        return pause;
    const uint16_t start = (uint16_t(storage.read(offset + 3)) << 8) | storage.read(offset + 4);
    const uint16_t end = (uint16_t(storage.read(offset + 5)) << 8) | storage.read(offset + 6);
    uint8_t check = 0;
    for (int i = 0; i < nightPauseStorageBytes - 1; ++i)
        check ^= storage.read(offset + i);
    if (check != storage.read(offset + 7))
        return pause;
    NightPause decoded;
    decoded.startMinute = start;
    decoded.endMinute = end;
    return decoded.valid() ? decoded : pause;
}

template<class Storage>
bool writeNightPause(Storage &storage, int offset, int capacity, const NightPause &pause)
{
    if (!pause.valid() || offset < 0 || offset > capacity - nightPauseStorageBytes)
        return false;
    const uint8_t bytes[] = {'F', 'N', 1, uint8_t(pause.startMinute >> 8), uint8_t(pause.startMinute),
                            uint8_t(pause.endMinute >> 8), uint8_t(pause.endMinute)};
    uint8_t check = 0;
    for (uint8_t byte : bytes)
    {
        storage.write(offset++, byte);
        check ^= byte;
    }
    storage.write(offset, check);
    return true;
}

} // namespace frameink
