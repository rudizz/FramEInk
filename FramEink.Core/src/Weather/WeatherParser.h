#pragma once

#include "WeatherForecast.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace frameink {

template<size_t N>
inline void weatherNumber(char (&output)[N], JsonVariantConst value, float minimum,
                          float maximum, unsigned decimals = 0, float scale = 1)
{
    const float number = value.is<float>() ? value.as<float>() * scale : NAN;
    if (!std::isfinite(number) || number < minimum || number > maximum)
        snprintf(output, N, "--");
    else
        snprintf(output, N, "%.*f", int(decimals), number > -0.5f && number < 0 && decimals == 0 ? 0.0f : number);
}

inline void weatherTime(char (&output)[6], JsonVariantConst value, int offset)
{
    const int64_t utc = value.as<int64_t>();
    if (utc < 1577836800 || utc + offset > 2147483647 || utc + offset < 0) return;
    const time_t epoch = static_cast<time_t>(utc + offset);
    tm fields{};
    if (gmtime_r(&epoch, &fields)) strftime(output, sizeof(output), "%H:%M", &fields);
}

inline bool parseWeather(char *data, size_t size, WeatherForecast &forecast)
{
    JsonDocument document;
    // Keep only fields that are displayed. A large description must not consume
    // unbounded heap; the HTTP body has an independent 32 KiB limit.
    JsonDocument filter;
    filter["timezone_offset"] = true;
    JsonObject dayFilter = filter["daily"][0].to<JsonObject>();
    for (const char *key : {"temp", "wind_speed", "weather", "pop", "rain", "snow", "moon_phase", "sunrise", "sunset"})
        dayFilter[key] = true;
    const auto error = deserializeJson(document, data, size, DeserializationOption::Filter(filter),
        DeserializationOption::NestingLimit(12));
    if (error || document.overflowed()) return false;
    const JsonArrayConst daily = document["daily"].as<JsonArrayConst>();
    if (daily.isNull() || daily.size() == 0 || !document["timezone_offset"].is<int>()) return false;
    const int offset = document["timezone_offset"].as<int>();
    if (offset < -12 * 3600 || offset > 14 * 3600) return false;
    forecast.timeZoneSeconds = offset;
    weatherNumber(forecast.currentTemperature, daily[0]["temp"]["day"], -100, 100);
    weatherNumber(forecast.currentWind, daily[0]["wind_speed"], 0, 500);
    for (size_t index = 0; index < WeatherForecast::DayCount && index < daily.size(); ++index)
    {
        const JsonObjectConst source = daily[index].as<JsonObjectConst>();
        auto &day = forecast.days[index];
        const char *condition = source["weather"][0]["main"] | "-";
        const char *icon = source["weather"][0]["icon"] | "";
        snprintf(day.condition, sizeof(day.condition), "%s", condition);
        snprintf(day.iconCode, sizeof(day.iconCode), "%s", icon);
        weatherNumber(day.minimumTemperature, source["temp"]["min"], -100, 100);
        weatherNumber(day.maximumTemperature, source["temp"]["max"], -100, 100);
        weatherNumber(day.precipitationProbability, source["pop"], 0, 100, 0, 100);
        const float pop = source["pop"].as<float>();
        const bool isRainOrSnow = strcmp(condition, "Rain") == 0 || strcmp(condition, "Snow") == 0;
        day.hasPrecipitation = isRainOrSnow && std::isfinite(pop) && pop > 0 && pop <= 1;
        const JsonVariantConst precipitation = source[strcmp(icon, "13d") == 0 ? "snow" : "rain"];
        if (precipitation.isNull()) snprintf(day.precipitationMillimeters, sizeof(day.precipitationMillimeters), "0.0");
        else weatherNumber(day.precipitationMillimeters, precipitation, 0, 9999,
            precipitation.as<float>() < 10 ? 1 : 0);
        const float phase = source["moon_phase"].as<float>();
        day.moonPhase = std::isfinite(phase) && phase >= 0 && phase <= 1 ? phase : 0;
        weatherTime(day.sunrise, source["sunrise"], offset);
        weatherTime(day.sunset, source["sunset"], offset);
    }
    return true;
}

} // namespace frameink
