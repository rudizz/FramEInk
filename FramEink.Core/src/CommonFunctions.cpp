#include "CommonFunctions.h"

#include <cctype>
#include <cstdlib>

#pragma region Variables

RTC_DATA_ATTR time_t last_awake = 0;

#pragma endregion

bool stringContain(char* str1, const char* pattern)
{
    return strspn(str1, pattern) == strlen(pattern);
}

time_t checkLastAwake(long long minutes, time_t nowEpoch)
{
    time_t diffLastAwake = nowEpoch - last_awake;
    if (nowEpoch - last_awake < minutes * 60l)
    {
        if (DEBUG_PRINT)
            Serial.printf("Last awake %d too soon.Let's sleep again for %d minutes.\n", last_awake, minutes);
        esp_sleep_enable_timer_wakeup(minutes * MIN_2_MICROSEC);
        (void)esp_deep_sleep_start();
    }
    last_awake = nowEpoch;
    return diffLastAwake;
}

// L'apostrofo viene visto erroneamente come carattere 226,
// quindi lo cerco e sostituisco con il carattere presente nei Font a 7 bit.
void correggiApostrofo(char* text, size_t lengthText)
{
    for (size_t i = 0; i < lengthText; i++)
    {
        if ((int)text[i] == 226)
            text[i] = '\'';
    }
}

// Il carriage return viene visto erroneamente come insieme di caratteri \n,
// quindi lo cerco e sostituisco con due spazi.
void correggiCarriageReturn(char* text, size_t lengthText)
{
    if (lengthText == 0)
        return;

    for (size_t i = 0; i + 1 < lengthText; i++)
    {
        if (text[i] == '\\' && text[i + 1] == 'n')
        {
            text[i++] = ' ';
            text[i] = ' ';
        }
    }
}

namespace {

constexpr size_t MAX_ICAL_PROPERTY_LENGTH = 384;
constexpr size_t MAX_ICAL_TEXT_LENGTH = 160;
constexpr size_t MAX_EXDATE_VALUES = 24;

enum class RecurrenceFrequency
{
    None,
    Daily,
    Weekly,
    Monthly,
    Yearly,
};

struct ICalProperty
{
    bool found = false;
    bool isDateOnly = false;
    bool isUtc = false;
    bool hasTimezoneId = false;
    char timezoneId[48] = "";
    char value[MAX_ICAL_PROPERTY_LENGTH] = "";
};

struct ExDateList
{
    time_t values[MAX_EXDATE_VALUES] = {};
    bool dateOnly[MAX_EXDATE_VALUES] = {};
    size_t count = 0;
};

struct RecurrenceRule
{
    RecurrenceFrequency frequency = RecurrenceFrequency::None;
    size_t interval = 1;
    bool hasUntil = false;
    time_t until = 0;
    int count = 0;
    bool byDay[7] = { false, false, false, false, false, false, false };
    bool hasByDay = false;
    int byMonthDays[8] = {};
    size_t byMonthDayCount = 0;
    int byMonths[12] = {};
    size_t byMonthCount = 0;
};

const char* findLineEnd(const char* start, const char* limit)
{
    const char* cursor = start;
    while (cursor < limit && *cursor != '\r' && *cursor != '\n')
        ++cursor;
    return cursor;
}

const char* skipLineBreaks(const char* cursor, const char* limit)
{
    while (cursor < limit && (*cursor == '\r' || *cursor == '\n'))
        ++cursor;
    return cursor;
}

void appendTextSegment(char* destination, size_t destinationSize, const char* source, size_t sourceLength)
{
    size_t currentLength = strlen(destination);
    if (currentLength >= destinationSize - 1)
        return;

    size_t toCopy = sourceLength;
    if (currentLength + toCopy >= destinationSize)
        toCopy = destinationSize - currentLength - 1;

    memcpy(destination + currentLength, source, toCopy);
    destination[currentLength + toCopy] = 0;
}

bool isPropertyStart(const char* lineStart, const char* lineEnd, const char* propertyName)
{
    size_t propertyLength = strlen(propertyName);
    if ((size_t)(lineEnd - lineStart) < propertyLength)
        return false;
    if (strncmp(lineStart, propertyName, propertyLength) != 0)
        return false;

    char separator = lineStart[propertyLength];
    return separator == ':' || separator == ';';
}

bool readPropertyLine(const char* searchStart,
    const char* eventEnd,
    const char* propertyName,
    char* output,
    size_t outputSize,
    const char** nextSearchStart)
{
    const char* lineStart = searchStart;
    while (lineStart < eventEnd)
    {
        const char* lineEnd = findLineEnd(lineStart, eventEnd);
        if (isPropertyStart(lineStart, lineEnd, propertyName))
        {
            output[0] = 0;
            appendTextSegment(output, outputSize, lineStart, (size_t)(lineEnd - lineStart));

            const char* nextLine = skipLineBreaks(lineEnd, eventEnd);
            while (nextLine < eventEnd && (*nextLine == ' ' || *nextLine == '\t'))
            {
                const char* continuationStart = nextLine + 1;
                const char* continuationEnd = findLineEnd(continuationStart, eventEnd);
                appendTextSegment(output, outputSize, continuationStart, (size_t)(continuationEnd - continuationStart));
                nextLine = skipLineBreaks(continuationEnd, eventEnd);
            }

            if (nextSearchStart != nullptr)
                *nextSearchStart = nextLine;
            return true;
        }

        lineStart = skipLineBreaks(lineEnd, eventEnd);
    }

    if (nextSearchStart != nullptr)
        *nextSearchStart = eventEnd;
    return false;
}

void trimLineEnd(char* text)
{
    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == '\r' || text[length - 1] == '\n'))
        text[--length] = 0;
}

