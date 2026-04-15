#pragma once

#include <array>

namespace frameink {

struct WeatherDayForecast
{
    char condition[32] = "-";
    char iconCode[16] = "";
    char minimumTemperature[4] = "--";
    char maximumTemperature[4] = "--";
    char precipitationProbability[5] = "--";
    char precipitationMillimeters[5] = "--";
    char sunrise[6] = "--:--";
    char sunset[6] = "--:--";
    float moonPhase = 0.0f;
    bool hasPrecipitation = false;
};

struct WeatherForecast
{
    static constexpr size_t DayCount = 6;

    int timeZoneSeconds = 0;
    char currentTemperature[16] = "--";
    char currentWind[16] = "0";
    char currentTime[16] = "00:00";
    std::array<WeatherDayForecast, DayCount> days;
};

} // namespace frameink
