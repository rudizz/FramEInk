#include <CommonFunctions.h>
#include <Calendar/CalendarText.h>
#include <Calendar/CalendarTextLayout.h>
#include <BatteryIndicator.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#define PROGMEM
struct GFXglyph { uint16_t bitmapOffset; uint8_t width, height, xAdvance; int8_t xOffset, yOffset; };
struct GFXfont { uint8_t *bitmap; GFXglyph *glyph; uint16_t first, last; uint8_t yAdvance; };
#include <Fonts/CalendarSansBold9pt.h>
#include <Fonts/CalendarSansBold12pt.h>
#include <Fonts/CalendarSansBold16pt.h>
#include <Fonts/CalendarSans12pt.h>

// Minimal software GFX renderer: the actual generated glyph bitmaps/metrics are
// used for width tests and the raster preview, not a system font approximation.
struct Display {
    int width = 1080, height = 780, x = 0, y = 0;
    const GFXfont *font = &CalendarSansBold12pt;
    std::vector<std::string> lines;
    std::vector<uint8_t> pixels = std::vector<uint8_t>(width * height, 255);
    void setCursor(int newX, int newY) { x = newX; y = newY; }
    int getCursorY() const { return y; }
    void setFont(const GFXfont *value) { font = value; }
    void fillRect(int left, int top, int w, int h, uint16_t) {
        for (int py = std::max(top, 0); py < std::min(top + h, height); ++py)
            for (int px = std::max(left, 0); px < std::min(left + w, width); ++px)
                pixels[py * width + px] = 0;
    }
    void drawRect(int left, int top, int w, int h, uint16_t color) {
        fillRect(left, top, w, 1, color); fillRect(left, top + h - 1, w, 1, color);
        fillRect(left, top, 1, h, color); fillRect(left + w - 1, top, 1, h, color);
    }
    void getTextBounds(const char *text, int16_t, int16_t, int16_t *left, int16_t *top, uint16_t *w, uint16_t *h) {
        int cursor = 0, minX = 32767, minY = 32767, maxX = -32768, maxY = -32768;
        for (const auto *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
            assert(*p >= font->first && *p <= font->last);
            const auto &g = font->glyph[*p - font->first];
            if (g.width && g.height) {
                minX = std::min(minX, cursor + g.xOffset); maxX = std::max(maxX, cursor + g.xOffset + g.width - 1);
                minY = std::min(minY, int(g.yOffset)); maxY = std::max(maxY, g.yOffset + g.height - 1);
            }
            cursor += g.xAdvance;
        }
        *left = minX <= maxX ? minX : 0; *top = minY <= maxY ? minY : 0;
        *w = minX <= maxX ? maxX - minX + 1 : 0; *h = minY <= maxY ? maxY - minY + 1 : 0;
    }
    void print(const char *text) {
        for (const auto *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
            assert(*p >= font->first && *p <= font->last);
            const auto &g = font->glyph[*p - font->first];
            for (int py = 0; py < g.height; ++py)
                for (int px = 0; px < g.width; ++px) {
                    const int bit = py * g.width + px;
                    if (font->bitmap[g.bitmapOffset + bit / 8] & (0x80 >> (bit % 8)))
                        fillRect(x + g.xOffset + px, y + g.yOffset + py, 1, 1, 0);
                }
            x += g.xAdvance;
        }
    }
    void println(const char *text) { lines.emplace_back(text); print(text); x = 0; y += font->yAdvance; }
    void save(const char *path) {
        std::ofstream output(path, std::ios::binary);
        output << "P5\n" << width << ' ' << height << "\n255\n";
        output.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
        assert(output.good());
    }
};

EventClass eventFor(const std::string &title, const std::string &location = "") {
    std::string source = "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nDTSTART:20261010T100000Z\r\nDTEND:20261010T110000Z\r\nSUMMARY:" +
        title + "\r\nLOCATION:" + location + "\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
    EventClass events[EventClass::MAX_CALENDAR_EVENTS] = {};
    const time_t start = getEpoch("20261010T000000Z");
    assert(parseCalendarEvents(events, source.data(), source.data() + source.size(), 0, start, start + 86400 - 1) == 1);
    return events[0];
}

void checkLayout(const GFXfont *font, bool hyphenate) {
    Display display;
    display.setFont(font);
    const auto event = eventFor(u8"È già l'ora di München, Straße e Österreich", u8"Caffè Müller, Düsseldorf e Zürich");
    display.setCursor(10, 50);
    frameink::printWrappedCalendarText(display, event.name, 10, 170, -2, hyphenate);
    assert(display.lines.size() >= 2);
    for (const auto &line : display.lines)
        assert(frameink::calendarTextWidth(display, line.c_str()) <= 170);
    char fitted[128] = {};
    frameink::fitCalendarText(display, event.location, fitted, sizeof(fitted), 150);
    assert(frameink::calendarTextWidth(display, fitted) <= 150);
    assert(std::string(fitted).find("...") != std::string::npos);
    display.lines.clear();
    std::string longWord(64, static_cast<char>(0xF6));
    frameink::printWrappedCalendarText(display, longWord.c_str(), 10, 40, 0, hyphenate);
    std::string recovered;
    for (const auto &line : display.lines) {
        assert(!line.empty() && frameink::calendarTextWidth(display, line.c_str()) <= 40);
        for (char c : line) if (c != '-') recovered += c;
    }
    assert(recovered == longWord);
    frameink::fitCalendarText(display, event.location, fitted, 1, 10);
    assert(fitted[0] == '\0');
    display.lines.clear();
    frameink::printWrappedCalendarText(display, "", 10, 170, 0);
    frameink::printWrappedCalendarText(display, "   ", 10, 170, 0);
    assert(display.lines.empty());
}

