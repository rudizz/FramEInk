// EventClass.h

#ifndef _EVENTCLASS_h
#define _EVENTCLASS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

class EventClass
{
	public:
		const static int MAX_CALENDAR_EVENTS = 50;
		const static int MAX_N_CHAR_TITLE_CALENDAR = 65;
		// Calendar text is decoded to single-byte CalendarText display glyphs.
		// Do not print these fields with an ASCII-only font or treat them as UTF-8.
		char name[MAX_N_CHAR_TITLE_CALENDAR];
		char time[14];
		char location[64];
		int day = -1;
		int timeStamp;

};


#endif

