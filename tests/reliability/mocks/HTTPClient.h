#pragma once
#include <Arduino.h>
#include <cassert>
#include <deque>
#include <string>
constexpr int HTTPC_STRICT_FOLLOW_REDIRECTS = 1, HTTP_CODE_OK = 200;
struct MockResponse {
    int code = 200;
    std::string body;
    int length = -1;
    String transfer, encoding;
};
inline std::deque<MockResponse> responses;
inline unsigned requests = 0;
class WiFiClient {
  public:
    std::string body;
    size_t position = 0;
    bool stopped = false;
    virtual ~WiFiClient() = default;
    int available() { return int(body.size() - position); }
    bool connected() { return !stopped && position < body.size(); }
    int read(uint8_t *data, size_t size) {
        size = std::min(size, body.size() - position);
        memcpy(data, body.data() + position, size);
        position += size;
        return int(size);
    }
    void stop() { stopped = true; }
};
class HTTPClient {
    WiFiClient *client = nullptr;
    MockResponse response;
  public:
    void useHTTP10(bool value) { assert(value); }
    void setReuse(bool value) { assert(!value); }
    void setConnectTimeout(unsigned value) { assert(value <= 15000); }
    void setTimeout(unsigned value) { assert(value <= 15000); }
    void setFollowRedirects(int) {}
    void setRedirectLimit(unsigned value) { assert(value <= 3); }
    void collectHeaders(const char **, unsigned) {}
    bool begin(WiFiClient &value, const String &) { client = &value; return true; }
    int GET() {
        assert(!responses.empty());
        ++requests;
        response = responses.front();
        responses.pop_front();
        client->body = response.body;
        return response.code;
    }
    String header(const char *name) { return std::string(name) == "Transfer-Encoding" ? response.transfer : response.encoding; }
    WiFiClient *getStreamPtr() { return client; }
    int getSize() { return response.length; }
    void end() { assert(client && client->stopped); }
};
