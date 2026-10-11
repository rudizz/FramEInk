# Calendar font source

The `CalendarSans*.h` files extend the existing project FreeSans bitmap fonts.
ASCII glyphs and metrics are copied without modification from their original
headers. Additional glyphs are rasterized from GNU FreeFont, release 20120503:

https://ftp.gnu.org/gnu/freefont/freefont-ttf-20120503.zip

Archive SHA-256:
`7c85baf1bf82a1a1845d1322112bc6ca982221b484e3b3925022e25b5cae89af`

Faces: `FreeSans.ttf` and `FreeSansBold.ttf`. Copyright belongs to the GNU
FreeFont contributors listed in AUTHORS. The original README contains the
GPL-3.0-or-later licensing statement and special font exception; COPYING is
the original GNU GPL v3 license. The font exception also applies to the bitmap
subsets generated here.

The generator is `tools/build_calendar_fonts.py`. It uses the Adafruit GFX
`fontconvert` utility (also supplied by InkplateLibrary), which rasterizes via
FreeType at 141 DPI. The glyph mapping is shared with `Calendar/CalendarText.h`.
No fonts need to be installed or copied to the SD card.
