#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace frameink {

enum class BodyStatus { Ok, TooLarge, Timeout, Incomplete, Invalid };
struct BodyResult { BodyStatus status; size_t size; };

// HTTP/1.0 identity bodies only. Callers reject other encodings before reading.
// Always reserve a terminator, including when Content-Length is absent/incorrect.
template<class Client>
BodyResult readHttpBody(Client &client, char *data, size_t capacity, int contentLength,
                        uint32_t totalTimeoutMs = 45000, uint32_t idleTimeoutMs = 8000)
{
    if (!data || capacity == 0) return {BodyStatus::Invalid, 0};
    data[0] = 0;
    if (contentLength >= 0 && static_cast<size_t>(contentLength) >= capacity)
        return {BodyStatus::TooLarge, 0};
    const uint32_t started = millis();
    uint32_t lastData = started;
    size_t size = 0;
    for (;;)
    {
        if (contentLength >= 0 && size == static_cast<size_t>(contentLength))
            return {BodyStatus::Ok, size};
        const uint32_t now = millis();
        if (now - started >= totalTimeoutMs || now - lastData >= idleTimeoutMs)
            return {BodyStatus::Timeout, size};
        const int available = client.available();
        if (available > 0)
        {
            if (size == capacity - 1) return {BodyStatus::TooLarge, size};
            size_t count = static_cast<size_t>(available);
            if (count > 1024) count = 1024;
            if (count > capacity - 1 - size) count = capacity - 1 - size;
            if (contentLength >= 0 && count > static_cast<size_t>(contentLength) - size)
                count = static_cast<size_t>(contentLength) - size;
            const int received = client.read(reinterpret_cast<uint8_t *>(data + size), count);
            if (received > 0)
            {
                size += static_cast<size_t>(received);
                data[size] = 0;
                lastData = now;
            }
        }
        else if (!client.connected())
            return {contentLength < 0 ? BodyStatus::Ok : BodyStatus::Incomplete, size};
        delay(1); // Feed the idle task even during a large, continuously available response.
    }
}

} // namespace frameink
