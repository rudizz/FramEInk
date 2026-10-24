#pragma once
#include "IPAddress.h"
#include <algorithm>
#include <deque>
#include <vector>

class WiFiUDP {
  public:
    inline static WiFiUDP *lastCreated = nullptr;
    WiFiUDP() { lastCreated = this; }
    bool active = false;
    uint8_t beginResult = 1;
    uint16_t listeningPort = 0, replyPort = 0;
    unsigned polls = 0, discarded = 0;
    std::deque<std::vector<uint8_t>> incoming;
    std::vector<std::vector<uint8_t>> replies;
    std::vector<uint8_t> outgoing;
    uint8_t begin(uint16_t port) { listeningPort = port; active = beginResult == 1; return beginResult; }
    void stop() { active = false; }
    int parsePacket() { ++polls; return incoming.empty() ? 0 : int(incoming.front().size()); }
    void flush() { incoming.pop_front(); ++discarded; }
    IPAddress remoteIP() { return {192, 168, 4, 2}; }
    uint16_t remotePort() { return 53000; }
    int read(uint8_t *target, size_t capacity) {
        const size_t size = std::min(capacity, incoming.front().size());
        std::copy_n(incoming.front().begin(), size, target);
        incoming.pop_front();
        return size;
    }
    int beginPacket(IPAddress, uint16_t port) { replyPort = port; outgoing.clear(); return 1; }
    size_t write(const uint8_t *data, size_t size) { outgoing.assign(data, data + size); return size; }
    int endPacket() { replies.push_back(outgoing); return 1; }
};
