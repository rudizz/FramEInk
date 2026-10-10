#pragma once

#include <Inkplate.h>

// Inkplate 11 moved image methods to display.image. Keep both library APIs usable.
namespace frameink {
namespace detail {

template <typename Display>
auto drawBitmap3Bit(Display &display, int x, int y, const uint8_t *bitmap, int width, int height, int)
    -> decltype(display.drawBitmap3Bit(x, y, bitmap, width, height), void())
{
    display.drawBitmap3Bit(x, y, bitmap, width, height);
}

template <typename Display>
auto drawBitmap3Bit(Display &display, int x, int y, const uint8_t *bitmap, int width, int height, long)
    -> decltype(display.image.drawBitmap3Bit(x, y, bitmap, width, height), void())
{
    display.image.drawBitmap3Bit(x, y, bitmap, width, height);
}

template <typename Display>
auto drawImage(Display &display, const char *path, int x, int y, bool dither, bool invert, int)
    -> decltype(display.drawImage(path, x, y, dither, invert))
{
    return display.drawImage(path, x, y, dither, invert);
}

template <typename Display>
auto drawImage(Display &display, const char *path, int x, int y, bool dither, bool invert, long)
    -> decltype(display.image.draw(path, x, y, dither, invert))
{
    return display.image.draw(path, x, y, dither, invert);
}

} // namespace detail

inline void drawBitmap3Bit(Inkplate &display, int x, int y, const uint8_t *bitmap, int width, int height)
{
    detail::drawBitmap3Bit(display, x, y, bitmap, width, height, 0);
}

inline bool drawImage(Inkplate &display, const char *path, int x, int y, bool dither, bool invert)
{
    return detail::drawImage(display, path, x, y, dither, invert, 0);
}

} // namespace frameink
