#pragma once

#include <ApplicationController.h>
#include <CommonFunctions.h>
#include <Network.h>
#include <SDPhoto.h>
#include <EventClass.h>

#include "Inkplate.h"
#include "fonts.h"
#include "icons_color.h"

#include <cmath>

class FramEink6ColorRenderer : public frameink::IApplicationRenderer
{
  public:
    FramEink6ColorRenderer()
    {
    }

    void begin()
    {
        if (rotation_ == 0 || rotation_ == 2)
        {
            width_ = display_.width();
            height_ = display_.height();
        }
        else
        {
            width_ = display_.height();
            height_ = display_.width();
        }

        cellWidth_ = (width_ - marginLeft_ - marginRight_) / columns_;
        cellHeight_ = (height_ - heightPhoto_ - headerCalendarName_ - marginUp_ - marginDown_) / rows_;
        headerWeather_ = 40 + color_01d_clear_h;

        display_.begin();
        display_.setRotation(rotation_);
        display_.setTextWrap(false);
        display_.setTextColor(0, 7);

        if (sdPhoto_ == nullptr)
            sdPhoto_ = new SDPhotoClass(&display_);
    }

    void present()
    {
        display_.display();
    }

    void renderSettingsPortal(const frameink::IDeviceConfigurationProvider &settings) override
    {
        display_.clearDisplay();
        display_.setFont(&FreeSans16pt7b);
        display_.setTextWrap(false);

        display_.setTextColor(INKPLATE_BLACK);
        display_.setCursor(15, 60);
        display_.print("1. Connect to ");
        display_.setTextColor(INKPLATE_RED);
        display_.print(settings.accessPointSsid());
        display_.setTextColor(INKPLATE_BLACK);
        display_.println(" WiFi");

        display_.setCursor(15, 120);
        display_.println("2. Open your web browser and");
        display_.setCursor(display_.getCursorX() + 60, display_.getCursorY());
        display_.print("go to: ");

        display_.setTextColor(INKPLATE_GREEN);
        display_.print("http://");
        display_.print(settings.accessPointIp());
        display_.println('/');
        display_.println();

        display_.setTextColor(INKPLATE_BLACK);
        display_.setCursor(15, 220);
        display_.println("3. Fill in the information and");
        display_.setCursor(display_.getCursorX() + 60, display_.getCursorY());
        display_.print("press ");
        display_.setTextColor(INKPLATE_ORANGE);
        display_.print("Send to FramEInk! ");

        display_.setTextColor(INKPLATE_RED);
        display_.setCursor(35, 300);
        drawCentreString("This page will remain", width_ / 2, 320);
        display_.println("");
        drawCentreString(
            (String("active for ") + String(settings.accessPointDurationMinutes()) + String(" minutes")).c_str(),
            width_ / 2,
            display_.getCursorY());

        const uint8_t thick = 5;
        for (size_t i = 0; i < thick; i++)
            display_.drawRoundRect(55 + i, 280 + i, width_ - 1 - 110 - 2 * i, display_.getCursorY() - 280 + 30 - 2 * i, 10, INKPLATE_ORANGE);

        for (size_t i = 0; i < thick; i++)
            display_.drawRect(i, i, width_ - 1 - 2 * i, height_ - 1 - 2 * i, INKPLATE_GREEN);

        display_.display();
    }

    void renderSettingsSaved() override
    {
    }

    void renderPhoto(frameink::ApplicationRuntime &runtime) override
    {
        runtime.counterLandscape = static_cast<unsigned int>(-1);
        display_.clearDisplay();
        if (sdPhoto_ != nullptr)
            sdPhoto_->drawImageFromSD(0, 0, SDPhotoClass::PhotoOrientation::landscape, runtime.counterLandscape);
    }

    void renderDashboard(const frameink::DashboardModel &model, frameink::ApplicationRuntime &runtime) override
    {
        (void)runtime;
        isSunriseShown_ = false;
        currentTimeZoneSeconds_ = model.weather.timeZoneSeconds;

        display_.clearDisplay();
        drawGrid();

        if (model.hasCalendar)
            drawCalendarData(model.calendar);

        drawWeatherStrip(model.weather);
    }