void parseProperty(const char* line, const char* propertyName, ICalProperty& property)
{
    property = ICalProperty {};
    if (line == nullptr)
        return;

    size_t nameLength = strlen(propertyName);
    if (strncmp(line, propertyName, nameLength) != 0)
        return;

    const char* cursor = line + nameLength;
    const char* valueSeparator = strchr(cursor, ':');
    if (valueSeparator == nullptr)
        return;

    property.found = true;

    while (cursor < valueSeparator)
    {
        if (*cursor != ';')
        {
            ++cursor;
            continue;
        }

        ++cursor;
        const char* parameterEnd = cursor;
        while (parameterEnd < valueSeparator && *parameterEnd != ';')
            ++parameterEnd;

        size_t parameterLength = (size_t)(parameterEnd - cursor);
        if (parameterLength >= 10 && strncmp(cursor, "VALUE=DATE", 10) == 0)
        {
            property.isDateOnly = true;
        }
        else if (parameterLength > 5 && strncmp(cursor, "TZID=", 5) == 0)
        {
            property.hasTimezoneId = true;
            size_t tzidLength = parameterLength - 5;
            if (tzidLength >= sizeof(property.timezoneId))
                tzidLength = sizeof(property.timezoneId) - 1;
            memcpy(property.timezoneId, cursor + 5, tzidLength);
            property.timezoneId[tzidLength] = 0;
        }

        cursor = parameterEnd;
    }

    strncpy(property.value, valueSeparator + 1, sizeof(property.value) - 1);
    property.value[sizeof(property.value) - 1] = 0;
    trimLineEnd(property.value);

    if (strchr(property.value, 'T') == nullptr)
        property.isDateOnly = true;

    size_t valueLength = strlen(property.value);
    property.isUtc = valueLength > 0 && property.value[valueLength - 1] == 'Z';
}

void decodeIcalText(const char* source, char* destination, size_t destinationSize)
{
    size_t outIndex = 0;
    for (size_t index = 0; source[index] != 0 && outIndex + 1 < destinationSize; ++index)
    {
        if (source[index] == '\\')
        {
            char next = source[index + 1];
            if (next == 0)
                break;

            switch (next)
            {
            case 'n':
            case 'N':
                destination[outIndex++] = ' ';
                break;
            case '\\':
            case ',':
            case ';':
                destination[outIndex++] = next;
                break;
            default:
                destination[outIndex++] = next;
                break;
            }
            ++index;
            continue;
        }

        if (source[index] != '\r' && source[index] != '\n')
            destination[outIndex++] = source[index];
    }

    destination[outIndex] = 0;
    correggiApostrofo(destination, outIndex);
    correggiCarriageReturn(destination, outIndex);
}

bool parseIcsNumber(const char* text, size_t start, size_t length, int& number)
{
    number = 0;
    for (size_t index = 0; index < length; ++index)
    {
        char character = text[start + index];
        if (!isdigit((unsigned char)character))
            return false;
        number = number * 10 + (character - '0');
    }
    return true;
}

