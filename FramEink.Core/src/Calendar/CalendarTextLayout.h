#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace frameink {

template<class Display>
int calendarTextWidth(Display &display, const char *text)
{
    int16_t x, y;
    uint16_t width, height;
    display.getTextBounds(text, 0, 0, &x, &y, &width, &height);
    // Bounds exclude a positive left bearing; include it so the right edge of
    // the actually printed glyph also fits inside the event column.
    return width + (x > 0 ? x : 0);
}

// Input contains one CalendarText glyph per byte. Never split a UTF-8 sequence
// or measure a font byte with an ASCII-only font. Caller sets font and cursor Y.
template<class Display>
void printWrappedCalendarText(Display &display, const char *text, int x, int maxWidth, int lineAdjustment, bool hyphenate = false)
{
    while (*text != '\0')
    {
        while (*text == ' ')
            ++text;
        if (*text == '\0')
            break;
        char line[128] = {};
        size_t fit = 0, lastSpace = 0;
        while (text[fit] != '\0' && fit + 2 < sizeof(line))
        {
            line[fit] = text[fit];
            line[fit + 1] = '\0';
            if (calendarTextWidth(display, line) > maxWidth)
            {
                line[fit] = '\0';
                break;
            }
            if (text[fit] == ' ')
                lastSpace = fit;
            ++fit;
        }
        // Always advance, even with a malformed/implausibly narrow text box.
        if (fit == 0)
            fit = 1;
        size_t consumed = fit;
        const bool splitWord = text[fit] != '\0' && text[fit] != ' ' && lastSpace == 0;
        if (text[fit] != '\0' && text[fit] != ' ' && lastSpace > 0)
            consumed = lastSpace;
        if (splitWord && hyphenate && fit > 1)
        {
            while (consumed > 1) {
                std::memcpy(line, text, consumed);
                line[consumed] = '-';
                line[consumed + 1] = '\0';
                if (calendarTextWidth(display, line) <= maxWidth)
                    break;
                --consumed;
            }
            std::memcpy(line, text, consumed);
            line[consumed] = '-';
            line[consumed + 1] = '\0';
            if (calendarTextWidth(display, line) > maxWidth)
                line[consumed] = '\0';
        }
        else
        {
            std::memcpy(line, text, consumed);
            line[consumed] = '\0';
        }
        display.setCursor(x, display.getCursorY() + lineAdjustment);
        display.println(line);
        text += consumed;
    }
}

template<class Display>
void fitCalendarText(Display &display, const char *text, char *output, size_t capacity, int maxWidth)
{
    if (capacity == 0)
        return;
    size_t length = std::strlen(text);
    if (length >= capacity)
        length = capacity - 1;
    std::memcpy(output, text, length);
    output[length] = '\0';
    if (text[length] == '\0' && calendarTextWidth(display, output) <= maxWidth)
        return;
    // Ellipsis is included in the width calculation, not painted over letters.
    while (length > 0)
    {
        if (length + 4 <= capacity)
        {
            std::memcpy(output + length, "...", 4);
            if (calendarTextWidth(display, output) <= maxWidth)
                return;
        }
        --length;
    }
    output[0] = '\0';
    if (capacity >= 4 && calendarTextWidth(display, "...") <= maxWidth)
        std::memcpy(output, "...", 4);
}

} // namespace frameink
