#pragma once

#include <cstddef>
#include <cstdint>

namespace frameink {

// Build a small authoritative DNS reply in place. No allocation or name copying:
// answer IPv4 lookups with the AP address, and other types (e.g. AAAA/HTTPS)
// with an empty answer so clients can fall back to IPv4. TTL zero avoids keeping
// these local answers when the phone switches back to its normal network.
inline size_t captiveDnsReply(uint8_t *packet, size_t length, size_t capacity, const uint8_t address[4])
{
    constexpr size_t header = 12;
    constexpr size_t answer = 16;
    if (length < header || length > capacity || (packet[2] & 0x80)) return 0;
    if ((packet[2] & 0x78) || packet[4] != 0 || packet[5] != 1 ||
        packet[6] != 0 || packet[7] != 0 || packet[8] != 0 || packet[9] != 0)
        return 0; // Standard query, exactly one question, no answer/authority.

    size_t end = header;
    while (true)
    {
        if (end >= length) return 0;
        const uint8_t label = packet[end++];
        if (label == 0) break;
        // A first question has no earlier name to compress. Reject pointers,
        // reserved label forms, truncated names and names exceeding 255 bytes.
        if (label > 63 || label > length - end || end - header + label >= 255) return 0;
        end += label;
    }
    if (length - end < 4) return 0;
    const uint16_t type = (uint16_t(packet[end]) << 8) | packet[end + 1];
    const uint16_t dnsClass = (uint16_t(packet[end + 2]) << 8) | packet[end + 3];
    end += 4;
    const bool ipv4 = (type == 1 || type == 255) && (dnsClass == 1 || dnsClass == 255);
    if (ipv4 && capacity - end < answer) return 0;

    packet[2] = 0x84 | (packet[2] & 1); // Response, authoritative, preserve RD.
    packet[3] = 0; // No error; no claim of recursive Internet access.
    packet[6] = 0;
    packet[7] = ipv4 ? 1 : 0;
    packet[8] = packet[9] = packet[10] = packet[11] = 0;
    // Discard any EDNS additional records; the response fits the classic 512 B.
    if (ipv4)
    {
        const uint8_t record[12] = {0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 0, 0, 0, 4};
        for (uint8_t byte : record) packet[end++] = byte;
        for (size_t i = 0; i < 4; ++i) packet[end++] = address[i];
    }
    return end;
}

} // namespace frameink
