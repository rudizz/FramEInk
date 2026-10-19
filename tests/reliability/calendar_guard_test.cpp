#include <CommonFunctions.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
int parse(std::string calendar, EventClass *events) {
    const time_t first = getEpoch("20261018T000000Z");
    return parseCalendarEvents(events, &calendar[0], &calendar[0] + calendar.size(), 0, first, first + 6 * DAYS_2_SEC - 1);
}
int main() {
    setenv("TZ", "UTC0", 1);
    tzset();
    EventClass events[EventClass::MAX_CALENDAR_EVENTS];
    assert(getEpoch("20260230T100000Z") == 0);
    assert(getEpoch("99991018T100000Z") == 0);
    assert(getEpoch("20261018T250000Z") == 0);
    const std::string prefix = "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\n";
    const std::string suffix = "SUMMARY:Test\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
    assert(parse(prefix + "DTSTART:20261018T100000Z\r\nDTEND:20261018T110000Z\r\n" + suffix, events) == 1);
    assert(parse(prefix + "DTSTART:20261018T100000Z\r\nDURATION:PT9999999999999999999999999999H\r\n" + suffix, events) == 1);
    assert(parse(prefix + "DTSTART:20261018T100000Z\r\nRRULE:FREQ=WEEKLY;INTERVAL=10000;BYDAY=SU\r\n" + suffix, events) == 1);
    assert(parse(prefix + "DTSTART:20261018T100000Z\r\nRRULE:FREQ=DAILY;INTERVAL=9999999999999999999999\r\n" + suffix, events) <= 6);
    assert(parse(prefix + "DTSTART;VALUE=DATE:19700101\r\nDTEND;VALUE=DATE:20370101\r\n" + suffix, events) == 6);
    assert(parse(prefix + "DTSTART:20261018T100000Z\r\nSUMMARY:Truncated", events) == 0);
    std::string expensive;
    for (int i = 0; i < 8; ++i)
        expensive += "BEGIN:VEVENT\r\nDTSTART:19700101T100000Z\r\nRRULE:FREQ=DAILY;COUNT=1000000;BYMONTH=13\r\nEND:VEVENT\r\n";
    testMillis = 0;
    assert(parse(expensive, events) == -1);
    assert(testMillis > 0 && testMillis < 10000);
    assert(aggiungiEpochRipetizione(2147483000, 7, 10000) == 2147483000);
    std::cout << "Calendar guards: invalid dates/durations, huge recurrences, clipping, parser budget and rollover passed.\n";
}
