#pragma once

#include "../EventClass.h"

#include <array>

namespace frameink {

struct CalendarAgenda
{
    bool available = false;
    long downloadSize = 0;
    int eventCount = 0;
    char name[64] = "";
    std::array<EventClass, EventClass::MAX_CALENDAR_EVENTS> events;
};

} // namespace frameink
