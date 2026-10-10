#pragma once

#include <Inkplate.h>

namespace frameink {

struct PhotoUploadTarget
{
    const char *name = "FramEInk";
    uint16_t width = 0;
    uint16_t height = 0;
    const char *directory = "/portrait";
    bool grayscale = false;
    SdFat *storage = nullptr;
};

} // namespace frameink
