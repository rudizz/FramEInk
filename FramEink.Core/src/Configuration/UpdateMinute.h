#pragma once

#include <cstdint>
#include <cstring>

namespace frameink {

constexpr uint8_t defaultUpdateMinute = 0;

inline bool parseUpdateMinute(const char *text, uint8_t &minute)
{
    if (text == nullptr)
        return false;
    const auto length = std::strlen(text);
    if (length < 1 || length > 2)
        return false;
    unsigned value = 0;
    for (unsigned i = 0; i < length; ++i)
    {
        if (text[i] < '0' || text[i] > '9')
            return false;
        value = value * 10 + unsigned(text[i] - '0');
    }
    if (value > 59)
        return false;
    minute = static_cast<uint8_t>(value);
    return true;
}

// Optional extension AFTER the existing night-pause block. Old credentials
// and pause data stay byte-for-byte compatible; missing/corrupt data uses :00.
constexpr int updateMinuteStorageBytes = 5;

template<class Storage>
uint8_t readUpdateMinute(Storage &storage, int offset, int capacity)
{
    if (offset < 0 || offset > capacity - updateMinuteStorageBytes ||
        storage.read(offset) != 'F' || storage.read(offset + 1) != 'M' || storage.read(offset + 2) != 1)
        return defaultUpdateMinute;
    const uint8_t minute = storage.read(offset + 3);
    const uint8_t check = uint8_t('F' ^ 'M' ^ 1 ^ minute);
    return minute < 60 && check == storage.read(offset + 4) ? minute : defaultUpdateMinute;
}

template<class Storage>
bool writeUpdateMinute(Storage &storage, int offset, int capacity, uint8_t minute)
{
    if (minute > 59 || offset < 0 || offset > capacity - updateMinuteStorageBytes)
        return false;
    const uint8_t bytes[] = {'F', 'M', 1, minute, uint8_t('F' ^ 'M' ^ 1 ^ minute)};
    for (uint8_t byte : bytes)
        storage.write(offset++, byte);
    return true;
}

} // namespace frameink
