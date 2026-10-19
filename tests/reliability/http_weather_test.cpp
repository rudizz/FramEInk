#include <Connectivity/BoundedHttpBody.h>
#include <Weather/WeatherParser.h>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <string>

struct Client {
    std::string body;
    size_t position = 0, fragment = 1024;
    bool stall = false;
    uint32_t nextByteAt = 0, interval = 0;
    int available() { return testMillis < nextByteAt ? 0 : int(std::min(fragment, body.size() - position)); }
    bool connected() { return stall || position < body.size(); }
    int read(uint8_t *out, size_t count) {
        count = std::min(count, body.size() - position);
        memcpy(out, body.data() + position, count);
        position += count;
        nextByteAt = testMillis + interval;
        return int(count);
    }
};

int main() {
    using namespace frameink;
    for (size_t fragment : {1, 2, 13, 29, 30, 1024}) {
        for (int declared : {-1, 40}) {
            char buffer[41];
            Client client{std::string(40, 'x'), 0, fragment};
            testMillis = 0;
            const auto result = readHttpBody(client, buffer, sizeof(buffer), declared);
            assert(result.status == BodyStatus::Ok && result.size == 40 && buffer[40] == 0);
        }
    }
    char buffer[32];
    Client oversize{std::string(32, 'x')};
    assert(readHttpBody(oversize, buffer, sizeof(buffer), 32).status == BodyStatus::TooLarge);
    assert(oversize.position == 0);
    assert(readHttpBody(oversize, buffer, sizeof(buffer), -1).status == BodyStatus::TooLarge);
    assert(buffer[31] == 0);
    Client shortBody{"short"};
    assert(readHttpBody(shortBody, buffer, sizeof(buffer), 31).status == BodyStatus::Incomplete);
    Client empty{""};
    assert(readHttpBody(empty, buffer, sizeof(buffer), 0).status == BodyStatus::Ok);
    Client stalled{"", 0, 1, true};
    testMillis = 0;
    assert(readHttpBody(stalled, buffer, sizeof(buffer), -1, 100, 8).status == BodyStatus::Timeout);
    assert(testMillis == 8);
    Client trickle{std::string(30, 'x'), 0, 1, true, 0, 7};
    testMillis = 0;
    assert(readHttpBody(trickle, buffer, sizeof(buffer), -1, 20, 8).status == BodyStatus::Timeout);
    assert(testMillis == 20); // Total budget wins even when data keeps arriving.
    testMillis = UINT32_MAX - 3;
    assert(readHttpBody(stalled, buffer, sizeof(buffer), -1, 100, 8).status == BodyStatus::Timeout);
    assert(testMillis == 4);

    auto parse = [](std::string json, WeatherForecast &weather) {
        return parseWeather(&json[0], json.size(), weather);
    };
    WeatherForecast weather;
    assert(!parse("{", weather));
    assert(!parse("{}", weather));
    assert(!parse(R"({"timezone_offset":0,"daily":[]})", weather));
    assert(!parse(R"({"timezone_offset":999999,"daily":[{}]})", weather));
    assert(parse(R"({"timezone_offset":3600,"daily":[{}]})", weather));
    assert(std::string(weather.days[0].condition) == "-");
    assert(std::string(weather.days[5].minimumTemperature) == "--");
    assert(parse(R"({"timezone_offset":0,"daily":[{"temp":{"min":-100,"max":100,"day":-0.1},"pop":1,"moon_phase":-1,"rain":9999,"weather":[{"main":"This condition name is much longer than the 32 byte destination","icon":"13d"}]}]})", weather));
    assert(std::string(weather.days[0].minimumTemperature) == "-100");
    assert(std::string(weather.days[0].maximumTemperature) == "100");
    assert(std::string(weather.currentTemperature) == "0");
    assert(std::string(weather.days[0].precipitationProbability) == "100");
    assert(strlen(weather.days[0].condition) == 31);
    assert(weather.days[0].moonPhase == 0);
    assert(parse(R"({"timezone_offset":0,"daily":[{"temp":{"min":1e38,"max":"wrong"},"pop":9,"moon_phase":1e38,"rain":1e38}]})", weather));
    assert(std::string(weather.days[0].minimumTemperature) == "--");
    assert(std::string(weather.days[0].maximumTemperature) == "--");
    assert(std::string(weather.days[0].precipitationProbability) == "--");
    assert(!weather.days[0].hasPrecipitation && weather.days[0].moonPhase == 0);
    assert(std::string(weather.days[0].precipitationMillimeters) == "--");
    std::cout << "HTTP bounds, fragmentation, truncation, idle/total timeout, wraparound; weather missing/oversized/malformed fields passed.\n";
}