bool parseICalDateTime(const ICalProperty& property, time_t& epoch)
{
    if (!property.found || property.value[0] == 0)
        return false;

    char compact[MAX_ICAL_PROPERTY_LENGTH] = "";
    size_t compactLength = 0;
    for (size_t index = 0; property.value[index] != 0 && compactLength + 1 < sizeof(compact); ++index)
    {
        char character = property.value[index];
        if (isdigit((unsigned char)character) || character == 'T' || character == 'Z')
            compact[compactLength++] = character;
    }
    compact[compactLength] = 0;

    if (compactLength < 8)
        return false;

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    if (!parseIcsNumber(compact, 0, 4, year) ||
        !parseIcsNumber(compact, 4, 2, month) ||
        !parseIcsNumber(compact, 6, 2, day))
    {
        return false;
    }

    const char* timeMarker = strchr(compact, 'T');
    if (timeMarker != nullptr)
    {
        size_t timeOffset = (size_t)(timeMarker - compact) + 1;
        if (compactLength >= timeOffset + 4)
        {
            if (!parseIcsNumber(compact, timeOffset, 2, hour) ||
                !parseIcsNumber(compact, timeOffset + 2, 2, minute))
            {
                return false;
            }
        }
        if (compactLength >= timeOffset + 6 && compact[timeOffset + 4] != 'Z')
        {
            if (!parseIcsNumber(compact, timeOffset + 4, 2, second))
                return false;
        }
    }

    tm tmValue = {};
    tmValue.tm_year = year - 1900;
    tmValue.tm_mon = month - 1;
    tmValue.tm_mday = day;
    tmValue.tm_hour = property.isDateOnly ? 0 : hour;
    tmValue.tm_min = property.isDateOnly ? 0 : minute;
    tmValue.tm_sec = property.isDateOnly ? 0 : second;

    epoch = mktime(&tmValue);
    return true;
}

long parseDurationSeconds(const char* durationValue)
{
    if (durationValue == nullptr || durationValue[0] == 0)
        return 0;

    const char* cursor = durationValue;
    bool negative = false;
    if (*cursor == '-')
    {
        negative = true;
        ++cursor;
    }
    if (*cursor != 'P')
        return 0;

    ++cursor;
    bool inTimeSection = false;
    long durationSeconds = 0;

    while (*cursor != 0)
    {
        if (*cursor == 'T')
        {
            inTimeSection = true;
            ++cursor;
            continue;
        }

        char* endCursor = nullptr;
        long value = strtol(cursor, &endCursor, 10);
        if (endCursor == cursor || endCursor == nullptr || *endCursor == 0)
            break;

        switch (*endCursor)
        {
        case 'W':
            durationSeconds += value * 7L * DAYS_2_SEC;
            break;
        case 'D':
            durationSeconds += value * DAYS_2_SEC;
            break;
        case 'H':
            if (inTimeSection)
                durationSeconds += value * 3600L;
            break;
        case 'M':
            if (inTimeSection)
                durationSeconds += value * 60L;
            break;
        case 'S':
            if (inTimeSection)
                durationSeconds += value;
            break;
        default:
            break;
        }

        cursor = endCursor + 1;
    }

    return negative ? -durationSeconds : durationSeconds;
}

int getWeekdayIndex(const tm& dateTime)
{
    return (dateTime.tm_wday + 6) % 7;
}

int getWeekdayIndexFromToken(const char* token)
{
    size_t length = strlen(token);
    if (length < 2)
        return -1;

    const char* code = token + length - 2;
    if (strncmp(code, "MO", 2) == 0) return 0;
    if (strncmp(code, "TU", 2) == 0) return 1;
    if (strncmp(code, "WE", 2) == 0) return 2;
    if (strncmp(code, "TH", 2) == 0) return 3;
    if (strncmp(code, "FR", 2) == 0) return 4;
    if (strncmp(code, "SA", 2) == 0) return 5;
    if (strncmp(code, "SU", 2) == 0) return 6;
    return -1;
}

int daysInMonth(int year, int month)
{
    static const int daysPerMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2)
    {
        bool leapYear = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leapYear ? 29 : 28;
    }
    return daysPerMonth[month - 1];
}

bool listContainsMonthDay(const RecurrenceRule& rule, int year, int month, int monthDay)
{
    if (rule.byMonthDayCount == 0)
        return true;

    int monthLength = daysInMonth(year, month);
    for (size_t index = 0; index < rule.byMonthDayCount; ++index)
    {
        int value = rule.byMonthDays[index];
        if (value > 0 && value == monthDay)
            return true;
        if (value < 0 && monthLength + value + 1 == monthDay)
            return true;
    }
    return false;
}

bool listContainsMonth(const RecurrenceRule& rule, int month)
{
    if (rule.byMonthCount == 0)
        return true;

    for (size_t index = 0; index < rule.byMonthCount; ++index)
    {
        if (rule.byMonths[index] == month)
            return true;
    }
    return false;
}