  private:
    static constexpr int rotation_ = 0;
    static constexpr int rows_ = 1;
    static constexpr int columns_ = 3;
    static constexpr int headerDay_ = 36;
    static constexpr int marginLeft_ = 1;
    static constexpr int marginRight_ = 2;
    static constexpr int marginUp_ = 1;
    static constexpr int marginDown_ = 2;
    static constexpr uint16_t colorGrid_ = INKPLATE_GREEN;
    static constexpr float thickLineGrid_ = 2.0f;
    static constexpr uint16_t colorCalendarTitle_ = INKPLATE_BLUE;
    static constexpr uint16_t colorCalendarTime_ = INKPLATE_RED;
    static constexpr uint16_t colorCalendarLocation_ = INKPLATE_GREEN;
    static constexpr uint16_t colorSunsetSunrise_ = INKPLATE_ORANGE;
    static constexpr uint16_t colorDayTitle_ = INKPLATE_BLACK;
    static constexpr uint16_t colorWeatherTempMax_ = INKPLATE_RED;
    static constexpr uint16_t colorWeatherTempMin_ = INKPLATE_BLUE;
    static constexpr uint16_t colorWeatherName_ = INKPLATE_BLACK;

    Inkplate display_;
    SDPhotoClass *sdPhoto_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int heightPhoto_ = 0;
    int cellWidth_ = 0;
    int cellHeight_ = 0;
    int headerCalendarName_ = 0;
    int headerWeather_ = 0;
    bool isSunriseShown_ = false;
    int currentTimeZoneSeconds_ = 0;

    const char *abbrs_[9] = { "01d", "02d", "03d", "04d", "09d", "10d", "11d", "13d", "50d" };
    const uint8_t *const colorIcons_[9] = {
        color_01d_clear, color_02d_few_clouds, color_03d_scattered_clouds,
        color_04d_broken_clouds, color_09d_shower_rain, color_10d_rain,
        color_11d_thunderstorm, color_13d_snow, color_50d_mist };
    const int colorIconsWidth_[9] = {
        color_01d_clear_w, color_02d_few_clouds_w, color_03d_scattered_clouds_w,
        color_04d_broken_clouds_w, color_09d_shower_rain_w, color_10d_rain_w,
        color_11d_thunderstorm_w, color_13d_snow_w, color_50d_mist_w };
    const int colorIconsHeight_[9] = {
        color_01d_clear_h, color_02d_few_clouds_h, color_03d_scattered_clouds_h,
        color_04d_broken_clouds_h, color_09d_shower_rain_h, color_10d_rain_h,
        color_11d_thunderstorm_h, color_13d_snow_h, color_50d_mist_h };
    const uint8_t *const moonPhases_[27] = {
        moon_phase_0, moon_phase_0_04, moon_phase_0_08, moon_phase_0_11, moon_phase_0_15,
        moon_phase_0_18, moon_phase_0_21, moon_phase_0_25, moon_phase_0_29, moon_phase_0_33,
        moon_phase_0_36, moon_phase_0_40, moon_phase_0_43, moon_phase_0_47, moon_phase_0_50,
        moon_phase_0_54, moon_phase_0_58, moon_phase_0_61, moon_phase_0_65, moon_phase_0_69,
        moon_phase_0_73, moon_phase_0_77, moon_phase_0_81, moon_phase_0_85, moon_phase_0_89,
        moon_phase_0_93, moon_phase_0_96 };

    void drawGrid()
    {
        const int x1 = marginLeft_;
        const int y1 = headerCalendarName_ + marginUp_;
        const int x2 = width_ - marginRight_;
        const int y2 = height_ - heightPhoto_ - marginDown_;

        for (size_t row = 0; row < headerDay_; row += yellow_white_h)
        {
            for (size_t col = 0; col < static_cast<size_t>(x2); col += yellow_white_w)
                display_.drawBitmap3Bit(x1 + col, y1 + row, yellow_white, yellow_white_w, yellow_white_h);
        }
        display_.fillRect(x1, headerDay_, x2 - x1, 3, INKPLATE_YELLOW);

        for (int i = 0; i < rows_ + 1; ++i)
        {
            display_.drawThickLine(
                x1,
                static_cast<int>(static_cast<float>(y1) + static_cast<float>(i) * static_cast<float>(y2 - y1) / static_cast<float>(rows_)),
                x2,
                static_cast<int>(static_cast<float>(y1) + static_cast<float>(i) * static_cast<float>(y2 - y1) / static_cast<float>(rows_)),
                colorGrid_,
                thickLineGrid_);
        }
        for (int i = 0; i < columns_ + 1; ++i)
        {
            display_.drawThickLine(
                static_cast<int>(static_cast<float>(x1) + static_cast<float>(i) * static_cast<float>(x2 - x1) / static_cast<float>(columns_)),
                y1,
                static_cast<int>(static_cast<float>(x1) + static_cast<float>(i) * static_cast<float>(x2 - x1) / static_cast<float>(columns_)),
                y2,
                colorGrid_,
                thickLineGrid_);
        }

        display_.setFont(&FreeSansBold12pt7b);
        display_.setTextColor(colorDayTitle_);
        char temp[32];
        for (int i = 0; i < columns_ * rows_; ++i)
        {
            time_t dayEpoch = time(nullptr) + currentTimeZoneSeconds_ + i * 3600L * 24;
            struct tm timeinfo;
            gmtime_r(&dayEpoch, &timeinfo);
            strncpy(temp, asctime(&timeinfo), sizeof(temp) - 1);
            temp[sizeof(temp) - 1] = '\0';
            temp[10] = 0;

            display_.setCursor(
                40 + static_cast<int>(static_cast<float>(x1) + static_cast<float>(i % columns_) * static_cast<float>(x2 - x1) / static_cast<float>(columns_)),
                y1 + headerDay_ - 9 + static_cast<int>(i / columns_) * static_cast<float>(y2 - y1) / static_cast<float>(rows_));
            display_.println(temp);
        }
    }

