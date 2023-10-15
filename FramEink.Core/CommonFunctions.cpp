#include "CommonFunctions.h"


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
        Serial.println("Last awake too soon. Let's sleep again.");
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
        if ((int)text[i] == 226) {
            text[i] = '\'';
        }
    }
}

// Il carriage return viene visto erroneamente come insieme di caratteri \n,
// quindi lo cerco e sostituisco con due spazi.
void correggiCarriageReturn(char* text, size_t lengthText)
{
    for (size_t i = 0; i < lengthText - 1; i++)
    {
        if (text[i] == '\\' && text[i + 1] == 'n') {
            text[i++] = ' ';
            text[i] = ' ';
        }
    }
}

time_t getEpoch(char* dateString)
{
    struct tm ltm = { 0 };
    char temp[32];

    strncpy(temp, dateString, 16);
    temp[16] = 0;

    // https://github.com/esp8266/Arduino/issues/5141, quickfix
    memmove(temp + 5, temp + 4, 16);
    memmove(temp + 8, temp + 7, 16);
    memmove(temp + 14, temp + 13, 16);
    memmove(temp + 16, temp + 15, 16);
    temp[4] = temp[7] = temp[13] = temp[16] = '-';

    // time.h function
    strptime(temp, "%Y-%m-%dT%H-%M-%SZ", &ltm);

    return mktime(&ltm);
}