bool parseCsvIntegers(const char* source, int* destination, size_t destinationCapacity, size_t& count)
{
    count = 0;
    if (source == nullptr || source[0] == 0)
        return false;

    char buffer[MAX_ICAL_PROPERTY_LENGTH] = "";
    strncpy(buffer, source, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = 0;

    char* savePtr = nullptr;
    char* token = strtok_r(buffer, ",", &savePtr);
    while (token != nullptr && count < destinationCapacity)
    {
        destination[count++] = atoi(token);
        token = strtok_r(nullptr, ",", &savePtr);
    }

    return count > 0;
}

void parseRecurrenceRule(const char* ruleValue, int timeZoneSeconds, RecurrenceRule& rule)
{
    rule = RecurrenceRule {};
    if (ruleValue == nullptr || ruleValue[0] == 0)
        return;

    char ruleBuffer[MAX_ICAL_PROPERTY_LENGTH] = "";
    strncpy(ruleBuffer, ruleValue, sizeof(ruleBuffer) - 1);
    ruleBuffer[sizeof(ruleBuffer) - 1] = 0;

    char* savePtr = nullptr;
    char* token = strtok_r(ruleBuffer, ";", &savePtr);
    while (token != nullptr)
    {
        char* separator = strchr(token, '=');
        if (separator != nullptr)
        {
            *separator = 0;
            const char* key = token;
            const char* value = separator + 1;

            if (strcmp(key, "FREQ") == 0)
            {
                if (strcmp(value, "DAILY") == 0) rule.frequency = RecurrenceFrequency::Daily;
                else if (strcmp(value, "WEEKLY") == 0) rule.frequency = RecurrenceFrequency::Weekly;
                else if (strcmp(value, "MONTHLY") == 0) rule.frequency = RecurrenceFrequency::Monthly;
                else if (strcmp(value, "YEARLY") == 0) rule.frequency = RecurrenceFrequency::Yearly;
            }
            else if (strcmp(key, "INTERVAL") == 0)
            {
                long interval = atol(value);
                rule.interval = interval > 0 ? (size_t)interval : 1;
            }
            else if (strcmp(key, "COUNT") == 0)
            {
                rule.count = atoi(value);
            }
            else if (strcmp(key, "UNTIL") == 0)
            {
                ICalProperty untilProperty;
                untilProperty.found = true;
                untilProperty.isDateOnly = strchr(value, 'T') == nullptr;
                strncpy(untilProperty.value, value, sizeof(untilProperty.value) - 1);
                untilProperty.value[sizeof(untilProperty.value) - 1] = 0;
                trimLineEnd(untilProperty.value);

                size_t valueLength = strlen(untilProperty.value);
                untilProperty.isUtc = valueLength > 0 && untilProperty.value[valueLength - 1] == 'Z';

                if (parseICalDateTime(untilProperty, rule.until))
                {
                    if (untilProperty.isDateOnly)
                        rule.until += DAYS_2_SEC - 1;
                    else if (untilProperty.isUtc)
                        rule.until += (time_t)timeZoneSeconds;
                    rule.hasUntil = true;
                }
            }
            else if (strcmp(key, "BYDAY") == 0)
            {
                char daysBuffer[MAX_ICAL_PROPERTY_LENGTH] = "";
                strncpy(daysBuffer, value, sizeof(daysBuffer) - 1);
                daysBuffer[sizeof(daysBuffer) - 1] = 0;

                char* daySavePtr = nullptr;
                char* dayToken = strtok_r(daysBuffer, ",", &daySavePtr);
                while (dayToken != nullptr)
                {
                    int weekdayIndex = getWeekdayIndexFromToken(dayToken);
                    if (weekdayIndex >= 0)
                    {
                        rule.byDay[weekdayIndex] = true;
                        rule.hasByDay = true;
                    }
                    dayToken = strtok_r(nullptr, ",", &daySavePtr);
                }
            }
            else if (strcmp(key, "BYMONTHDAY") == 0)
            {
                parseCsvIntegers(value, rule.byMonthDays, sizeof(rule.byMonthDays) / sizeof(rule.byMonthDays[0]), rule.byMonthDayCount);
            }
            else if (strcmp(key, "BYMONTH") == 0)
            {
                parseCsvIntegers(value, rule.byMonths, sizeof(rule.byMonths) / sizeof(rule.byMonths[0]), rule.byMonthCount);
            }
        }

        token = strtok_r(nullptr, ";", &savePtr);
    }
}

void collectExDates(const char* eventStart, const char* eventEnd, ExDateList& exDates)
{
    exDates = ExDateList {};

    char propertyLine[MAX_ICAL_PROPERTY_LENGTH];
    const char* searchStart = eventStart;
    const char* nextSearchStart = eventStart;

    while (exDates.count < MAX_EXDATE_VALUES &&
        readPropertyLine(searchStart, eventEnd, "EXDATE", propertyLine, sizeof(propertyLine), &nextSearchStart))
    {
        ICalProperty property;
        parseProperty(propertyLine, "EXDATE", property);
        if (property.found)
        {
            char valuesBuffer[MAX_ICAL_PROPERTY_LENGTH] = "";
            strncpy(valuesBuffer, property.value, sizeof(valuesBuffer) - 1);
            valuesBuffer[sizeof(valuesBuffer) - 1] = 0;

            char* savePtr = nullptr;
            char* token = strtok_r(valuesBuffer, ",", &savePtr);
            while (token != nullptr && exDates.count < MAX_EXDATE_VALUES)
            {
                ICalProperty exDateProperty;
                exDateProperty.found = true;
                exDateProperty.isDateOnly = property.isDateOnly || strchr(token, 'T') == nullptr;
                strncpy(exDateProperty.value, token, sizeof(exDateProperty.value) - 1);
                exDateProperty.value[sizeof(exDateProperty.value) - 1] = 0;
                trimLineEnd(exDateProperty.value);

                size_t tokenLength = strlen(exDateProperty.value);
                exDateProperty.isUtc = tokenLength > 0 && exDateProperty.value[tokenLength - 1] == 'Z';

                if (parseICalDateTime(exDateProperty, exDates.values[exDates.count]))
                {
                    exDates.dateOnly[exDates.count] = exDateProperty.isDateOnly;
                    ++exDates.count;
                }

                token = strtok_r(nullptr, ",", &savePtr);
            }
        }

        searchStart = nextSearchStart;
    }
}

bool isExcludedOccurrence(const ExDateList& exDates, time_t occurrenceStart, bool allDay)
{
    tm occurrenceTime = {};
    gmtime_r(&occurrenceStart, &occurrenceTime);

    for (size_t index = 0; index < exDates.count; ++index)
    {
        if (!exDates.dateOnly[index] && !allDay)
        {
            if (exDates.values[index] == occurrenceStart)
                return true;
            continue;
        }

        tm excludedTime = {};
        gmtime_r(&exDates.values[index], &excludedTime);
        if (excludedTime.tm_year == occurrenceTime.tm_year &&
            excludedTime.tm_mon == occurrenceTime.tm_mon &&
            excludedTime.tm_mday == occurrenceTime.tm_mday)
        {
            return true;
        }
    }

    return false;
}

bool overlapsVisibleRange(time_t from, time_t to, time_t visibleFrom, time_t visibleTo)
{
    return !(to < visibleFrom || from > visibleTo);
}

time_t startOfWeek(time_t epoch)
{
    tm dateTime = {};
    gmtime_r(&epoch, &dateTime);
    time_t dayStart = resetEpochOf(epoch, false, false, false, true, true, true);
    return dayStart - getWeekdayIndex(dateTime) * DAYS_2_SEC;
}

bool matchesMonthlyYearlyFilters(const RecurrenceRule& rule, time_t occurrenceStart)
{
    tm occurrenceTime = {};
    gmtime_r(&occurrenceStart, &occurrenceTime);

    int month = occurrenceTime.tm_mon + 1;
    int monthDay = occurrenceTime.tm_mday;
    int year = occurrenceTime.tm_year + 1900;

    if (!listContainsMonth(rule, month))
        return false;
    if (!listContainsMonthDay(rule, year, month, monthDay))
        return false;
    if (rule.hasByDay && !rule.byDay[getWeekdayIndex(occurrenceTime)])
        return false;

    return true;
}

void clearEvent(EventClass& entry)
{
    entry.name[0] = 0;
    entry.time[0] = 0;
    entry.location[0] = 0;
    entry.day = -1;
    entry.timeStamp = 0;
}

void addOccurrenceEntries(EventClass entries[],
    int& entriesNum,
    time_t epochFirstDayShown,
    time_t epochLastDayShown,
    time_t epochFrom,
    time_t epochTo,
    bool allDay,
    const char* summary,
    const char* location)
{
    if (entriesNum >= EventClass::MAX_CALENDAR_EVENTS || epochTo < epochFrom)
        return;

    time_t dayCursor = resetEpochOf(epochFrom, false, false, false, true, true, true);
    time_t endDay = resetEpochOf(epochTo, false, false, false, true, true, true);

    while (dayCursor <= endDay && entriesNum < EventClass::MAX_CALENDAR_EVENTS)
    {
        time_t dayEnd = dayCursor + DAYS_2_SEC - 1;
        time_t partFrom = epochFrom > dayCursor ? epochFrom : dayCursor;
        time_t partTo = epochTo < dayEnd ? epochTo : dayEnd;

        if (overlapsVisibleRange(partFrom, partTo, epochFirstDayShown, epochLastDayShown))
        {
            clearEvent(entries[entriesNum]);
            addEventToEntry(&entries[entriesNum], epochFirstDayShown, partFrom, partTo, allDay, summary, location);
            ++entriesNum;
        }

        dayCursor += DAYS_2_SEC;
    }
}

} // namespace

