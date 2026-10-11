#pragma once

#include <cstdint>

namespace frameink {

// Wait for association AND an IP address. A stalled attempt is restarted only
// after 30 seconds, within a single overall deadline (also across millis wrap).
// Disconnect events are logged without exposing the SSID or password.
bool ensureWiFiConnected(const char *ssid, const char *password, uint32_t timeoutMs = 120000);

} // namespace frameink
