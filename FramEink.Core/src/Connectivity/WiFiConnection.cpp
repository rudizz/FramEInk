#include "WiFiConnection.h"

#include <Arduino.h>
#include <WiFi.h>

namespace frameink {
namespace {

constexpr uint32_t attemptTimeoutMs = 30000;
constexpr uint32_t pollIntervalMs = 100;

const char *disconnectReason(uint8_t reason)
{
    switch (reason)
    {
    case WIFI_REASON_UNSPECIFIED: return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE: authentication timed out";
    case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE: association timed out";
    case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY: access point full";
    case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
    case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE: connection closed";
    case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "DISASSOC_PWRCAP_BAD";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "DISASSOC_SUPCHAN_BAD";
    case WIFI_REASON_BSS_TRANSITION_DISASSOC: return "BSS_TRANSITION_DISASSOC";
    case WIFI_REASON_IE_INVALID: return "IE_INVALID";
    case WIFI_REASON_MIC_FAILURE: return "MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT: security handshake timed out";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP_KEY_UPDATE_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "IE_IN_4WAY_DIFFERS";
    case WIFI_REASON_GROUP_CIPHER_INVALID: return "GROUP_CIPHER_INVALID";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID: return "AKMP_INVALID";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "UNSUPP_RSN_IE_VERSION";
    case WIFI_REASON_INVALID_RSN_IE_CAP: return "INVALID_RSN_IE_CAP";
    case WIFI_REASON_802_1X_AUTH_FAILED: return "802_1X_AUTH_FAILED";
    case WIFI_REASON_CIPHER_SUITE_REJECTED: return "CIPHER_SUITE_REJECTED";
    case WIFI_REASON_INVALID_PMKID: return "INVALID_PMKID";
    case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT: access point no longer responding";
    case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND: network not found";
    case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL: authentication failed";
    case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL: association failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT: security handshake timed out";
    case WIFI_REASON_CONNECTION_FAIL: return "CONNECTION_FAIL";
    case WIFI_REASON_AP_TSF_RESET: return "AP_TSF_RESET";
    case WIFI_REASON_ROAMING: return "ROAMING";
    default: return "UNKNOWN: see numeric reason code";
    }
}

// Called on the ESP32 event task. Serial.printf is thread-safe; there is no
// shared mutable application state and no Wi-Fi API call inside this callback.
void logDisconnect(arduino_event_t *event)
{
    const uint8_t reason = event->event_info.wifi_sta_disconnected.reason;
    Serial.printf("[WiFi] Disconnected: reason=%u (%s).\n",
                  static_cast<unsigned>(reason), disconnectReason(reason));
}

void registerDiagnostics()
{
    // Network may reconnect between weather/calendar downloads. Do not register
    // a new handler for each call or capture a Network instance's lifetime.
    static bool registered = false;
    if (!registered)
    {
        WiFi.onEvent(logDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        registered = true;
    }
}

} // namespace

bool ensureWiFiConnected(const char *ssid, const char *password, uint32_t timeoutMs)
{
    registerDiagnostics();
    if (WiFi.status() == WL_CONNECTED)
        return true;

    if (timeoutMs == 0)
        return false;

    const uint32_t started = millis();
    if (!WiFi.mode(WIFI_STA))
    {
        Serial.printf("[WiFi] Could not enable station mode.\n");
        return false;
    }

    // Let the driver retry after actual disconnect/failure events. The wait
    // loop must not abort scanning, authentication or DHCP every few seconds.
    WiFi.setAutoReconnect(true);
    Serial.printf("[WiFi] Connecting (timeout %lu ms, attempt timeout %lu ms)...\n",
                  static_cast<unsigned long>(timeoutMs),
                  static_cast<unsigned long>(attemptTimeoutMs));
    WiFi.begin(ssid, password);
    uint32_t attemptStarted = millis();
    unsigned attempt = 1;

    for (;;)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.printf("[WiFi] Connected after %lu ms: IP=%s, RSSI=%ld dBm, channel=%ld.\n",
                          static_cast<unsigned long>(static_cast<uint32_t>(millis()) - started),
                          WiFi.localIP().toString().c_str(),
                          static_cast<long>(WiFi.RSSI()), static_cast<long>(WiFi.channel()));
            return true;
        }

        const uint32_t now = millis();
        const uint32_t elapsed = now - started;
        if (elapsed >= timeoutMs)
        {
            Serial.printf("[WiFi] Connection timed out after %lu ms (status=%d).\n",
                          static_cast<unsigned long>(elapsed), static_cast<int>(WiFi.status()));
            return false;
        }

        if (now - attemptStarted >= attemptTimeoutMs)
        {
            // reconnect() disconnects first: use it ONLY after an attempt has
            // timed out, never on a short periodic poll. Re-check for an IP
            // acquired while evaluating the deadline before disconnecting.
            if (WiFi.status() == WL_CONNECTED)
                continue;

            Serial.printf("[WiFi] Attempt %u timed out; starting attempt %u.\n", attempt, attempt + 1);
            ++attempt;
            if (!WiFi.reconnect())
                Serial.printf("[WiFi] Could not start reconnect; waiting before another retry.\n");
            attemptStarted = millis();
            continue;
        }

        const uint32_t remaining = timeoutMs - elapsed;
        delay(remaining < pollIntervalMs ? remaining : pollIntervalMs);
    }
}

} // namespace frameink
