#include "CalendarService.h"

#include <cstdlib>
#include "../Diagnostics/DiagnosticLog.h"

namespace frameink {

CalendarService::CalendarService(Network *network) : network_(network)
{
}

void CalendarService::setNetwork(Network *network)
{
    network_ = network;
}

bool CalendarService::loadAgenda(CalendarAgenda &agenda, int timeZoneSeconds, int visibleDays, time_t currentEpoch)
{
    resetAgenda(agenda);

    if (network_ == nullptr || !network_->configuration().device.hasCalendarUrl() ||
        visibleDays < 1 || visibleDays > 7 || currentEpoch < 1577836800)
        return false;

    char *rawCalendarData = static_cast<char *>(ps_malloc(SIZE_CALENDAR_DATA));
    if (rawCalendarData == nullptr)
    {
        diagnosticLog("Calendar PSRAM allocation failed (%ld bytes)", SIZE_CALENDAR_DATA);
        return false;
    }

    const long downloadSize = network_->getDataCalendar(rawCalendarData, SIZE_CALENDAR_DATA);
    if (downloadSize <= 0)
    {
        diagnosticLog("Calendar unavailable; retry on a later scheduled cycle");
        free(rawCalendarData);
        return false;
    }
    diagnosticStage("calendar_parse");

    agenda.available = true;
    agenda.downloadSize = downloadSize;
    extractCalendarName(agenda, rawCalendarData);

    char *beginEvent = rawCalendarData;
    char *endCalendar = rawCalendarData + downloadSize;
    const time_t epochFirstDayShown = resetEpochOf(currentEpoch, false, false, false, true, true, true);
    const time_t epochLastDayShown = epochFirstDayShown + visibleDays * DAYS_2_SEC - 1;

    agenda.eventCount = parseCalendarEvents(
        agenda.events.data(),
        beginEvent,
        endCalendar,
        timeZoneSeconds,
        epochFirstDayShown,
        epochLastDayShown);

    if (agenda.eventCount < 0)
    {
        diagnosticLog("Calendar parsing exceeded its resource budget");
        resetAgenda(agenda);
        free(rawCalendarData);
        return false;
    }
    qsort(agenda.events.data(), agenda.eventCount, sizeof(EventClass), compareEvents);
    diagnosticLog("Calendar loaded events=%d bytes=%ld", agenda.eventCount, downloadSize);

    free(rawCalendarData);
    return true;
}

void CalendarService::resetAgenda(CalendarAgenda &agenda) const
{
    agenda.available = false;
    agenda.downloadSize = 0;
    agenda.eventCount = 0;
    agenda.name[0] = '\0';
}

void CalendarService::extractCalendarName(CalendarAgenda &agenda, const char *rawData) const
{
    const char *start = strstr(rawData, "X-WR-CALNAME:");
    if (start == nullptr)
        return;

    start += strlen("X-WR-CALNAME:");
    const char *end = strchr(start, '\n');
    if (end == nullptr || end <= start)
        return;

    const size_t copyLength = min(static_cast<size_t>(end - start), sizeof(agenda.name) - 1);
    strncpy(agenda.name, start, copyLength);
    agenda.name[copyLength] = '\0';

    const size_t length = strlen(agenda.name);
    if (length > 0 && agenda.name[length - 1] == '\r')
        agenda.name[length - 1] = '\0';
}

int CalendarService::compareEvents(const void *lhs, const void *rhs)
{
    const EventClass *entryA = static_cast<const EventClass *>(lhs);
    const EventClass *entryB = static_cast<const EventClass *>(rhs);
    return (entryA->timeStamp > entryB->timeStamp) - (entryA->timeStamp < entryB->timeStamp);
}

} // namespace frameink
