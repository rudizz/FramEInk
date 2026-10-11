#pragma once

#include <Arduino.h>
#include <limits>
#include <string>
#include <vector>

enum wl_status_t
{
    WL_IDLE_STATUS = 0, WL_NO_SSID_AVAIL = 1, WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4, WL_CONNECTION_LOST = 5, WL_DISCONNECTED = 6
};
constexpr int WIFI_STA = 1;
enum arduino_event_id_t { ARDUINO_EVENT_WIFI_STA_DISCONNECTED };
struct arduino_event_t
{
    struct { struct { uint8_t reason; } wifi_sta_disconnected; } event_info;
};
using WiFiEventSysCb = void (*)(arduino_event_t *);

// Numeric values from the installed Inkplate Boards 2.0.0 esp_wifi_types.h.
enum wifi_err_reason_t
{
    WIFI_REASON_UNSPECIFIED = 1, WIFI_REASON_AUTH_EXPIRE = 2, WIFI_REASON_AUTH_LEAVE = 3,
    WIFI_REASON_ASSOC_EXPIRE = 4, WIFI_REASON_ASSOC_TOOMANY = 5, WIFI_REASON_NOT_AUTHED = 6,
    WIFI_REASON_NOT_ASSOCED = 7, WIFI_REASON_ASSOC_LEAVE = 8, WIFI_REASON_ASSOC_NOT_AUTHED = 9,
    WIFI_REASON_DISASSOC_PWRCAP_BAD = 10, WIFI_REASON_DISASSOC_SUPCHAN_BAD = 11,
    WIFI_REASON_BSS_TRANSITION_DISASSOC = 12, WIFI_REASON_IE_INVALID = 13,
    WIFI_REASON_MIC_FAILURE = 14, WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT = 15,
    WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT = 16, WIFI_REASON_IE_IN_4WAY_DIFFERS = 17,
    WIFI_REASON_GROUP_CIPHER_INVALID = 18, WIFI_REASON_PAIRWISE_CIPHER_INVALID = 19,
    WIFI_REASON_AKMP_INVALID = 20, WIFI_REASON_UNSUPP_RSN_IE_VERSION = 21,
    WIFI_REASON_INVALID_RSN_IE_CAP = 22, WIFI_REASON_802_1X_AUTH_FAILED = 23,
    WIFI_REASON_CIPHER_SUITE_REJECTED = 24, WIFI_REASON_INVALID_PMKID = 53,
    WIFI_REASON_BEACON_TIMEOUT = 200, WIFI_REASON_NO_AP_FOUND = 201,
    WIFI_REASON_AUTH_FAIL = 202, WIFI_REASON_ASSOC_FAIL = 203,
    WIFI_REASON_HANDSHAKE_TIMEOUT = 204, WIFI_REASON_CONNECTION_FAIL = 205,
    WIFI_REASON_AP_TSF_RESET = 206, WIFI_REASON_ROAMING = 207
};

struct TestIP { std::string toString() const { return "192.0.2.42"; } };

struct TestWiFi
{
    static constexpr uint32_t never = std::numeric_limits<uint32_t>::max();
    WiFiEventSysCb handler = nullptr;
    unsigned registrations = 0;
    unsigned begins = 0;
    unsigned modeCalls = 0;
    bool modeResult = true;
    bool reconnectResult = true;
    bool autoReconnect = false;
    bool connected = false;
    bool active = false;
    uint32_t attemptStarted = 0;
    uint32_t associatedAfter = never;
    std::vector<uint32_t> ipDelays{never};
    std::vector<uint32_t> retries;
    std::string ssid;
    std::string password;

    void reset()
    {
        // Diagnostics must be registered exactly once across all connections.
        const auto savedHandler = handler;
        const auto savedRegistrations = registrations;
        *this = TestWiFi{};
        handler = savedHandler;
        registrations = savedRegistrations;
    }

    size_t onEvent(WiFiEventSysCb callback, arduino_event_id_t event)
    {
        assert(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED && !handler);
        handler = callback;
        return ++registrations;
    }

    bool mode(int value) { assert(value == WIFI_STA); ++modeCalls; return modeResult; }
    void setAutoReconnect(bool enabled) { autoReconnect = enabled; }
    wl_status_t begin(const char *network, const char *secret)
    {
        assert(autoReconnect);
        ssid = network;
        password = secret;
        ++begins;
        active = true;
        attemptStarted = testMilliseconds;
        return status();
    }

    wl_status_t status()
    {
        const size_t index = retries.size();
        const uint32_t ipDelay = index < ipDelays.size() ? ipDelays[index] : never;
        const uint32_t elapsed = testMilliseconds - attemptStarted;
        if (active && ipDelay != never && elapsed >= ipDelay)
            connected = true;
        if (connected)
            return WL_CONNECTED;
        if (active && elapsed >= associatedAfter)
            return WL_IDLE_STATUS; // Associated but still waiting for DHCP.
        return WL_DISCONNECTED;
    }

    bool reconnect()
    {
        assert(!connected);
        retries.push_back(testMilliseconds);
        attemptStarted = testMilliseconds;
        active = reconnectResult;
        emit(WIFI_REASON_ASSOC_LEAVE);
        return reconnectResult;
    }

    void emit(uint8_t reason)
    {
        assert(handler);
        arduino_event_t event{};
        event.event_info.wifi_sta_disconnected.reason = reason;
        handler(&event);
    }

    TestIP localIP() const { return {}; }
    int32_t RSSI() const { return -61; }
    int32_t channel() const { return 6; }
};

extern TestWiFi WiFi;
