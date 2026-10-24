#pragma once

#include <ApplicationController.h>
#include <CommonFunctions.h>
#include <SDPhoto.h>
#include <InkplateImageCompat.h>
#include <BatteryIndicator.h>
#include <Calendar/CalendarTextLayout.h>

#include "Inkplate.h"
#include "fonts.h"
#include "icons_1bit.h"

#include <cmath>
#include <new>

class FramEink10Renderer : public frameink::IApplicationRenderer
{
  public:
    FramEink10Renderer() : display_(INKPLATE_3BIT)
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

        if (rows_ > 1)
            heightPhoto_ = 0;

        cellWidth_ = (width_ - marginLeft_ - marginRight_) / columns_;
        cellHeight_ = (height_ - heightPhoto_ - headerCalendarName_ - marginUp_ - marginDown_) / rows_;
        headerWeather_ = 40 + iconWeatherHeight_;

        display_.begin();
        display_.setRotation(rotation_);
        display_.setTextWrap(false);
        display_.cp437(true);
        display_.setTextColor(0, 7);

        if (sdPhoto_ == nullptr)
            sdPhoto_ = new (std::nothrow) SDPhotoClass(&display_);
    }

    void present()
    {
        display_.display();
    }

    RTC &clockRtc() { return display_.rtc; }

    frameink::PhotoUploadTarget photoUploadTarget()
    {
        frameink::PhotoUploadTarget target;
        target.name = "FramEInk 10";
        target.width = display_.width();
        target.height = display_.height();
        target.directory = "/portrait";
        target.grayscale = true;
        target.storage = sdPhoto_ != nullptr && sdPhoto_->initOk ? &display_.getSdFat() : nullptr;
        return target;
    }

    void renderSettingsPortal(const frameink::IDeviceConfigurationProvider &settings) override
    {
        display_.clearDisplay();
        display_.setFont(&FreeSans24pt7b);
        display_.setTextWrap(false);

        display_.setTextColor(0);
        display_.setCursor(15, 80);
        const int x1 = 55;
        const int y1 = 55;
        const int x2 = x1 + width_ - 110;
        const int y2 = y1 + 92;
        for (size_t row = y1 + 4; row < static_cast<size_t>(y2 - 4); row += light_gray_h)
        {
            for (size_t col = x1 + 4; col < static_cast<size_t>(x2 - 4); col += light_gray_w)
                frameink::drawBitmap3Bit(display_, col, row, light_gray, light_gray_w, light_gray_h);
        }
        drawCentreString("SETTINGS", width_ / 2, 100);
        const uint8_t thick = 5;
        for (size_t i = 0; i < thick; i++)
            display_.drawRoundRect(x1 + i, y1 + i, width_ - 110 - 2 * i, 92 - 2 * i, 10, 1);

        display_.setFont(&FreeSans18pt7b);
        display_.setTextColor(2);
        display_.setCursor(x1, 260);
        display_.print("1. ");
        const int connectX = display_.getCursorX();
        display_.print("Connect to ");
        display_.setTextColor(0);
        display_.print(settings.accessPointSsid());
        display_.setTextColor(2);
        display_.println(" WiFi");

        display_.setCursor(connectX, 310);
        display_.print("Password: ");
        display_.setTextColor(0);
        display_.print(settings.accessPointPassword());

        display_.setCursor(x1, 370);
        display_.println("2. Settings open automatically");
        display_.setCursor(display_.getCursorX() + 95, display_.getCursorY());
        display_.print("Or open: ");

        display_.setTextColor(0);
        display_.print("http://");
        display_.print(settings.accessPointIp());
        display_.println('/');
        display_.println();

        display_.setTextColor(2);
        display_.setCursor(x1, 520);
        display_.println("3. Settings and photo upload");
        display_.setCursor(display_.getCursorX() + 95, display_.getCursorY());
        display_.print("Tap ");
        display_.setTextColor(0);
        display_.print("Finish when done");

        display_.setTextColor(1);
        drawCentreString("Closes after inactivity:", width_ / 2, 800);
        display_.println("");
        drawCentreString(
            (String(settings.accessPointDurationMinutes()) + String(" minutes")).c_str(),
            width_ / 2,
            display_.getCursorY());

        for (size_t i = 0; i < thick; i++)
            display_.drawRoundRect(x1 + i, display_.getCursorY() - 120 + i, width_ - 1 - 110 - 2 * i, 200 - 2 * i, 10, 1);

        for (size_t i = 0; i < thick; i++)
            display_.drawRect(i, i, width_ - 1 - 2 * i, height_ - 1 - 2 * i, 1);

        frameink::drawBatteryIcon(display_, width_ - 56, 20, display_.readBattery(), 0);
        display_.display();
    }

    void renderSettingsSaved() override
    {
        display_.clearDisplay();
        display_.setFont(&FreeSans24pt7b);
        display_.setTextWrap(false);
        display_.setTextColor(0);
        drawCentreString("SETTINGS SAVED", width_ / 2, 440);
        drawCentreString("WAIT LOADING...", width_ / 2, 510);
        const uint8_t thick = 5;
        for (size_t i = 0; i < thick; i++)
            display_.drawRoundRect(55 + i, display_.getCursorY() - 140, width_ - 1 - 110 - 2 * i, 200 - 2 * i, 10, 1);
        display_.display();
    }

    void renderPhoto(frameink::ApplicationRuntime &runtime, frameink::PhotoOrder order) override
    {
        display_.clearDisplay();
        if (sdPhoto_ != nullptr)
            sdPhoto_->drawImageFromSD(0, 0, SDPhotoClass::PhotoOrientation::portrait, runtime.portraitPhotos, order);
    }

    void renderDashboard(const frameink::DashboardModel &model, frameink::ApplicationRuntime &runtime) override
    {
        currentTimeZoneSeconds_ = model.weather.timeZoneSeconds;
        display_.clearDisplay();

        drawBattery();
        if (model.hasCalendar)
            drawCalendarName(model.calendar);
        drawCalendarSize(model.calendar);
        drawTime();

        // Paint complete, opaque rows from top to bottom. A long event can
        // extend below its cell; the next row must cover it before drawing its
        // own day headers, weather and events, regardless of event array order.
        for (int row = 0; row < rows_; ++row)
        {
            drawGrid(row);
            if (model.hasCalendar)
                drawCalendarData(model.calendar, row);
            drawWeatherStrip(model.weather, row);
        }

        if (rows_ < 2)
        {
            if (sdPhoto_ != nullptr)
                sdPhoto_->drawImageFromSD(0, heightPhoto_ + 3, SDPhotoClass::PhotoOrientation::landscape,
                    runtime.landscapePhotos, model.deviceConfiguration.photoOrder);
        }
    }

  private:
    static constexpr int rotation_ = 1;
    static constexpr int rows_ = 2;
    static constexpr int columns_ = 3;
    static constexpr int iconWeatherHeight_ = 96;
    static constexpr int headerCalendarName_ = 30;
    static constexpr int headerDay_ = 40;
    static constexpr int marginLeft_ = -3;
    static constexpr int marginRight_ = 0;
    static constexpr int marginUp_ = 0;
    static constexpr int marginDown_ = -2;
    static constexpr int colorGrid_ = 2;
    static constexpr float thickLineGrid_ = 2.0f;
    static constexpr uint16_t colorCalendarTitle_ = 0;
    static constexpr uint16_t colorCalendarTime_ = 0;
    static constexpr uint16_t colorCalendarLocation_ = 0;

    Inkplate display_;
    SDPhotoClass *sdPhoto_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int heightPhoto_ = 600;
    int cellWidth_ = 0;
    int cellHeight_ = 0;
    int headerWeather_ = 0;
    int currentTimeZoneSeconds_ = 0;

    const char *abbrs_[11] = { "13d", "sl", "h", "11d", "10d", "lr", "09d", "04d", "02d", "01d", "03d" };
    const uint8_t *const colorIcons_[11] = {
        icon_s_sn, icon_s_sl, icon_s_h, icon_s_t, icon_s_hr,
        icon_s_lr, icon_s_s, icon_s_hc, icon_s_lc, icon_s_c, icon_s_hc };
    const uint8_t *const moonPhases_[27] = {
        moon_phase_0, moon_phase_0_04, moon_phase_0_08, moon_phase_0_11, moon_phase_0_15,
        moon_phase_0_18, moon_phase_0_21, moon_phase_0_25, moon_phase_0_29, moon_phase_0_33,
        moon_phase_0_36, moon_phase_0_40, moon_phase_0_43, moon_phase_0_47, moon_phase_0_50,
        moon_phase_0_54, moon_phase_0_58, moon_phase_0_61, moon_phase_0_65, moon_phase_0_69,
        moon_phase_0_73, moon_phase_0_77, moon_phase_0_81, moon_phase_0_85, moon_phase_0_89,
        moon_phase_0_93, moon_phase_0_96 };

    void drawCalendarName(const frameink::CalendarAgenda &calendar)
    {
        if (calendar.name[0] == '\0')
            return;

        display_.setTextColor(0, 7);
        display_.setFont(&FreeSans12pt7b);
        display_.setTextSize(1);
        display_.setCursor(marginLeft_ + 20, marginUp_ + 22);
        display_.println(calendar.name);
    }

    void drawTime()
    {
        display_.setTextColor(0, 7);
        display_.setFont(&FreeSans12pt7b);
        display_.setTextSize(1);
        display_.setCursor(marginLeft_ + 500, marginUp_ + 22);

        const time_t nowSecs = time(nullptr) + currentTimeZoneSeconds_;
        struct tm timeinfo;
        gmtime_r(&nowSecs, &timeinfo);
        char dateTime[64];
        strncpy(dateTime, asctime(&timeinfo), sizeof(dateTime) - 1);
        dateTime[sizeof(dateTime) - 1] = '\0';
        const int t = dateTime[16];
        dateTime[16] = 0;
        display_.println(dateTime);
        dateTime[16] = t;
    }

    void drawBattery()
    {
        const float batteryVoltage = display_.readBattery();
        const int batteryBodyWidth = 34;
        const int batteryTerminalWidth = 4;
        const int batteryTextX = width_ - marginRight_ - 50;
        const int batteryX = batteryTextX - batteryBodyWidth - batteryTerminalWidth - 10;
        const int batteryY = marginUp_ + 4;
        frameink::drawBatteryIcon(display_, batteryX, batteryY, batteryVoltage, 0);

        display_.setTextColor(0, 7);
        display_.setFont(&FreeSans12pt7b);
        display_.setTextSize(1);
        display_.setCursor(batteryTextX, marginUp_ + 22);
        display_.println(batteryVoltage, 2);
    }

    void drawCalendarSize(const frameink::CalendarAgenda &calendar)
    {
        display_.setTextColor(0, 7);
        display_.setFont(&FreeSans12pt7b);
        display_.setTextSize(1);
        display_.setCursor(marginLeft_ + 200, marginUp_ + 22);
        display_.printf("CalendarSize: %ld", calendar.downloadSize);
    }

    void drawGrid(int row)
    {
        const int x1 = marginLeft_;
        const int y1 = headerCalendarName_ + marginUp_ + row * cellHeight_;
        const int x2 = width_ - marginRight_;
        const int y2 = y1 + cellHeight_;

        // The patterned header and glyphs have transparent gaps, so repaint
        // the entire row, including empty days and the weather strip, first.
        display_.fillRect(0, y1, width_, cellHeight_, 7);
        for (int headerY = 0; headerY < headerDay_; headerY += light_gray_h)
        {
            for (size_t col = 0; col < static_cast<size_t>(x2); col += light_gray_w)
                frameink::drawBitmap3Bit(display_, x1 + col, y1 + headerY, light_gray, light_gray_w, light_gray_h);
        }
        display_.fillRect(x1, y1 + headerDay_ - thickLineGrid_, x2 - x1, thickLineGrid_, 6);

        for (int edgeY : {y1, y2})
        {
            display_.drawThickLine(x1, edgeY, x2, edgeY, colorGrid_, thickLineGrid_);
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

        display_.setTextColor(0, 7);
        display_.setFont(&FreeSansBold12pt7b);
        for (int i = row * columns_; i < (row + 1) * columns_; ++i)
        {
            time_t dayEpoch = time(nullptr) + currentTimeZoneSeconds_ + i * 3600L * 24;
            struct tm timeinfo;
            gmtime_r(&dayEpoch, &timeinfo);
            char temp[64];
            strncpy(temp, asctime(&timeinfo), sizeof(temp) - 1);
            temp[sizeof(temp) - 1] = '\0';
            temp[10] = 0;

            display_.setCursor(
                70 + static_cast<int>(static_cast<float>(x1) + static_cast<float>(i % columns_) * static_cast<float>(x2 - x1) / static_cast<float>(columns_)),
                y1 + headerDay_ - 13);
            display_.println(temp);
        }
    }

    bool drawEvent(const EventClass &event, int day, int beginY, int maxHeight, int &heightNeeded)
    {
        const int padLeftRight = 6;
        const int xLeftBox = marginLeft_ + cellWidth_ * (day % columns_) + padLeftRight;
        const int xRightBox = xLeftBox + cellWidth_ - 2 * padLeftRight;
        const int yUpBox = beginY + 3;
        const int marginTextEvent = 8;
        const int xStartTitle = xLeftBox + marginTextEvent;
        const int textWidth = xRightBox - xLeftBox - 2 * marginTextEvent;

        display_.setFont(&CalendarSansBold16pt);
        display_.setTextColor(colorCalendarTitle_);
        display_.setCursor(xStartTitle, beginY + 50);
        // Extra accents need enough vertical space between wrapped lines.
        frameink::printWrappedCalendarText(display_, event.name, xStartTitle, textWidth, -14, true);

        display_.setCursor(xStartTitle, display_.getCursorY() - 15);
        display_.setFont(&CalendarSans12pt);
        display_.setTextColor(colorCalendarTime_);
        if (event.location[0] != '\0')
        {
            display_.println(event.time);
            display_.setCursor(xStartTitle, display_.getCursorY() - 2);
            char locationLine[128] = {};
            frameink::fitCalendarText(display_, event.location, locationLine, sizeof(locationLine), textWidth);
            display_.setTextColor(colorCalendarLocation_);
            display_.print(locationLine);
        }
        else
        {
            display_.print(event.time);
        }

        const int yDownBox = display_.getCursorY() + 10;
        display_.drawThickLine(xLeftBox, yUpBox, xLeftBox, yDownBox, 3, thickLineGrid_);
        display_.drawThickLine(xLeftBox, yDownBox, xRightBox, yDownBox, 3, thickLineGrid_);
        display_.drawThickLine(xRightBox, yDownBox, xRightBox, yUpBox, 3, thickLineGrid_);
        display_.drawThickLine(xRightBox, yUpBox, xLeftBox, yUpBox, 3, thickLineGrid_);

        heightNeeded = display_.getCursorY() + 12 - yUpBox;
        return display_.getCursorY() < maxHeight - 100;
    }

    void drawCalendarData(const frameink::CalendarAgenda &calendar, int row)
    {
        int columns[columns_ * rows_] = { 0 };
        bool clogged[columns_ * rows_] = { 0 };
        int cloggedCount[columns_ * rows_] = { 0 };

        for (int i = 0; i < calendar.eventCount; ++i)
        {
            const EventClass &event = calendar.events[i];
            if (event.day < row * columns_ || event.day >= (row + 1) * columns_)
                continue;
            if (clogged[event.day])
                ++cloggedCount[event.day];
            if (clogged[event.day])
                continue;

            int shift = 0;
            const int padDown = 5;
            const int padUp = 8;
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

        for (int i = row * columns_; i < (row + 1) * columns_; ++i)
        {
            if (clogged[i] && cloggedCount[i] > 0)
            {
                const int xBegin = marginLeft_ + cellWidth_ * (i % columns_) + thickLineGrid_;
                const int yBegin = marginUp_ + headerCalendarName_ + (i / columns_) * cellHeight_ + cellHeight_ - 23;
                display_.fillRoundRect(5 + xBegin, yBegin, cellWidth_ - 10, 20, 10, 6);
                display_.setCursor(15 + xBegin, yBegin + 15);
                display_.setTextColor(7, WHITE);
                display_.setFont(&FreeSans9pt7b);
                display_.print(cloggedCount[i]);
                display_.print(" more event");
                if (cloggedCount[i] > 1)
                    display_.print("s");
            }
        }
    }

    void drawWeatherIcon(int beginX, int beginY, const frameink::WeatherDayForecast &day)
    {
        for (int i = 0; i < 11; ++i)
        {
            if (strcmp(day.iconCode, abbrs_[i]) == 0)
                display_.drawBitmap(beginX, beginY, colorIcons_[i], iconWeatherHeight_, iconWeatherHeight_, 3);
        }
    }

    int drawWeatherLabel(int x, int y, const frameink::WeatherDayForecast &day)
    {
        display_.setTextColor(BLACK, WHITE);
        display_.setFont(&FreeSans18pt7b);
        display_.setTextSize(1);
        display_.setCursor(x, y);
        display_.println(day.condition);

        int16_t x1, y1;
        uint16_t w, h;
        display_.getTextBounds(day.condition, 0, 0, &x1, &y1, &w, &h);
        return x + w;
    }

    void drawWeatherTemp(int x, int y, int thermometerX, const frameink::WeatherDayForecast &day)
    {
        const int thermometerGap = 8;
        int16_t xMaxBounds, yMaxBounds, xMinBounds, yMinBounds;
        uint16_t wMax, hMax, wMin, hMin;

        display_.setFont(&FreeSans18pt7b);
        display_.getTextBounds(day.maximumTemperature, 0, 0, &xMaxBounds, &yMaxBounds, &wMax, &hMax);
        display_.getTextBounds(day.minimumTemperature, 0, 0, &xMinBounds, &yMinBounds, &wMin, &hMin);

        const int xTempMax = min(x, thermometerX - thermometerGap - xMaxBounds - static_cast<int>(wMax));
        const int xTempMin = min(x, thermometerX - thermometerGap - xMinBounds - static_cast<int>(wMin));

        display_.setCursor(xTempMax, y + 2);
        display_.print(day.maximumTemperature);
        display_.setCursor(xTempMin, y + 38);
        display_.print(day.minimumTemperature);

        frameink::drawBitmap3Bit(display_, thermometerX, y - 28, termometro, termometro_w, termometro_h);
    }

    void drawSunriseSunsetTime(int x, int y, const frameink::WeatherDayForecast &day)
    {
        display_.setFont(&FreeSans12pt7b);
        display_.setCursor(x, y + icon_sunset_h + 20);
        display_.print(day.sunrise);
        display_.setFont(&FreeSans12pt7b);
        display_.setCursor(x, y + icon_sunset_h + 40);
        display_.print(day.sunset);

        int16_t x1, y1;
        uint16_t w, h;
        display_.getTextBounds(day.sunset, 0, 0, &x1, &y1, &w, &h);
        const int offsetXIcon = 0.5f * (w - icon_sunset_w);
        display_.drawBitmap(x + offsetXIcon, y, icon_sunset, icon_sunset_w, icon_sunset_h, 7, 0);
    }

    void drawProbabilityOfRain(int x, int y, const frameink::WeatherDayForecast &day)
    {
        const int separator = 9;
        display_.setTextColor(2);
        display_.setFont(&FreeSansBold11pt7b);
        display_.setCursor(x + icon_drop_w, y);
        display_.print(day.precipitationProbability);
        display_.setFont(&FreeSansBold7pt7b);
        display_.setCursor(display_.getCursorX() + 3, display_.getCursorY() - 1);
        display_.print("%");

        int16_t x1, y1;
        uint16_t w, h;
        display_.getTextBounds(day.precipitationProbability, 0, 0, &x1, &y1, &w, &h);

        display_.setFont(&FreeSansBold11pt7b);
        display_.setCursor(x + icon_drop_w, y + h + separator);
        display_.print(day.precipitationMillimeters);
        display_.setFont(&FreeSansBold7pt7b);
        display_.setCursor(display_.getCursorX() + 3, display_.getCursorY() - 1);
        display_.print("mm");

        const int offsetYIcon = 0.5f * (2 * (h + separator) - icon_drop_h);
        frameink::drawBitmap3Bit(display_, x, y - h * 1.3f + offsetYIcon, icon_drop, icon_drop_w, icon_drop_h);
    }

    void drawWeatherStrip(const frameink::WeatherForecast &weather, int row)
    {
        const int padLeft = 15;
        const int padUp = -1;
        const int padDown = 5;

        for (int dayIndex = row * columns_; dayIndex < (row + 1) * columns_; ++dayIndex)
        {
            const frameink::WeatherDayForecast &day = weather.days[dayIndex];
            const int xBegin = marginLeft_ + cellWidth_ * (dayIndex % columns_) + padLeft;
            const int yBegin = marginUp_ + headerCalendarName_ + headerDay_ + (dayIndex / columns_) * cellHeight_ + padUp;
            drawWeatherIcon(xBegin, yBegin + 5, day);
            const int yWeatherLabel = yBegin + headerWeather_ - 4;
            drawWeatherLabel(xBegin, yWeatherLabel, day);
            const int sunsetTimeX = xBegin + iconWeatherHeight_ + 100;
            const int thermometerX = sunsetTimeX - termometro_w - 8;
            drawWeatherTemp(xBegin + iconWeatherHeight_ + 12, yBegin + 40, thermometerX, day);
            drawSunriseSunsetTime(sunsetTimeX, yBegin + 10, day);

            if (day.hasPrecipitation)
                drawProbabilityOfRain(xBegin + iconWeatherHeight_ + 8, yWeatherLabel - 19, day);

            unsigned int indexMoon = static_cast<unsigned int>(round(day.moonPhase * (sizeof(moonPhases_) / sizeof(moonPhases_[0]))));
            if (indexMoon >= sizeof(moonPhases_) / sizeof(moonPhases_[0]))
                indexMoon = sizeof(moonPhases_) / sizeof(moonPhases_[0]) - 1;

            display_.drawBitmap(
                xBegin + cellWidth_ - moon_phase_0_w - padLeft - 15,
                yBegin + headerWeather_ - moon_phase_0_w,
                moonPhases_[indexMoon],
                moon_phase_0_w,
                moon_phase_0_w,
                7,
                0);

            display_.drawThickLine(
                xBegin - padLeft + thickLineGrid_,
                yBegin + headerWeather_ + padDown,
                xBegin - padLeft + cellWidth_,
                yBegin + headerWeather_ + padDown,
                colorGrid_ + 3,
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