time_t getEpoch(const char* dateString)
{
    ICalProperty property;
    property.found = true;
    property.isDateOnly = strchr(dateString, 'T') == nullptr;
    strncpy(property.value, dateString, sizeof(property.value) - 1);
    property.value[sizeof(property.value) - 1] = 0;

    size_t length = strlen(property.value);
    property.isUtc = length > 0 && property.value[length - 1] == 'Z';

    time_t epoch = 0;
    if (!parseICalDateTime(property, epoch))
        return 0;
    return epoch;
}

// Set to zero the part of the time of the epoch in input
time_t resetEpochOf(time_t epochToReset, bool year, bool month, bool monthDay, bool hour, bool minute, bool second)
{
    tm tm_epochToReset = {};
    gmtime_r(&epochToReset, &tm_epochToReset);
    if (year)
        tm_epochToReset.tm_year = 0;
    if (month)
        tm_epochToReset.tm_mon = 0;
    if (monthDay)
        tm_epochToReset.tm_mday = 0;
    if (hour)
        tm_epochToReset.tm_hour = 0;
    if (minute)
        tm_epochToReset.tm_min = 0;
    if (second)
        tm_epochToReset.tm_sec = 0;

    return mktime(&tm_epochToReset);
}

#pragma region Calendar

int parseCalendarEvents(EventClass entries[], char* beginEvt, char* endCal, int timeZone, time_t epochFirstDayShown, time_t epochLastDayShown)
{
    int entriesNum = 0;
    char propertyLine[MAX_ICAL_PROPERTY_LENGTH];

    while (entriesNum < EventClass::MAX_CALENDAR_EVENTS && beginEvt < endCal)
    {
        char* eventMarker = strstr(beginEvt, "BEGIN:VEVENT");
        if (eventMarker == nullptr || eventMarker >= endCal)
            break;

        const char* eventStart = eventMarker + 12;
        char* endEvt = strstr((char*)eventStart, "END:VEVENT");
        if (endEvt == nullptr || endEvt > endCal)
            break;

        ICalProperty startProperty;
        ICalProperty endProperty;
        ICalProperty durationProperty;
        ICalProperty summaryProperty;
        ICalProperty locationProperty;
        ICalProperty recurrenceProperty;

        if (readPropertyLine(eventStart, endEvt, "DTSTART", propertyLine, sizeof(propertyLine), nullptr))
            parseProperty(propertyLine, "DTSTART", startProperty);
        if (!startProperty.found)
        {
            beginEvt = endEvt + 10;
            continue;
        }

        if (readPropertyLine(eventStart, endEvt, "DTEND", propertyLine, sizeof(propertyLine), nullptr))
            parseProperty(propertyLine, "DTEND", endProperty);
        if (readPropertyLine(eventStart, endEvt, "DURATION", propertyLine, sizeof(propertyLine), nullptr))
            parseProperty(propertyLine, "DURATION", durationProperty);
        if (readPropertyLine(eventStart, endEvt, "SUMMARY", propertyLine, sizeof(propertyLine), nullptr))
            parseProperty(propertyLine, "SUMMARY", summaryProperty);
        if (readPropertyLine(eventStart, endEvt, "LOCATION", propertyLine, sizeof(propertyLine), nullptr))
            parseProperty(propertyLine, "LOCATION", locationProperty);
        if (readPropertyLine(eventStart, endEvt, "RRULE", propertyLine, sizeof(propertyLine), nullptr))
            parseProperty(propertyLine, "RRULE", recurrenceProperty);

        char summary[MAX_ICAL_TEXT_LENGTH] = "";
        char location[sizeof(entries[0].location)] = "";
        decodeIcalText(summaryProperty.found ? summaryProperty.value : "(No title)", summary, sizeof(summary));
        decodeIcalText(locationProperty.found ? locationProperty.value : "", location, sizeof(location));

        time_t baseStart = 0;
        if (!parseICalDateTime(startProperty, baseStart))
        {
            beginEvt = endEvt + 10;
            continue;
        }

        bool allDay = startProperty.isDateOnly;
        bool applyTimeZone = startProperty.isUtc && !startProperty.hasTimezoneId && !startProperty.isDateOnly;
        if (applyTimeZone)
            baseStart += (time_t)timeZone;

        time_t baseEnd = baseStart;
        if (endProperty.found)
        {
            if (!parseICalDateTime(endProperty, baseEnd))
            {
                beginEvt = endEvt + 10;
                continue;
            }
            if (endProperty.isUtc && !endProperty.hasTimezoneId && !endProperty.isDateOnly)
                baseEnd += (time_t)timeZone;

            if (allDay || endProperty.isDateOnly)
                baseEnd -= 1;
        }
        else if (durationProperty.found)
        {
            long durationSeconds = parseDurationSeconds(durationProperty.value);
            baseEnd = baseStart + durationSeconds;
            if (allDay && durationSeconds > 0)
                baseEnd -= 1;
        }
        else if (allDay)
        {
            baseEnd = baseStart + DAYS_2_SEC - 1;
        }

        if (!allDay && baseEnd > baseStart)
        {
            tm endTime = {};
            gmtime_r(&baseEnd, &endTime);
            if (endTime.tm_hour == 0 && endTime.tm_min == 0 && endTime.tm_sec == 0)
                baseEnd -= 1;
        }

        if (baseEnd < baseStart)
        {
            if (DEBUG_PRINT)
                Serial.printf("\nERRORE: epochFrom: %d, epochTo: %d\n", baseStart, baseEnd);
            beginEvt = endEvt + 10;
            continue;
        }

        time_t duration = baseEnd - baseStart;
        RecurrenceRule recurrence;
        parseRecurrenceRule(recurrenceProperty.found ? recurrenceProperty.value : "", timeZone, recurrence);

        ExDateList exDates;
        collectExDates(eventStart, endEvt, exDates);

        if (recurrence.frequency == RecurrenceFrequency::None)
        {
            if (!isExcludedOccurrence(exDates, baseStart, allDay))
                addOccurrenceEntries(entries, entriesNum, epochFirstDayShown, epochLastDayShown, baseStart, baseEnd, allDay, summary, location);
            beginEvt = endEvt + 10;
            continue;
        }

        int generatedOccurrences = 0;

        if (recurrence.frequency == RecurrenceFrequency::Weekly && recurrence.hasByDay)
        {
            time_t baseDayStart = resetEpochOf(baseStart, false, false, false, true, true, true);
            time_t timeOfDay = baseStart - baseDayStart;
            time_t baseWeekStart = startOfWeek(baseStart);

            for (size_t weekIndex = 0; entriesNum < EventClass::MAX_CALENDAR_EVENTS; ++weekIndex)
            {
                time_t weekStart = baseWeekStart + (time_t)(weekIndex * recurrence.interval * 7) * DAYS_2_SEC;
                bool finished = false;

                for (int weekDay = 0; weekDay < 7 && entriesNum < EventClass::MAX_CALENDAR_EVENTS; ++weekDay)
                {
                    if (!recurrence.byDay[weekDay])
                        continue;

                    time_t occurrenceStart = weekStart + weekDay * DAYS_2_SEC + timeOfDay;
                    if (occurrenceStart < baseStart)
                        continue;
                    if (recurrence.hasUntil && occurrenceStart > recurrence.until)
                    {
                        finished = true;
                        continue;
                    }

                    ++generatedOccurrences;
                    if (recurrence.count > 0 && generatedOccurrences > recurrence.count)
                    {
                        finished = true;
                        break;
                    }

                    time_t occurrenceEnd = occurrenceStart + duration;
                    if (occurrenceStart > epochLastDayShown)
                    {
                        finished = true;
                        break;
                    }

                    if (!isExcludedOccurrence(exDates, occurrenceStart, allDay) &&
                        overlapsVisibleRange(occurrenceStart, occurrenceEnd, epochFirstDayShown, epochLastDayShown))
                    {
                        addOccurrenceEntries(entries, entriesNum, epochFirstDayShown, epochLastDayShown, occurrenceStart, occurrenceEnd, allDay, summary, location);
                    }
                }

                if (finished)
                    break;

                time_t nextWeekStart = weekStart + (time_t)(recurrence.interval * 7) * DAYS_2_SEC + timeOfDay;
                if (nextWeekStart > epochLastDayShown && (!recurrence.hasUntil || nextWeekStart > recurrence.until))
                    break;
            }
        }
        else
        {
            time_t occurrenceStart = baseStart;
            while (entriesNum < EventClass::MAX_CALENDAR_EVENTS)
            {
                if (recurrence.hasUntil && occurrenceStart > recurrence.until)
                    break;
                if (occurrenceStart > epochLastDayShown)
                    break;

                bool matchesFilters = true;
                if (recurrence.frequency == RecurrenceFrequency::Daily)
                {
                    tm occurrenceTime = {};
                    gmtime_r(&occurrenceStart, &occurrenceTime);
                    if (recurrence.hasByDay && !recurrence.byDay[getWeekdayIndex(occurrenceTime)])
                        matchesFilters = false;
                    if (!listContainsMonth(recurrence, occurrenceTime.tm_mon + 1))
                        matchesFilters = false;
                    if (!listContainsMonthDay(recurrence, occurrenceTime.tm_year + 1900, occurrenceTime.tm_mon + 1, occurrenceTime.tm_mday))
                        matchesFilters = false;
                }
                else if (recurrence.frequency == RecurrenceFrequency::Monthly || recurrence.frequency == RecurrenceFrequency::Yearly)
                {
                    matchesFilters = matchesMonthlyYearlyFilters(recurrence, occurrenceStart);
                }

                if (matchesFilters)
                {
                    ++generatedOccurrences;
                    if (recurrence.count > 0 && generatedOccurrences > recurrence.count)
                        break;

                    time_t occurrenceEnd = occurrenceStart + duration;
                    if (!isExcludedOccurrence(exDates, occurrenceStart, allDay) &&
                        overlapsVisibleRange(occurrenceStart, occurrenceEnd, epochFirstDayShown, epochLastDayShown))
                    {
                        addOccurrenceEntries(entries, entriesNum, epochFirstDayShown, epochLastDayShown, occurrenceStart, occurrenceEnd, allDay, summary, location);
                    }
                }

                time_t nextOccurrence = aggiungiEpochRipetizione(
                    occurrenceStart,
                    recurrence.frequency == RecurrenceFrequency::Daily ? 1 :
                    recurrence.frequency == RecurrenceFrequency::Weekly ? 7 :
                    recurrence.frequency == RecurrenceFrequency::Monthly ? 30 :
                    recurrence.frequency == RecurrenceFrequency::Yearly ? 365 : 0,
                    recurrence.interval);

                if (nextOccurrence <= occurrenceStart)
                    break;
                occurrenceStart = nextOccurrence;
            }
        }

        beginEvt = endEvt + 10;
    }

    return entriesNum;
}