// Set to zero the part of the time of the epoch in input
time_t resetEpochOf(time_t epochToReset, bool year, bool month, bool monthDay, bool hour, bool minute, bool second)
{
    tm tm_epochToReset;
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
    // reset count
    int entriesNum = 0;

    // Search raw data for events
    while (entriesNum < EventClass::MAX_CALENDAR_EVENTS && beginEvt < endCal && strstr(beginEvt, "BEGIN:VEVENT"))
    {
        // Find next event start and end
        beginEvt = strstr(beginEvt, "BEGIN:VEVENT") + 12;
        char* endEvt = strstr(beginEvt, "END:VEVENT");

        if (endEvt == NULL)
            continue;

        bool correggiTimeZone = true;
        // Find all relevant event data
        char* summary = strstr(beginEvt, "SUMMARY:") + 8;
        char* location = strstr(beginEvt, "LOCATION:") + 9;

        // Dopo il DTSTART potrebbero esserci dei parametri, quindi li cerco.
        char* timeStart = strstr(beginEvt, "DTSTART") + 7;

        // Se esiste il campo TZID devo riportare l'orario senza correggerlo tramite TimeZone.
        // Il parametro ;VALUE=DATE indica che non è presente l'orario perché l'evento occupa tutto il giorno.
        if (stringContain(timeStart, ";TZID") || stringContain(timeStart, ";VALUE=DATE"))
        {
            correggiTimeZone = false;
        }
        timeStart = strstr(timeStart, ":") + 1;


        char* timeEnd = strstr(beginEvt, "DTEND") + 5;
        timeEnd = strstr(timeEnd, ":") + 1;

        // //Gestisco gli eventi ripetuti nel tempo. Per ora solo Daily e Weekly, Monthly e Yearly.
        char* rRule = strstr(beginEvt, "RRULE:") + 6;
        size_t giorniFrequenzaRipetizione = 0;
        size_t intervalloRipetizione = 1;
        if (rRule && rRule > beginEvt && rRule < endEvt && rRule < endCal)
        {
            if (stringContain(rRule, "FREQ=DAILY"))
                giorniFrequenzaRipetizione = 1;
            else if (stringContain(rRule, "FREQ=WEEKLY"))
                giorniFrequenzaRipetizione = 7;
            else if (stringContain(rRule, "FREQ=MONTHLY"))
                giorniFrequenzaRipetizione = 30;
            else if (stringContain(rRule, "FREQ=YEARLY"))
                giorniFrequenzaRipetizione = 365;
            // Cerco l'intervallo
            char* interval = strstr(rRule, ";INTERVAL=") + 10;
            if (interval > rRule && interval < endEvt)
            {
                char* intervalEnd = strstr(interval, ";");
                intervalloRipetizione = String(interval, intervalEnd - interval).toInt();
            }
            Serial.printf("Frequenza: %d, intervalloRipetizione: %d\n", giorniFrequenzaRipetizione, intervalloRipetizione);
        }
        // Sequenza
//        char *sequence = strstr(data + i, "SEQUENCE:") + 9;
//        Serial.print("Sequence: ");
//        Serial.println(sequence);

//        for (int seq = 0; seq <= atoi(sequence)


        // --------  Controllo se l'evento è da visualizzare  -----------
        time_t epochFrom = getEpoch(timeStart);
        time_t epochTo = getEpoch(timeEnd);

        if (epochTo < epochFrom)
        {
            // In questo caso c'è stato un errore nella lettura delle epoche, quindi passo all'evento successivo
            Serial.printf("\nERRORE: epochFrom: %d, epochTo: %d\n", epochFrom, epochTo);
            continue;
        }
        // Se l'evento dura tutto il giorno, non è presente il campo di ore, min e sec.
        // Inoltre devo togliere 1 secondo da epochTo in modo che finisca alle 23:59.
        if (String(timeStart + 8, 1) != "T")
        {
            epochTo -= 1;
        }

        // In alcuni casi viene passato l'orario già corretto del TimeZone, quindi non devo modificarlo.
        if (correggiTimeZone)
        {
            epochFrom += (time_t)timeZone;
            epochTo += (time_t)timeZone;
        }

        //Serial.printf("epochFirstDayShown: %d\n", epochFirstDayShown);
        //Serial.printf("epochLastDayShown: %d\n", epochLastDayShown);

        // Inizializzo epochUntil con l'ultimo giorno visualizzato nel calendario,
        // in modo che se non è presente il campo viene considerato infinito
        // e i controlli finiscono l'ultimo giorno mostrato nel calendario.
        time_t epochUntil = epochLastDayShown;
        if (giorniFrequenzaRipetizione)
        {
            // Cerco il parametro Until
            char* until = strstr(timeEnd, "UNTIL=") + 6;
            char* dtStamp = strstr(timeEnd, "DTSTAMP:") + 8;
            if (until < dtStamp) // Se è vero, vuol dire che esiste il campo Until
            {
                // Limito il campo Until all'ultimo giorno visualizzato nel calendario.
                epochUntil = min<time_t>(getEpoch(until), epochUntil);
            }
            Serial.printf("epoch until: %d\n", epochUntil);
        }


        struct tm eventFrom, eventTo;
        gmtime_r(&epochFrom, &eventFrom);
        gmtime_r(&epochTo, &eventTo);
        // Conto quanti giorni dura l'evento. Non lo conto con le epoch perché potrebbe durare
        // meno di 24 ore ma essere compreso su due giorni diversi.
        int evtLastNDays = eventTo.tm_yday - eventFrom.tm_yday;
        // Se il giorno di inizio e fine è diverso, correggo l'ora da scrivere nel calendario
        if (evtLastNDays < 0)
        {
            // Gestire il cambio dell'anno, considerando gli anni bisestili.
            eventFrom.tm_mon = 11;
            eventFrom.tm_mday = 31;
            time_t evtFineAnno = mktime(&eventFrom);
            gmtime_r(&evtFineAnno, &eventFrom);
            evtLastNDays += eventFrom.tm_yday + 1;
        }
        time_t epochFromTemp = epochFrom;
        time_t epochToTemp = epochTo;
        time_t epochUntilTemp = epochUntil;

        // Ciclo i giorni degli eventi
        for (size_t iDay = 0; iDay <= evtLastNDays; iDay++)
        {
            epochUntil = epochUntilTemp + iDay * DAYS_2_SEC;
            // Correggo epochFrom e epochTo in modo che durino max 1 giorno
            epochFrom = epochFromTemp + iDay * DAYS_2_SEC;
            if (iDay != 0)
                epochFrom = resetEpochOf(epochFrom, false, false, false, true, true, true);
            if (iDay < evtLastNDays)
                epochTo = resetEpochOf(epochFrom, false, false, false, true, true, true) + DAYS_2_SEC - 1;
            else
                epochTo = epochToTemp;

            // Manca da considerare l'evento:
            // - Pro: inizio 18 feb 2023, dura 2 giorni interi con ripetizione settimanale e termine il 25 feb 2023
            do
            {
                if (epochUntil > epochFirstDayShown &&
                    (epochFrom >= epochFirstDayShown && epochFrom <= epochUntil) ||
                    (epochTo >= epochFirstDayShown && epochTo <= epochUntil) ||
                    (epochFrom < epochFirstDayShown && epochTo > epochUntil))
                {
                    Serial.printf("epochFrom: %d, epochTo: %d, fRep: %d, lastDay: %d, iDay: %d, evtLastNDays: %d\n",
                        epochFrom, epochTo, giorniFrequenzaRipetizione, epochLastDayShown, iDay, evtLastNDays);
                    // -- Giorni di Esclusione --
                    bool excludeDate = false;
                    char* exdate = strstr(timeEnd, "EXDATE") + 6;
                    char* dtStamp = strstr(timeEnd, "DTSTAMP:");
                    //char summ[16];
                    //strncpy(summ, summary, 16);
                    if (exdate < dtStamp && exdate > beginEvt)
                    {
                        //strncpy(summ, exdate, 16);
                        // Se entro qui esiste almeno una data di esclusione.
                        // Le ciclo tutte per vedere se ne esiste una che corrisponde con il
                        // giorno di inizio evento
                        do
                        {
                            exdate = strstr(exdate, ":") + 1;
                            time_t epochExdate = getEpoch(exdate);
                            if (epochExdate == epochFrom)
                            {
                                // Se il giorno dell'evento corrisponde con il giorno di esclusione,
                                // esco dal ciclo e non aggiungo tale evento alla visualizzazione. 
                                excludeDate = true;
                            }
                            // Cerco se esiste un altro campo EXDATE, se non esiste esco dal while
                            exdate = strstr(exdate, "EXDATE") + 6;
                        } while (!excludeDate && exdate < dtStamp && exdate > beginEvt);
                    }
                    if (!excludeDate && epochFrom > epochFirstDayShown && entriesNum < EventClass::MAX_CALENDAR_EVENTS)
                    {
                        addEventToEntry(&entries[entriesNum], epochFirstDayShown,
                            epochFrom, epochTo, String(timeStart + 8, 1) != "T", summary, location, beginEvt, endEvt);
                        Serial.printf("[ADD] entriesNum: %d, Summary: %s\n\n", entriesNum, entries[entriesNum].name);
                        entriesNum++;
                    }
                }
                epochFrom = aggiungiEpochRipetizione(epochFrom,
                    giorniFrequenzaRipetizione,
                    intervalloRipetizione);
                epochTo = aggiungiEpochRipetizione(epochTo,
                    giorniFrequenzaRipetizione,
                    intervalloRipetizione);

            } while (giorniFrequenzaRipetizione > 0 && epochFrom <= epochUntil);
        }

        // Porto il begin un carattere dopo l'end
        beginEvt = endEvt + 11;
    }

    return entriesNum;
}

