#include "WiFiAPSettings.h"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>

namespace {
std::vector<uint8_t> query(uint16_t type = 1, const std::vector<std::string> &labels = {"captive", "apple", "com"})
{
    std::vector<uint8_t> packet = {0x12, 0x34, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0};
    for (const auto &label : labels)
    {
        packet.push_back(label.size());
        packet.insert(packet.end(), label.begin(), label.end());
    }
    packet.insert(packet.end(), {0, uint8_t(type >> 8), uint8_t(type), 0, 1});
    return packet;
}

void dnsReplies()
{
    const uint8_t address[] = {192, 168, 4, 1};
    const auto request = query();
    std::array<uint8_t, 512> packet{};
    std::copy(request.begin(), request.end(), packet.begin());
    size_t size = frameink::captiveDnsReply(packet.data(), request.size(), packet.size(), address);
    assert(size == request.size() + 16);
    assert(packet[0] == 0x12 && packet[1] == 0x34 && packet[2] == 0x85 && packet[3] == 0);
    assert(packet[5] == 1 && packet[7] == 1 && packet[11] == 0);
    assert(std::equal(request.begin() + 12, request.end(), packet.begin() + 12));
    const std::vector<uint8_t> answer = {0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 0, 0, 0, 4, 192, 168, 4, 1};
    assert(std::equal(answer.begin(), answer.end(), packet.begin() + request.size()));
    const auto any = query(255);
    std::copy(any.begin(), any.end(), packet.begin());
    assert(frameink::captiveDnsReply(packet.data(), any.size(), packet.size(), address) == any.size() + 16);
    assert(packet[7] == 1);

    // IPv6/HTTPS records must not receive an invalid IPv4 answer. They should
    // complete promptly with NODATA while A lookups still reach the portal.
    for (uint16_t type : {28, 65, 16})
    {
        const auto other = query(type);
        std::copy(other.begin(), other.end(), packet.begin());
        size = frameink::captiveDnsReply(packet.data(), other.size(), packet.size(), address);
        assert(size == other.size() && packet[3] == 0 && packet[7] == 0);
    }

    // EDNS is common on phones: retain the question but omit the OPT record.
    auto edns = request;
    edns[11] = 1;
    edns.insert(edns.end(), {0, 0, 41, 4, 208, 0, 0, 0, 0, 0, 0});
    std::copy(edns.begin(), edns.end(), packet.begin());
    size = frameink::captiveDnsReply(packet.data(), edns.size(), packet.size(), address);
    assert(size == request.size() + 16 && packet[11] == 0);

    // Maximum legal name, including names longer than 127 bytes.
    const auto longest = query(1, {std::string(63, 'a'), std::string(63, 'b'),
                                   std::string(63, 'c'), std::string(61, 'd')});
    std::copy(longest.begin(), longest.end(), packet.begin());
    assert(frameink::captiveDnsReply(packet.data(), longest.size(), packet.size(), address) == longest.size() + 16);

    for (size_t length = 0; length < request.size(); ++length)
    {
        std::copy(request.begin(), request.end(), packet.begin());
        assert(frameink::captiveDnsReply(packet.data(), length, packet.size(), address) == 0);
    }
    for (const auto &invalid : std::vector<std::pair<size_t, uint8_t>>{
             {2, 0x81}, {2, 0x09}, {5, 0}, {5, 2}, {7, 1}, {9, 1}, {12, 64}, {12, 0xc0}})
    {
        std::copy(request.begin(), request.end(), packet.begin());
        packet[invalid.first] = invalid.second;
        assert(frameink::captiveDnsReply(packet.data(), request.size(), packet.size(), address) == 0);
    }
    const auto oversizedName = query(1, {std::string(63, 'a'), std::string(63, 'b'),
                                         std::string(63, 'c'), std::string(62, 'd')});
    std::copy(oversizedName.begin(), oversizedName.end(), packet.begin());
    assert(frameink::captiveDnsReply(packet.data(), oversizedName.size(), packet.size(), address) == 0);
    std::copy(request.begin(), request.end(), packet.begin());
    assert(frameink::captiveDnsReply(packet.data(), request.size(), request.size() + 15, address) == 0);
    assert(frameink::captiveDnsReply(packet.data(), request.size(), request.size() - 1, address) == 0);

    // Exercise damaged packets under ASan/UBSan, including label/length mutations.
    uint32_t random = 0x12345678;
    for (unsigned iteration = 0; iteration < 20000; ++iteration)
    {
        std::copy(request.begin(), request.end(), packet.begin());
        random = random * 1664525u + 1013904223u;
        packet[(random >> 16) % packet.size()] = random >> 24;
        const size_t length = random % (packet.size() + 1);
        const size_t reply = frameink::captiveDnsReply(packet.data(), length, packet.size(), address);
        assert(reply <= packet.size());
    }
}

void dnsSocket()
{
    frameink::CaptivePortalDns dns;
    WiFiUDP &udp = *WiFiUDP::lastCreated;
    assert(dns.start({192, 168, 4, 1}) && udp.active && udp.listeningPort == 53);
    udp.incoming.push_back(query());
    dns.processNextRequest();
    assert(udp.replies.size() == 1 && udp.replyPort == 53000);
    assert(udp.replies.back().back() == 1);
    udp.incoming.emplace_back(513, 0);
    dns.processNextRequest();
    assert(udp.discarded == 1 && udp.replies.size() == 1);
    udp.incoming.push_back({1, 2, 3});
    dns.processNextRequest();
    assert(udp.replies.size() == 1);
    udp.incoming.push_back(query());
    dns.processNextRequest();
    assert(udp.replies.size() == 2);
    dns.stop();
    const unsigned polls = udp.polls;
    dns.processNextRequest();
    assert(!udp.active && udp.polls == polls);
    udp.beginResult = 0;
    assert(!dns.start({192, 168, 4, 1}));
    dns.processNextRequest();
    assert(udp.polls == polls);
}

void portalRoutesAndLifetime()
{
    WiFiAPSettingsClass portal;
    portal.SSID_User = "Configured network";
    portal.initAP();
    WebServer &server = *WebServer::lastCreated;
    WiFiUDP &udp = *WiFiUDP::lastCreated;
    for (const char *path : {"/hotspot-detect.html", "/library/test/success.html",
                            "/generate_204", "/gen_204", "/connecttest.txt", "/ncsi.txt",
                            "/redirect", "/fwlink", "/success.txt", "/canonical.html", "/unknown"})
    {
        server.call(path);
        assert(server.status == 302);
        assert(server.responseHeaders.at("Location") == "http://192.168.4.1/");
        assert(server.responseHeaders.at("Cache-Control") == "no-store");
    }
    server.call("/generate_204", HTTP_HEAD);
    assert(server.status == 302);
    for (const auto &request : std::vector<std::pair<const char *, HTTPMethod>>{
             {"/api/missing", HTTP_GET}, {"/api", HTTP_GET}, {"/unknown", HTTP_POST}, {"/api/settings", HTTP_HEAD}})
    {
        server.call(request.first, request.second);
        assert(server.status == 404 && server.responseHeaders.count("Location") == 0);
    }
    for (const char *path : {"/", "/foto", "/portal.js"})
    {
        server.call(path);
        assert(server.status == 200 && server.responseHeaders.count("Location") == 0);
        assert(server.responseHeaders.at("Content-Encoding") == "gzip");
    }
    server.call("/api/settings");
    assert(server.status == 200 && server.responseHeaders.count("Location") == 0);
    server.call("/api/ping", HTTP_POST);
    assert(server.status == 204);

    // Repeated connectivity probes must not keep an unattended frame awake.
    const unsigned long started = millis();
    udp.incoming.push_back(query());
    server.onHandleClient = [&]() {
        server.call("/generate_204");
        server.call("/"); // A probe following the redirect still is not activity.
        testMillis += 60000;
    };
    portal.loop();
    assert(millis() - started >= 600000 && millis() - started < 660000);
    assert(!server.active && !udp.active && !WiFi.active && udp.replies.size() == 1);

    // Finish still closes both listeners; DNS bind failure leaves direct IP access.
    portal.initAP();
    server.onHandleClient = [&]() { server.call("/api/finish", HTTP_POST); };
    portal.loop();
    assert(server.status == 200 && !server.active && !udp.active && !WiFi.active);

    // Real interactions, including conversion/upload heartbeats, still extend
    // the idle deadline beyond ten minutes until Finish is pressed.
    portal.initAP();
    const unsigned long interactiveStart = millis();
    unsigned interactions = 0;
    server.onHandleClient = [&]() {
        if (interactions++ < 3)
        {
            testMillis += 300000;
            server.call("/api/ping", HTTP_POST);
        }
        else server.call("/api/finish", HTTP_POST);
    };
    portal.loop();
    assert(interactions == 4 && millis() - interactiveStart > 600000);
    assert(!server.active && !udp.active && !WiFi.active);
    udp.beginResult = 0;
    portal.initAP();
    server.call("/");
    assert(server.status == 200 && server.active && !udp.active);
    server.onHandleClient = [&]() { server.call("/api/finish", HTTP_POST); };
    portal.loop();
    assert(!server.active && !udp.active && !WiFi.active);
}
} // namespace

int main()
{
    dnsReplies();
    dnsSocket();
    portalRoutesAndLifetime();
    std::cout << "Captive portal: bounded DNS A/AAAA/EDNS, OS probe redirects, Settings/assets, idle timeout and shutdown passed\n";
}