time_t aggiungiEpochRipetizione(time_t epoch, uint16_t giorniFrequenzaRipetizione, size_t intervalloRipetizione)
{
    time_t epochOut = epoch;
    switch (giorniFrequenzaRipetizione)
    {
    case 1:
    case 7:
        epochOut = epoch + (time_t)giorniFrequenzaRipetizione * (time_t)intervalloRipetizione * DAYS_2_SEC;
        break;
    case 30:
    {
        tm tm_epoch = {};
        gmtime_r(&epoch, &tm_epoch);
        tm_epoch.tm_mon += (int)intervalloRipetizione;
        epochOut = mktime(&tm_epoch);
        break;
    }
    case 365:
    {
        tm tm_epochY = {};
        gmtime_r(&epoch, &tm_epochY);
        tm_epochY.tm_year += (int)intervalloRipetizione;
        epochOut = mktime(&tm_epochY);
        break;
    }
    default:
        break;
    }
    return epochOut;
}

void addEventToEntry(EventClass* dstEntry, time_t epochFirstDayShown, time_t epochEvtFrom, time_t epochEvtTo, bool allDay, const char* summary, const char* location)
{
    struct tm eventFrom = {};
    struct tm eventTo = {};
    gmtime_r(&epochEvtFrom, &eventFrom);
    gmtime_r(&epochEvtTo, &eventTo);

    if (allDay || (!eventFrom.tm_hour && !eventFrom.tm_min &&
        eventTo.tm_hour == 23 && eventTo.tm_min == 59))
    {
        strncpy(dstEntry->time, "All-day", sizeof(dstEntry->time) - 1);
        dstEntry->time[sizeof(dstEntry->time) - 1] = 0;
    }
    else
    {
        strncpy(dstEntry->time, asctime(&eventFrom) + 11, 5);
        dstEntry->time[5] = ' ';
        dstEntry->time[6] = '-';
        dstEntry->time[7] = ' ';
        strncpy(dstEntry->time + 8, asctime(&eventTo) + 11, 5);
        dstEntry->time[13] = 0;
    }

    const char* safeSummary = (summary != nullptr && summary[0] != 0) ? summary : "(No title)";
    size_t summaryLength = strlen(safeSummary);
    if (summaryLength >= EventClass::MAX_N_CHAR_TITLE_CALENDAR)
        summaryLength = EventClass::MAX_N_CHAR_TITLE_CALENDAR - 1;
    strncpy(dstEntry->name, safeSummary, summaryLength);
    dstEntry->name[summaryLength] = 0;
    if (summaryLength == EventClass::MAX_N_CHAR_TITLE_CALENDAR - 1 && summaryLength >= 4)
    {
        dstEntry->name[summaryLength - 1] = 0;
        dstEntry->name[summaryLength - 2] = '.';
        dstEntry->name[summaryLength - 3] = '.';
        dstEntry->name[summaryLength - 4] = '.';
    }

    const char* safeLocation = location != nullptr ? location : "";
    strncpy(dstEntry->location, safeLocation, sizeof(dstEntry->location) - 1);
    dstEntry->location[sizeof(dstEntry->location) - 1] = 0;

    dstEntry->timeStamp = epochEvtFrom;
    const time_t secondsFromFirstShownDay = epochEvtFrom - epochFirstDayShown;
    dstEntry->day = secondsFromFirstShownDay < 0 ? -1 : (int)(secondsFromFirstShownDay / (24ll * 3600ll));
    if (DEBUG_PRINT)
        Serial.printf("epochFrom: %d , epochFirstDayShown: %d, day: %d\n", dstEntry->timeStamp, epochFirstDayShown, dstEntry->day);
}

#pragma endregion
