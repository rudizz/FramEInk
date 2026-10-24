#pragma once

#include <WiFiUdp.h>
#include "CaptiveDnsReply.h"

namespace frameink {

class CaptivePortalDns
{
  public:
    bool start(const IPAddress &address)
    {
        stop();
        for (size_t i = 0; i < 4; ++i) address_[i] = address[i];
        active_ = udp_.begin(53) == 1;
        return active_;
    }

    void processNextRequest()
    {
        if (!active_) return;
        const int size = udp_.parsePacket();
        if (size <= 0) return;
        if (size > int(sizeof(packet_)))
        {
            udp_.flush();
            return;
        }
        const IPAddress sender = udp_.remoteIP();
        const uint16_t port = udp_.remotePort();
        if (udp_.read(packet_, sizeof(packet_)) != size) return;
        const size_t reply = captiveDnsReply(packet_, size, sizeof(packet_), address_);
        if (reply && udp_.beginPacket(sender, port))
        {
            udp_.write(packet_, reply);
            udp_.endPacket();
        }
    }

    void stop()
    {
        udp_.stop();
        active_ = false;
    }

  private:
    WiFiUDP udp_;
    uint8_t address_[4] = {};
    uint8_t packet_[512] = {};
    bool active_ = false;
};

} // namespace frameink