    bool drawEvent(const EventClass &event, int day, int beginY, int maxHeight, int &heightNeeded)
    {
        const int padLeftRight = 6;
        const int xLeftBox = marginLeft_ + cellWidth_ * (day % columns_) + padLeftRight;
        const int xRightBox = xLeftBox + cellWidth_ - 2 * padLeftRight;
        const int yUpBox = beginY + 3;
        const int marginTextEvent = 5;
        const int xStartTitle = xLeftBox + marginTextEvent;
        const int interlinea = -5;

        display_.setFont(&FreeSansBold12pt7b);
        display_.setTextColor(colorCalendarTitle_);

        int n = 0;
        char line[128];
        int lastSpace = -100;
        display_.setCursor(xStartTitle, beginY + 33);
        for (int i = 0; i < min(EventClass::MAX_N_CHAR_TITLE_CALENDAR, static_cast<int>(strlen(event.name))); ++i)
        {
            if (n == 0 && event.name[i] == ' ')
                i++;

            line[n] = event.name[i];
            if (line[n] == ' ')
                lastSpace = n;
            line[++n] = 0;

            int16_t xt1, yt1;
            uint16_t w, h;
            display_.getTextBounds(line, 0, 0, &xt1, &yt1, &w, &h);

            if (w > xRightBox - xLeftBox - 2 * marginTextEvent)
            {
                i--;
                line[--n] = 0;

                if (n - lastSpace < 5)
                {
                    i -= n - lastSpace - 1;
                    line[lastSpace] = 0;
                }

                display_.setCursor(xStartTitle, display_.getCursorY() + interlinea);
                display_.println(line);
                line[0] = 0;
                n = 0;
            }
        }

        if (line[0] != 0)
        {
            display_.setCursor(xStartTitle, display_.getCursorY() + interlinea);
            display_.println(line);
        }

        display_.setCursor(xStartTitle, display_.getCursorY() - 7);
        display_.setFont(&FreeSansBold9pt7b);

        if (strlen(event.location) > 1)
        {
            display_.setTextColor(colorCalendarTime_);
            display_.println(event.time);

            display_.setCursor(xStartTitle, display_.getCursorY() - 3);

            char locationLine[128] = { 0 };
            display_.setTextColor(colorCalendarLocation_);
            for (int i = 0; i < static_cast<int>(strlen(event.location)); ++i)
            {
                locationLine[i] = event.location[i];
                locationLine[i + 1] = 0;

                int16_t xt1, yt1;
                uint16_t w, h;
                display_.getTextBounds(locationLine, 0, 0, &xt1, &yt1, &w, &h);
                if (w > xRightBox - xLeftBox - 2 * marginTextEvent)
                {
                    for (int j = i - 1; j > max(-1, i - 4); --j)
                        locationLine[j] = '.';
                    locationLine[i] = 0;
                }
            }

            display_.print(locationLine);
        }
        else
        {
            display_.setTextColor(colorCalendarTime_);
            display_.print(event.time);
        }

        const int yDownBox = display_.getCursorY() + 7;
        display_.drawThickLine(xLeftBox, yUpBox, xLeftBox, yDownBox, 3, thickLineGrid_);
        display_.drawThickLine(xLeftBox, yDownBox, xRightBox, yDownBox, 3, thickLineGrid_);
        display_.drawThickLine(xRightBox, yDownBox, xRightBox, yUpBox, 3, thickLineGrid_);
        display_.drawThickLine(xRightBox, yUpBox, xLeftBox, yUpBox, 3, thickLineGrid_);

        heightNeeded = display_.getCursorY() + 12 - yUpBox;
        return display_.getCursorY() < maxHeight - 100;
    }

