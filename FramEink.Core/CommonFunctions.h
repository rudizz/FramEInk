#include "Arduino.h"
#include "EventClass.h"
#include <cstring>
#include <ctime>

#ifndef COMMON_FUNCTIONS_H
#define COMMON_FUNCTIONS_H

#pragma region CONST

// Delay between API calls. Converts time_sleeping in minutes.
#define MIN_2_MICROSEC 60000000ll
// Convert Days to Seconds
#define DAYS_2_SEC 24 * 3600ll

#pragma endregion

//struct entry
//{
//    char name[MAX_N_CHAR_TITLE_CALENDAR];
//    char time[14];
//    char location[64];
//    int day = -1;
//    int timeStamp;
//};

#pragma region Functions

// Verifico se il pattern di una stringa è presente all'inizio della str1.
bool stringContain(char* str1, const char* pattern);


/// <summary>
/// Se non sono passati almeno 'minutes' dall'ultimo risveglio
/// applico il deep sleep per 'minutes'
/// </summary>
/// <param name="minutes">Minuti minimi tra due risvegli</param>
time_t checkLastAwake(long long minutes, time_t nowEpoch);

void correggiApostrofo(char* text, size_t lengthText);
void correggiCarriageReturn(char* text, size_t lengthText);

time_t getEpoch(char* dateString);

time_t resetEpochOf(time_t epochToReset, bool year, bool month, bool monthDay, bool hour, bool minute, bool second);

int parseCalendarEvents(EventClass entries[], char* beginEvt, char* endCal, int timeZone, time_t epochFirstDayShown, time_t epochLastDayShown);
time_t aggiungiEpochRipetizione(time_t epoch, uint16_t giorniFrequenzaRipetizione, size_t intervalloRipetizione);
void addEventToEntry(EventClass* dstEntry, time_t epochFirstDayShown, time_t epochEvtFrom, time_t epochEvtTo, bool allDay, char* summary, char* location, char* beginEvent, char* endEvent);

#pragma endregion
#endif