#pragma once

#include "../CommonFunctions.h"
#include "../Network.h"
#include "CalendarAgenda.h"

namespace frameink {

class CalendarService
{
  public:
    CalendarService() = default;
    explicit CalendarService(Network *network);

    void setNetwork(Network *network);
    bool loadAgenda(CalendarAgenda &agenda, int timeZoneSeconds, int visibleDays, time_t currentEpoch);

  private:
    void resetAgenda(CalendarAgenda &agenda) const;
    void extractCalendarName(CalendarAgenda &agenda, const char *rawData) const;
    static int compareEvents(const void *lhs, const void *rhs);

    Network *network_ = nullptr;
};

} // namespace frameink