    void drawCalendarData(const frameink::CalendarAgenda &calendar)
    {
        int columns[columns_ * rows_] = { 0 };
        bool clogged[columns_ * rows_] = { 0 };
        int cloggedCount[columns_ * rows_] = { 0 };

        for (int i = 0; i < calendar.eventCount; ++i)
        {
            const EventClass &event = calendar.events[i];
            if (event.day != -1 && clogged[event.day])
                ++cloggedCount[event.day];
            if (event.day == -1 || clogged[event.day])
                continue;

            int shift = 0;
            const int padDown = 5;
            const int padUp = 10;
            const bool visible = drawEvent(
                event,
                event.day,
                columns[event.day] + (event.day / columns_) * cellHeight_ + marginUp_ + headerCalendarName_ + headerDay_ + headerWeather_ + padUp,
                (event.day / columns_) * cellHeight_ + marginUp_ + headerCalendarName_ + cellHeight_,
                shift);

            columns[event.day] += shift + padDown;
            if (!visible)
                clogged[event.day] = 1;
        }

        for (int i = 0; i < columns_ * rows_; ++i)
        {
            if (clogged[i] && cloggedCount[i] > 0)
            {
                const int marginMoreEvent = 6;
                const int xBegin = marginLeft_ + cellWidth_ * (i % columns_) + marginMoreEvent;
                const int yBegin = marginUp_ + headerCalendarName_ + (i / columns_) * cellHeight_ + cellHeight_ - 23;
                display_.fillRoundRect(xBegin, yBegin, cellWidth_ - 2 * marginMoreEvent, 20, 10, INKPLATE_YELLOW);
                display_.drawRoundRect(xBegin, yBegin, cellWidth_ - 2 * marginMoreEvent, 20, 10, INKPLATE_ORANGE);
                display_.setCursor(15 + xBegin, yBegin + 15);
                display_.setTextColor(INKPLATE_BLACK, INKPLATE_WHITE);
                display_.setFont(&FreeSansBold9pt7b);
                display_.print(cloggedCount[i]);
                display_.print(" more event");
                if (cloggedCount[i] > 1)
                    display_.print("s");
            }
        }
    }

    void drawWeatherIcon(int beginX, int beginY, const frameink::WeatherDayForecast &day)
    {
        for (int i = 0; i < 9; ++i)
        {
            if (strcmp(day.iconCode, abbrs_[i]) == 0)
                display_.drawBitmap3Bit(beginX, beginY, colorIcons_[i], colorIconsWidth_[i], colorIconsHeight_[i]);
        }
    }

    void drawWeatherLabel(int x, int y, const frameink::WeatherDayForecast &day)
    {
        display_.setTextColor(colorWeatherName_, INKPLATE_WHITE);
        display_.setFont(&FreeSans16pt7b);
        display_.setCursor(x, y);
        display_.println(day.condition);
    }

    void drawWeatherTemp(int x, int y, const frameink::WeatherDayForecast &day)
    {
        int xThermometer = x + 41;
        int xTempMax = x;
        int xTempMin = x;
        int16_t x1, y1;
        uint16_t w, h;

        display_.getTextBounds(day.maximumTemperature, 0, 0, &x1, &y1, &w, &h);
        if (xTempMax + static_cast<int>(w) > xThermometer)
            xTempMax = xThermometer - w;

        display_.getTextBounds(day.minimumTemperature, 0, 0, &x1, &y1, &w, &h);
        if (xTempMin + static_cast<int>(w) > xThermometer)
            xTempMin = xThermometer - w;

        display_.setFont(&FreeSansSerifBold14pt7b);
        display_.setTextColor(colorWeatherTempMax_);
        display_.setCursor(xTempMax, y);
        display_.print(day.maximumTemperature);

        display_.setFont(&FreeSansSerifBold14pt7b);
        display_.setTextColor(colorWeatherTempMin_);
        display_.setCursor(xTempMin, y + 30);
        display_.print(day.minimumTemperature);

        display_.drawBitmap3Bit(xThermometer, y - 28, termometro, termometro_w, termometro_h);
    }