time_t aggiungiEpochRipetizione(time_t epoch, uint16_t giorniFrequenzaRipetizione, size_t intervalloRipetizione)
{
    time_t epochOut;
    switch (giorniFrequenzaRipetizione)
    {
    case 1:
    case 7:
        epochOut = epoch + giorniFrequenzaRipetizione * DAYS_2_SEC;
        break;
    case 30:
        tm tm_epoch;
        gmtime_r(&epoch, &tm_epoch);
        tm_epoch.tm_mon += intervalloRipetizione;
        if (tm_epoch.tm_mon > 11)
        {
            tm_epoch.tm_mon -= 12;
            tm_epoch.tm_year += 1;
        }
        epochOut = mktime(&tm_epoch);
        break;
    case 365:
        tm tm_epochY;
        gmtime_r(&epoch, &tm_epochY);
        tm_epochY.tm_year += intervalloRipetizione;
        epochOut = mktime(&tm_epochY);
        break;
    }
    return epochOut;
}

void addEventToEntry(EventClass* dstEntry, time_t epochFirstDayShown, time_t epochEvtFrom, time_t epochEvtTo, bool allDay, char* summary, char* location, char* beginEvent, char* endEvent)
{
    //  ====   DATA   ====
    // Creo la data a partire dall'epoch
    struct tm eventFrom, eventTo;
    gmtime_r(&epochEvtFrom, &eventFrom);
    gmtime_r(&epochEvtTo, &eventTo);
    // Verifico se l'evento dura tutto il giorno
    if (!eventFrom.tm_hour && !eventFrom.tm_min &&
        eventTo.tm_hour == 23 && eventTo.tm_min == 59)
    {
        strncpy(dstEntry->time, "All-day", 7);
    }
    else
    {
        // Scrivo l'ora di inizio in dstEntry->time.
        strncpy(dstEntry->time, asctime(&eventFrom) + 11, 5);
        dstEntry->time[5] = ' ';
        dstEntry->time[6] = '-';
        dstEntry->time[7] = ' ';
        // Scrivo l'ora di fine in dstEntry->time
        strncpy(dstEntry->time + 8, asctime(&eventTo) + 11, 5);
    }
    dstEntry->time[13] = 0; // indico la fine della stringa

    //  ====   SUMMARY   ====
    // Parso il titolo dell'evento
    if (summary > beginEvent && summary < endEvent)
    {
        int lengthSummary = strchr(summary, '\n') - summary;
        if (lengthSummary > EventClass::MAX_N_CHAR_TITLE_CALENDAR)
            lengthSummary = EventClass::MAX_N_CHAR_TITLE_CALENDAR;
        strncpy(dstEntry->name, summary, lengthSummary);
        correggiApostrofo(dstEntry->name, lengthSummary);
        correggiCarriageReturn(dstEntry->name, lengthSummary);
        // Se il titolo è troppo lungo, lo tronco e aggiungo ... alla fine
        if (lengthSummary == EventClass::MAX_N_CHAR_TITLE_CALENDAR)
        {
            dstEntry->name[lengthSummary - 2] = '.';
            dstEntry->name[lengthSummary - 3] = '.';
            dstEntry->name[lengthSummary - 4] = '.';
        }
        dstEntry->name[lengthSummary - 1] = 0;
    }

    //  ====   LOCATION   ====
    if (location > beginEvent && location < endEvent)
    {
        int lengthLocation = strchr(location, '\n') - location;
        strncpy(dstEntry->location, location, lengthLocation);
        dstEntry->location[lengthLocation] = 0;
        correggiApostrofo(dstEntry->location, lengthLocation);
        correggiCarriageReturn(dstEntry->location, lengthLocation);
    }

    // Assegno il valore Timestamp
    dstEntry->timeStamp = epochEvtFrom;

    // Definisco il giorno di inizio evento
    dstEntry->day = (int)((float)(epochEvtFrom - epochFirstDayShown) / 24 / 3600);
    Serial.printf("epochFrom: %d , epochFirstDayShown: %d, day: %d\n", dstEntry->timeStamp, epochFirstDayShown, dstEntry->day);
}

#pragma endregion
