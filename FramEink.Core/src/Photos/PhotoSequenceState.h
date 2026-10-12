#pragma once

#include <cstddef>

namespace frameink {
constexpr size_t photoFileNameSize = 100;

// POD retained in RTC memory across deep sleep. No constructor/default member
// initializer may clear the last name on every wake. Empty means first photo.
struct PhotoSequenceState
{
    char lastFileName[photoFileNameSize];
};
} // namespace frameink