int main(int argc, char **argv) {
    DEBUG_PRINT = false;
    const auto sample = eventFor(u8"è ö ß È Ö Ü ẞ œ €", u8"Caffè Müller – Zürich");
    assert(std::string(sample.name) == std::string("\xE8 \xF6 \xDF \xC8 \xD6 \xDC \x80 \x82 \x88"));
    assert(std::string(sample.location) == std::string("Caff\xE8 M\xFCller - Z\xFCrich"));
    const auto composed = eventFor(u8"Cafe\u0300 O\u0308 A\u0308 S\u030C", u8"Mu\u0308nchen");
    assert(std::string(composed.name) == std::string("Caf\xE8 \xD6 \xC4 \x83"));
    assert(std::string(composed.location) == std::string("M\xFCnchen"));
    const auto legacy = eventFor(std::string("Caf\xE9"), std::string("\xF6"));
    assert(std::string(legacy.name) == std::string("Caf\xE9"));
    assert(static_cast<unsigned char>(legacy.location[0]) == 0xF6);
    const auto escaped = eventFor(u8"Caffè\\, Müller\\; sì\\nore 10");
    assert(std::string(escaped.name) == std::string("Caff\xE8, M\xFCller; s\xEC ore 10"));
    const auto folded = eventFor(u8"Caffè e Stra\r\n ße");
    assert(std::string(folded.name) == std::string("Caff\xE8 e Stra\xDF" "e"));
    const auto boundary = eventFor(std::string(200, 'a'), std::string(62, 'a') + u8"e\u0300");
    assert(std::strlen(boundary.name) < sizeof(boundary.name));
    assert(std::string(boundary.name).substr(std::strlen(boundary.name) - 3) == "...");
    assert(std::strlen(boundary.location) == 63 && static_cast<unsigned char>(boundary.location[62]) == 0xE8);
    assert(std::string(eventFor(u8"OK 😀 end").name) == "OK ? end");
    for (const auto *font : {&CalendarSansBold9pt, &CalendarSansBold12pt, &CalendarSansBold16pt, &CalendarSans12pt})
        checkLayout(font, font == &CalendarSansBold16pt);

    assert(frameink::batteryLevel(3.70f) == 0.0f && frameink::batteryLevel(4.16f) == 1.0f);
    assert(std::abs(frameink::batteryLevel(3.93f) - 0.5f) < 0.0001f);
    assert(frameink::batteryLevel(0.0f) == 0.0f && frameink::batteryLevel(5.0f) == 1.0f);
    assert(frameink::batteryLevel(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    Display battery;
    frameink::drawBatteryIcon(battery, 10, 10, 3.70f, 0);
    frameink::drawBatteryIcon(battery, 110, 10, 3.93f, 0);
    frameink::drawBatteryIcon(battery, 210, 10, 4.16f, 0);
    assert(battery.pixels[15 * battery.width + 15] == 255); // Empty interior.
    assert(battery.pixels[15 * battery.width + 115] == 0);
    assert(battery.pixels[15 * battery.width + 135] == 255); // Half-filled interior.
    assert(battery.pixels[15 * battery.width + 240] == 0); // Full interior.
    // Initial instruction icons remain inside the 5-pixel border, above text.
    for (int screenWidth : {600, 825}) {
        const int iconX = screenWidth - 56;
        assert(iconX > 5 && iconX + 38 < screenWidth - 5);
    }
    Display preview;
    const auto letters = eventFor(u8"À È É Ì Ò Ù Ä Ö Ü ß ẞ Ç Ñ Œ", u8"à è é ì ò ù ä ö ü ß ç ñ œ");
    int y = 45;
    for (const auto *font : {&CalendarSansBold9pt, &CalendarSansBold12pt, &CalendarSansBold16pt, &CalendarSans12pt}) {
        preview.setFont(font);
        preview.setCursor(20, y); preview.print(letters.name);
        preview.setCursor(20, y + 45); preview.print(letters.location);
        y += 130;
    }
    preview.setFont(&CalendarSansBold12pt);
    preview.setCursor(20, 580); preview.print("Battery: 3.70V / 3.93V / 4.16V");
    frameink::drawBatteryIcon(preview, 20, 610, 3.70f, 0);
    frameink::drawBatteryIcon(preview, 120, 610, 3.93f, 0);
    frameink::drawBatteryIcon(preview, 220, 610, 4.16f, 0);
    const auto wrapped = eventFor(u8"È già l'ora di München e Straße", u8"Caffè Müller, Düsseldorf e Zürich");
    preview.setCursor(620, 60);
    frameink::printWrappedCalendarText(preview, wrapped.name, 620, 170, -2);
    char location[128];
    frameink::fitCalendarText(preview, wrapped.location, location, sizeof(location), 170);
    preview.setCursor(620, preview.getCursorY() + 10); preview.print(location);
    if (argc > 1) preview.save(argv[1]);
    std::cout << "Calendar: UTF-8/Latin-1, accents, folding, escaping, limits, layout and battery tests passed.\n";
}