    void drawSunriseSunsetTime(int x, int y, const frameink::WeatherDayForecast &day)
    {
        const int separator = 5;
        display_.setTextColor(colorSunsetSunrise_);
        display_.setFont(&FreeSansBold9pt7b);
        display_.setCursor(x + icon_sunset_w + separator, y);
        display_.print(day.sunrise);

        int16_t x1, y1;
        uint16_t w, h;
        display_.getTextBounds(day.sunrise, 0, 0, &x1, &y1, &w, &h);

        display_.setCursor(x + icon_sunset_w + separator, y + h + separator);
        display_.print(day.sunset);

        const int offsetYIcon = 0.5f * (2 * (h + separator) - icon_sunset_h);
        display_.drawBitmap3Bit(x + 2, y - h * 1.2f + offsetYIcon, icon_sunset, icon_sunset_w, icon_sunset_h);
    }

    void drawProbabilityOfRain(int x, int y, const frameink::WeatherDayForecast &day)
    {
        const int separator = 7;
        display_.setTextColor(colorWeatherTempMin_);
        display_.setFont(&FreeSansBold9pt7b);
        display_.setCursor(x + icon_drop_w, y);
        display_.print(day.precipitationProbability);
        display_.setFont(&FreeSansBold7pt7b);
        display_.setCursor(display_.getCursorX() + 3, display_.getCursorY() - 1);
        display_.print("%");

        int16_t x1, y1;
        uint16_t w, h;
        display_.getTextBounds(day.precipitationProbability, 0, 0, &x1, &y1, &w, &h);

        display_.setFont(&FreeSansBold9pt7b);
        display_.setCursor(x + icon_drop_w, y + h + separator);
        display_.print(day.precipitationMillimeters);
        display_.setFont(&FreeSansBold7pt7b);
        display_.setCursor(display_.getCursorX() + 3, display_.getCursorY() - 1);
        display_.print("mm");

        const int offsetYIcon = 0.5f * (2 * (h + separator) - icon_drop_h);
        display_.drawBitmap3Bit(x, y - h * 1.3f + offsetYIcon, icon_drop, icon_drop_w, icon_drop_h);
    }

    void drawWeatherStrip(const frameink::WeatherForecast &weather)
    {
        const int padLeft = 15;
        const int padUp = 2;
        const int padDown = 5;

        for (int dayIndex = 0; dayIndex < columns_ * rows_; dayIndex++)
        {
            const frameink::WeatherDayForecast &day = weather.days[dayIndex];
            const int xBegin = marginLeft_ + cellWidth_ * (dayIndex % columns_) + padLeft;
            const int yBegin = marginUp_ + headerCalendarName_ + headerDay_ + (dayIndex / columns_) * cellHeight_ + padUp;

            drawWeatherIcon(xBegin, yBegin + 3, day);
            drawWeatherLabel(xBegin - 8, yBegin + headerWeather_ - 5, day);
            drawWeatherTemp(xBegin + color_01d_clear_h + 20, yBegin + 36, day);

            if (day.hasPrecipitation)
            {
                drawProbabilityOfRain(xBegin + color_01d_clear_h + 6, yBegin + headerWeather_ - 22, day);
            }
            else if (!isSunriseShown_)
            {
                isSunriseShown_ = true;
                drawSunriseSunsetTime(xBegin + cellWidth_ - icon_sunset_w - 71, yBegin + headerWeather_ - icon_sunset_h / 2.0f - 3, day);
            }
            else
            {
                unsigned int indexMoon = static_cast<unsigned int>(round(day.moonPhase * (sizeof(moonPhases_) / sizeof(moonPhases_[0]))));
                if (indexMoon >= sizeof(moonPhases_) / sizeof(moonPhases_[0]))
                    indexMoon = sizeof(moonPhases_) / sizeof(moonPhases_[0]) - 1;

                display_.drawBitmap(
                    xBegin + cellWidth_ - moon_phase_0_w - padLeft - 6,
                    yBegin + headerWeather_ - moon_phase_0_w,
                    moonPhases_[indexMoon],
                    moon_phase_0_w,
                    moon_phase_0_w,
                    INKPLATE_WHITE,
                    INKPLATE_BLACK);
            }

            display_.drawThickLine(
                xBegin - padLeft + thickLineGrid_,
                yBegin + headerWeather_ + padDown,
                xBegin - padLeft + cellWidth_,
                yBegin + headerWeather_ + padDown,
                colorGrid_,
                thickLineGrid_);
        }
    }

    void drawCentreString(const char *text, int x, int y)
    {
        int16_t x1, y1;
        uint16_t w, h;
        display_.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        display_.setCursor(x - (w / 2), y + (h / 2));
        display_.print(text);
    }
};
