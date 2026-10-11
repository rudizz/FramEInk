#pragma once

#include <cstdint>

namespace frameink {
// A valid retained date is NOT proof of a fresh NTP response.
bool synchronizeNetworkTime(uint32_t timeoutMs = 15000);
} // namespace frameink
