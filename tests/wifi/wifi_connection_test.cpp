#include "Connectivity/WiFiConnection.h"
#include <WiFi.h>

#include <cassert>
#include <iostream>
#include <limits>

uint32_t testMilliseconds = 0;
void (*testClockAdvanced)() = nullptr;
TestSerial Serial;
TestWiFi WiFi;

namespace {

constexpr const char *ssid = "private-network-name";
constexpr const char *password = "private-network-password";

void reset(uint32_t now = 0)
{
    testMilliseconds = now;
    testClockAdvanced = nullptr;
    WiFi.reset();
    Serial.output.clear();
}

bool connect(uint32_t timeout = 120000)
{
    const bool result = frameink::ensureWiFiConnected(ssid, password, timeout);
    assert(WiFi.registrations == 1);
    assert(Serial.output.find(ssid) == std::string::npos);
    assert(Serial.output.find(password) == std::string::npos);
    return result;
}

void slowConnectionAndDhcpTests()
{
    // These would have been interrupted by the old 1/6-second reconnect loop.
    for (uint32_t duration : {0u, 100u, 6000u, 12000u, 29900u, 30000u})
    {
        reset();
        WiFi.ipDelays = {duration};
        WiFi.associatedAfter = 100;
        assert(connect());
        assert(testMilliseconds == duration);
        assert(WiFi.begins == 1 && WiFi.retries.empty());
        assert(WiFi.ssid == ssid && WiFi.password == password);
        assert(Serial.output.find("IP=192.0.2.42, RSSI=-61 dBm, channel=6") != std::string::npos);
    }
}

void retryAndDeadlineTests()
{
    reset();
    WiFi.ipDelays = {TestWiFi::never, 1800};
    assert(connect());
    assert(testMilliseconds == 31800 && WiFi.begins == 1);
    assert(WiFi.retries == std::vector<uint32_t>{30000});
    assert(Serial.output.find("Attempt 1 timed out; starting attempt 2") != std::string::npos);

    reset();
    assert(!connect());
    assert(testMilliseconds == 120000);
    assert(WiFi.retries == (std::vector<uint32_t>{30000, 60000, 90000}));
    assert(Serial.output.find("timed out after 120000 ms") != std::string::npos);

    reset();
    WiFi.reconnectResult = false;
    assert(!connect());
    assert(testMilliseconds == 120000 && WiFi.retries.size() == 3);
    assert(Serial.output.find("Could not start reconnect") != std::string::npos);

    // A custom overall timeout must not get rounded up to the next poll/attempt.
    reset();
    assert(!connect(31555));
    assert(testMilliseconds == 31555 && WiFi.retries.size() == 1);
    reset();
    assert(!connect(1));
    assert(testMilliseconds == 1 && WiFi.retries.empty());
    reset();
    assert(!connect(0));
    assert(WiFi.begins == 0 && WiFi.modeCalls == 0 && testMilliseconds == 0);
}

void existingConnectionAndFailureTests()
{
    reset();
    WiFi.connected = true;
    assert(connect());
    assert(WiFi.modeCalls == 0 && WiFi.begins == 0 && WiFi.retries.empty());
    assert(testMilliseconds == 0);

    reset();
    WiFi.modeResult = false;
    assert(!connect());
    assert(WiFi.begins == 0 && WiFi.retries.empty());
    assert(Serial.output.find("Could not enable station mode") != std::string::npos);

    // A real failure can be logged while waiting. The driver's automatic retry
    // can recover without the application forcibly disconnecting an active join.
    reset();
    WiFi.ipDelays = {10000};
    testClockAdvanced = [] {
        if (testMilliseconds == 2000)
            WiFi.emit(WIFI_REASON_AUTH_EXPIRE);
    };
    assert(connect());
    assert(WiFi.autoReconnect && WiFi.retries.empty() && testMilliseconds == 10000);
    assert(Serial.output.find("reason=2 (AUTH_EXPIRE: authentication timed out)") != std::string::npos);

    // A later calendar/weather reconnect uses the same handler and the same
    // uninterrupted waiting policy, rather than registering duplicate logs.
    WiFi.connected = false;
    WiFi.ipDelays = {14000};
    assert(connect());
    assert(WiFi.begins == 2 && WiFi.retries.empty() && testMilliseconds == 24000);
}

void wraparoundTests()
{
    const uint32_t start = std::numeric_limits<uint32_t>::max() - 2000;
    reset(start);
    WiFi.ipDelays = {12000};
    assert(connect());
    assert(static_cast<uint32_t>(testMilliseconds - start) == 12000);
    assert(WiFi.retries.empty());

    reset(start);
    assert(!connect());
    assert(static_cast<uint32_t>(testMilliseconds - start) == 120000);
    assert(WiFi.retries.size() == 3);
    for (size_t i = 0; i < WiFi.retries.size(); ++i)
        assert(static_cast<uint32_t>(WiFi.retries[i] - start) == (i + 1) * 30000);
}

void diagnosticTests()
{
    reset();
    for (unsigned reason = 0; reason < 256; ++reason)
    {
        Serial.output.clear();
        WiFi.emit(static_cast<uint8_t>(reason));
        assert(Serial.output.find("reason=" + std::to_string(reason) + " (") != std::string::npos);
        const bool known = (reason >= 1 && reason <= 24) || reason == 53 || (reason >= 200 && reason <= 207);
        assert((Serial.output.find("UNKNOWN") == std::string::npos) == known);
    }
    Serial.output.clear();
    WiFi.emit(WIFI_REASON_NO_AP_FOUND);
    assert(Serial.output.find("reason=201 (NO_AP_FOUND: network not found)") != std::string::npos);
    Serial.output.clear();
    WiFi.emit(WIFI_REASON_AUTH_FAIL);
    assert(Serial.output.find("reason=202 (AUTH_FAIL: authentication failed)") != std::string::npos);
    assert(WiFi.registrations == 1);
}

} // namespace

int main()
{
    slowConnectionAndDhcpTests();
    retryAndDeadlineTests();
    existingConnectionAndFailureTests();
    wraparoundTests();
    diagnosticTests();
    std::cout << "Wi-Fi connection tests passed\n";
}
