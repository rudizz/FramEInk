/*
 Name:		FramEink_6Color.ino
 Created:	10/16/2022 9:17:34 PM
 Author:	rudipirani
*/
/*
   3-Google_calendar_example for e-radionica.com Inkplate 10
   For this example you will need only USB cable and Inkplate 10.
   Select "Inkplate 10(ESP32)" from Tools -> Board menu.
   Don't have "Inkplate 10(ESP32)" option? Follow our tutorial and add it:
   https://e-radionica.com/en/blog/add-inkplate-6-to-arduino-ide/

   This project shows you how Inkplate 10 can be used to display
   events in your Google Calendar using their provided API

   For this to work you need to change your timezone, wifi credentials and your private calendar url
   which you can find following these steps:

    1. Open your google calendar
    2. Click the 3 menu dots of the calendar you want to access at the bottom of left hand side
    3. Click 'Settings and sharing'
    4. Navigate to 'Integrate Calendar'
    5. Take the 'Secret address in iCal format'

   (https://support.google.com/calendar/thread/2408874?hl=en)

   Want to learn more about Inkplate? Visit www.inkplate.io
   Looking to get support? Write on our forums: http://forum.e-radionica.com/en/
   11 February 2021 by e-radionica.com
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#ifndef ARDUINO_INKPLATECOLOR
#error "Wrong board s-election for this example, please select Inkplate 10 in the boards menu."
#endif

// Include Inkplate library to the sketch
#include "Inkplate.h"
#include "SDPhoto.h"

#include "fonts.h"
#include "icons_color.h"

#include "Network.h"
#include <algorithm>
#include <ctime>
#include <uICAL.h>

#include "WiFiAccessPoint/WiFiAPSettings.h"
//#include "WiFiAccessPoint/HTMLSettingsPage.h"

// WiFi Credentials ---------------
String ssid;
String pass;
// CALENDAR -----------------
String calendarURL;
//String calendarURL = "https://calendar.google.com/calendar/ical/e9jkn0qjeetjm2mkkvhubtlvrk%40group.calendar.google.com/private-7a399955cd5efd535cac7599422df2b0/basic.ics";

// Indica l'offset in secondi da applicare alle date, relativo al timeZone della location Meteo impostata.
int timeZone = 0;

// Set to 3 to flip the screen 180 degrees
#define ROTATION 0
#define ROWS 1
#define COLUMNS 3
//---------------------------
int WIDTH, HEIGHT;
int HEIGHT_PHOTO = 0;
int cellWidth, cellHeight;
// header size for calendar name and current Time
int headerCalendarName = 0;
// header height, for day info
int headerDay = 36;
// margini della tabella
const int marginLeft = 1;
const int marginRight = 2;
const int marginUp = 1;
const int marginDown = 2;
const uint16_t colorGrid = INKPLATE_GREEN;
const float thickLineGrid = 2.0;
const uint16_t colorCalendarTitle = INKPLATE_BLUE;
const uint16_t colorCalendarTime = INKPLATE_RED;
const uint16_t colorCalendarLocation = INKPLATE_GREEN;

// WiFi AP Settings
WiFiAPSettingsClass* wiFiAPSettings = NULL;
RTC_DATA_ATTR bool settingsOK = false; // viene messa a true dopo che vengono settati i parametri, 
                                       // così non compare più la pagina dei Settings

// WEATHER  -----------------
// City filled by query
char city[128] = "";
#define icon_height 88
// height of the weather strip
int headerWeather = 40; // qui verrà aggiunto icon_height
const uint16_t colorDayTitle = INKPLATE_BLACK;
const uint16_t colorWeatherTempMax = INKPLATE_RED;
const uint16_t colorWeatherTempMin = INKPLATE_BLUE;
const uint16_t colorWeatherName = INKPLATE_BLACK;

// Constants used for drawing icons
char abbrs[9][4] = { "01d", "02d", "03d", "04d", "09d", "10d", "11d", "13d", "50d" };
const uint8_t *color_icons[9] = { color_01d_clear, color_02d_few_clouds, color_03d_scattered_clouds,
                                    color_04d_broken_clouds, color_09d_shower_rain, color_10d_rain,
                                    color_11d_thunderstorm, color_13d_snow, color_50d_mist };
const int color_icons_width[9] = { color_01d_clear_w, color_02d_few_clouds_w, color_03d_scattered_clouds_w,
                                    color_04d_broken_clouds_w, color_09d_shower_rain_w, color_10d_rain_w,
                                    color_11d_thunderstorm_w, color_13d_snow_w, color_50d_mist_w };
const int color_icons_height[9] = { color_01d_clear_h, color_02d_few_clouds_h, color_03d_scattered_clouds_h,
                                    color_04d_broken_clouds_h, color_09d_shower_rain_h, color_10d_rain_h,
                                    color_11d_thunderstorm_h, color_13d_snow_h, color_50d_mist_h };
const uint8_t* moon_phases_1bit[27] = {moon_phase_0, moon_phase_0_04, moon_phase_0_08, moon_phase_0_11, moon_phase_0_15,
                                    moon_phase_0_18, moon_phase_0_21, moon_phase_0_25, moon_phase_0_29, moon_phase_0_33,
                                    moon_phase_0_36, moon_phase_0_40, moon_phase_0_43, moon_phase_0_47, moon_phase_0_50,
                                    moon_phase_0_54, moon_phase_0_58, moon_phase_0_61, moon_phase_0_65, moon_phase_0_69,
                                    moon_phase_0_73, moon_phase_0_77, moon_phase_0_81, moon_phase_0_85, moon_phase_0_89,
                                    moon_phase_0_93, moon_phase_0_96 };
const int moon_phase_icon_size = moon_phase_0_w;
float moon_phase[6] = { 0, 0, 0, 0, 0, 0};
RTC_DATA_ATTR char abbr_days[6][16];

// Variables for storing temperature
RTC_DATA_ATTR char temps_min[8][6] = {
    "0F",
    "0F",
    "0F",
    "0F",
    "0F",
    "0F",
};
RTC_DATA_ATTR char temps_max[8][6] = {
    "0F",
    "0F",
    "0F",
    "0F",
    "0F",
    "0F",
};
RTC_DATA_ATTR char predictability[8][6] = {
    "0F",
    "0F",
    "0F",
    "0F",
    "0F",
    "0F",
};
// Variables for storing days of the week
RTC_DATA_ATTR char days[8][4] = {
    "",
    "",
    "",
    "",
};
// Variables for storing current time and weather info
RTC_DATA_ATTR char currentTemp[16] = "0F";
RTC_DATA_ATTR char currentWind[16] = "0m/s";

RTC_DATA_ATTR char currentTime[16] = "9:41";

RTC_DATA_ATTR char nameWeather[6][32] = {
  "-",
  "-",
  "-",
  "-",
  "-",
  "-",
};


// Go to sleep before checking again
const long long time_sleeping = 60ll; // [minutes] 180' = 3h sleep
// Delay between API calls. Converts time_sleeping in minutes.
#define MIN_2_MICROSEC 60000000ll
// Convert Days to Seconds
#define DAYS_2_SEC 24 * 3600ll

// Initiate out Inkplate object
Inkplate display;
// SDPhoto Object
SDPhotoClass* sdPhoto = NULL;
RTC_DATA_ATTR uint counterPortrait = 0;
RTC_DATA_ATTR uint counterLandscape = 0;
RTC_DATA_ATTR bool stateCalendar = true; // true: Calendar+landscapePhoto ; false: portraitPhoto

// Our networking functions, see Network.cpp for info
Network network;

// Variables for time and raw event info
char date[64];
char* data;

// Struct for storing calender event info
const int MAX_N_CHAR_TITLE_CALENDAR = 65;
struct entry
{
    char name[MAX_N_CHAR_TITLE_CALENDAR];
    char time[14];
    char location[64];
    int day = -1;
    int timeStamp;
};

// Here we store calendar entries
int entriesNum = 0;
const int MAX_CALENDAR_EVENTS = 50;
entry entries[MAX_CALENDAR_EVENTS];

// All our functions declared below setup and loop
void drawInfo();
void drawTime();
void drawBattery();
void drawGrid();
//bool getToFrom(char* dst, char* from, char* to, int* day, int* timeStamp, bool correctTimeZone);
bool drawEvent(entry* event, int day, int beginY, int maxHeigth, int* heigthNeeded);
int cmp(const void* a, const void* b);
void drawCalendarData();
void drawWeatherIcon(int beginX, int beginY, int heightIcon);
void drawWeatherStrip();
bool stringContain(char* str1, const char* pattern);

void setup()
{
    Serial.begin(115200);
    Serial.println("Inizializzo...");

    // calcolo le variabili iniziali in base all'orientamento
    if (ROTATION == 0 | ROTATION == 2)
    {
        WIDTH = display.width(); // 600
        HEIGHT = display.height(); // 448
    }
    else {
        WIDTH = display.height();
        HEIGHT = display.width();
    }
    cellWidth = (WIDTH - marginLeft - marginRight) / COLUMNS;
    cellHeight = (HEIGHT - HEIGHT_PHOTO - headerCalendarName - marginUp - marginDown) / ROWS;

    headerWeather += icon_height;

    // Initial display settings
    display.begin();

    display.setRotation(ROTATION);
    display.setTextWrap(false);

    settingsOK = true; // debug
    delay(10000); // debug
    wiFiAPSettings = new WiFiAPSettingsClass();
    if (!settingsOK)
    {
        wiFiAPSettings->initAP();
        drawSettings();
        wiFiAPSettings->loop(); // esco dopo 10 minuti
        settingsOK = true;
    }
    ssid = wiFiAPSettings->SSID_User;
    pass = wiFiAPSettings->PWD_User;
    network.latitude = atoff(wiFiAPSettings->Latitude_User.c_str());
    network.longitude = atoff(wiFiAPSettings->Longitude_User.c_str());
    calendarURL = wiFiAPSettings->ICALID_User;

    display.setTextColor(0, 7);

    sdPhoto = new SDPhotoClass(&display);

    //Serial.print("ssid: ");
    //Serial.println(ssid);
    //Serial.print("pass: ");
    //Serial.println(pass);
    //Serial.print("calendarURL: ");
    //Serial.println(calendarURL);
    //Serial.printf("sd init: %d\n", sdPhoto->initOk);
    //Serial.printf("display sd init: %d\n", display.sdCardInit());

    //if (ssid == "" || pass == "")
    //{
    //    // Se non sono state impostate le credenziali del WiFi,
    //    // non faccio comparire la pagina con Meteo e Calendario
    //    stateCalendar = false;
    //}
    //else if (!sdPhoto->initOk)
    //{
    //    stateCalendar = true;
    //}
    
    //ssid = "Baghini";
    //pass = "lastanzadeibottoni";
    //calendarURL = "https://calendar.google.com/calendar/ical/e9jkn0qjeetjm2mkkvhubtlvrk%40group.calendar.google.com/private-7a399955cd5efd535cac7599422df2b0/basic.ics";
    
    if (!stateCalendar)
    {
        // Photo
        counterLandscape = -1; // attivo il random mode
        display.clearDisplay();
        sdPhoto->drawImageFromSD(0, 0, SDPhotoClass::PhotoOrientation::landscape, counterLandscape);
    }
    else {

        network.begin(); // stampo su seriale la data e ora attuale
        // ---  WEATHER  ---
        network.getDaysLabel(days[0], days[1], days[2], days[3]);
        network.getDataFromMetaWeather(&timeZone, temps_min[0], temps_min[1], temps_min[2], temps_min[3], temps_min[4], temps_min[5], currentTemp,
            temps_max[0], temps_max[1], temps_max[2], temps_max[3], temps_max[4], temps_max[5],
            predictability[0], predictability[1], predictability[2], predictability[3], predictability[4], predictability[5],
            currentWind, currentTime, nameWeather[0], nameWeather[1], nameWeather[2], nameWeather[3], nameWeather[4], nameWeather[5],
            abbr_days[0], abbr_days[1], abbr_days[2], abbr_days[3], abbr_days[4], abbr_days[5],
            &moon_phase[0], &moon_phase[1], &moon_phase[2], &moon_phase[3], &moon_phase[4], &moon_phase[5]);

        // ---  CALENDAR  ---
        if (calendarURL != "") {
            data = (char*)ps_malloc(1000000L); // alloco 1Mb su RAM extra del ESP32
            // Keep trying to get data if it fails the first time
            while (!network.getDataCalendar(data))
            {
                Serial.println("Failed getting data, retrying");
                delay(1000);
            }
        }
        //Serial.println(data); // stampo i dati grezzi del calendario
        // Drawing all data, functions for that are above
        display.clearDisplay();

        //drawInfo();
        //drawBattery();
        drawGrid();

        if (calendarURL != "")
            drawCalendarData();
        //drawTime();

        drawWeatherStrip();

    }
    // Toggle the state of the InkPlate
    stateCalendar = !stateCalendar;
    delay(1000);

    // Can't do partial due to deepsleep
    display.display();

    Serial.printf("counterPortrait: %d, counterLandscape: %d\n", counterPortrait, counterLandscape);

    // Se si sveglia tra le 23:00 e l' 1:59, imposto a 6 ore il tempo di sleep
    //int timeHour;
    //network.getTimeHour(&timeHour, 0);
    //if (timeHour >= 23 || timeHour < 2)
    //{
    //    //time_sleeping = 360ll; // 6h sleep
    //}
    Serial.print("Sleep time [us]: ");
    Serial.println(time_sleeping * MIN_2_MICROSEC);
    esp_sleep_enable_timer_wakeup(time_sleeping * MIN_2_MICROSEC);
    (void)esp_deep_sleep_start();
}

void loop()
{
    // Never here
}

// Function for drawing calendar info
void drawInfo()
{
    // Setting font and color
    display.setTextColor(0, 7);
    display.setFont(&FreeSans12pt7b);
    display.setTextSize(1);

    display.setCursor(marginLeft + 20, marginUp + 22);

    // Find email in raw data
    char temp[64];
    char* start = strstr(data, "X-WR-CALNAME:");

    // If not found return
    if (!start)
        return;

    // Find where it ends
    start += 13;
    char* end = strchr(start, '\n');

    strncpy(temp, start, end - start - 1);
    temp[end - start - 1] = 0;

    // Print it
    display.println(temp);
}

// Drawing what time it is
void drawTime()
{
    // Initial text settings
    display.setTextColor(0, 7);
    display.setFont(&FreeSans12pt7b);
    display.setTextSize(1);

    display.setCursor(marginLeft + 500, marginUp + 22);

    // Our function to get time
    network.getTime(date);

    int t = date[16];
    date[16] = 0;
    display.println(date);
    date[16] = t;
}

void drawBattery()
{
    // Initial text settings
    display.setTextColor(0, 7);
    display.setFont(&FreeSans12pt7b);
    display.setTextSize(1);

    display.setCursor(WIDTH - marginRight - 50, marginUp + 22);

    display.println(display.readBattery());
    //  double voltage = display.readBattery();

}

// Draw lines in which to put events
void drawGrid()
{
    // upper left and low right coordinates
    int x1 = marginLeft, y1 = headerCalendarName + marginUp;
    int x2 = WIDTH - marginRight, y2 = HEIGHT - HEIGHT_PHOTO - marginDown;

    // Columns and rows
    int n = ROWS, m = COLUMNS;
    for (size_t row = 0; row < headerDay; row+=yellow_white_h)
    {
        for (size_t col = 0; col < x2; col+=yellow_white_w)
        {
            display.drawBitmap3Bit(x1 + col, y1 + row, yellow_white, yellow_white_w, yellow_white_h);
        }
    }
    //display.drawBitmap3Bit(x1, y1, yellow_white, x2 - x1, 36);
    display.fillRect(x1, headerDay, x2 - x1, 3, INKPLATE_YELLOW);

    // Line drawing
//    display.drawThickLine(x1, y1 + headerDay, x2, y1 + headerDay, colorGrid, thickLineGrid);
    for (int i = 0; i < n + 1; ++i)
    {
        // Riga orizzontale sopra header
        display.drawThickLine(x1, (int)((float)y1 + (float)i * (float)(y2 - y1) / (float)n), x2,
            (int)((float)y1 + (float)i * (float)(y2 - y1) / (float)n), colorGrid, thickLineGrid);
        // Riga orizzontale sotto header
  //        display.drawThickLine(x1, (int)((float)y1 + headerDay + (float)i * (float)(y2 - y1) / (float)n), x2,
  //                              (int)((float)y1 + headerDay + (float)i * (float)(y2 - y1) / (float)n), colorGrid+4, thickLineGrid);
    }
    for (int i = 0; i < m + 1; ++i)
    {
        // Righe verticali
        display.drawThickLine((int)((float)x1 + (float)i * (float)(x2 - x1) / (float)m), y1,
            (int)((float)x1 + (float)i * (float)(x2 - x1) / (float)m), y2, colorGrid, thickLineGrid);
    }
    // Stampo la scritta dei giorni del calendario
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(colorDayTitle);
    char temp[32];
    for (int i = 0; i < m * n; ++i)
    {
        // Display day info using time offset
        network.getTime(temp, i * 3600L * 24);
        temp[10] = 0;

        display.setCursor(40 + (int)((float)x1 + (float)(i % m) * (float)(x2 - x1) / (float)m), y1 + headerDay - 9 + (int)(i / m) * (float)(y2 - y1) / (float)n);
        display.println(temp);
    }
}

void addEventToEntry(entry* dstEntry, time_t epochFirstDayShown,
                     time_t epochEvtFrom, time_t epochEvtTo, bool allDay,
                     char* summary, char* location, char* endEvent)
{
    //  ====   DATA   ====
    // Creo la data a partire dall'epoch
    struct tm eventFrom, eventTo;
    gmtime_r(&epochEvtFrom, &eventFrom);
    gmtime_r(&epochEvtTo, &eventTo);
    // Se non è presente il tempo dopo la data, vuol dire che l'evento dura 'All-day'.
    //char allDay[1];
    //strncpy(allDay, epochEvtFrom +1, sizeof(char));
    if (allDay)
    {
        strncpy(dstEntry->time, "All-day", 7);
    }
    else {
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
    if (summary && summary < endEvent)
    {
        int lengthSummary = strchr(summary, '\n') - summary;
        if (lengthSummary > MAX_N_CHAR_TITLE_CALENDAR)
            lengthSummary = MAX_N_CHAR_TITLE_CALENDAR;
        strncpy(dstEntry->name, summary, lengthSummary);
        correggiApostrofo(dstEntry->name, lengthSummary);
        correggiCarriageReturn(dstEntry->name, lengthSummary);
        // Se il titolo è troppo lungo, lo tronco e aggiungo ... alla fine
        if (lengthSummary == MAX_N_CHAR_TITLE_CALENDAR)
        {
            dstEntry->name[lengthSummary - 2] = '.';
            dstEntry->name[lengthSummary - 3] = '.';
            dstEntry->name[lengthSummary - 4] = '.';
        }
        dstEntry->name[lengthSummary - 1] = 0;
    }

    //  ====   LOCATION   ====
    if (location && location < endEvent)
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

// Function to draw event
bool drawEvent(entry* event, int day, int beginY, int maxHeigth, int* heigthNeeded)
{
    // Upper left coordinates
    int padLeftRight = 6; // margin from grid to event box
    int xLeftBox = marginLeft + cellWidth * (day % COLUMNS) + padLeftRight;
    int xRightBox = xLeftBox + cellWidth - 2 * padLeftRight;
    int yUpBox = beginY + 3;
    int marginTextEvent = 5; // margine dx e sx del titolo dell'evento
    int xStartTitle = xLeftBox + marginTextEvent;
    int interlinea = -5; // nel titolo dell'evento indica la distanza tra una riga e la successiva

    // Setting text font
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(colorCalendarTitle);
    //    // Draw background // lascio commentato, risulta troppo scuro
    //    display.fillRect(x1, y1, cellWidth - 2 * padLeftRight, 110, 6);

        // Some temporary variables
    int n = 0;
    char line[128];

    // Insert line brakes into setTextColor
    int lastSpace = -100;
    display.setCursor(xStartTitle, beginY + 33); // offset tra la linea alta della griglia e l'inizio del testo
    for (int i = 0; i < min(MAX_N_CHAR_TITLE_CALENDAR, (int)strlen(event->name)); ++i)
    {
        // Se il primo carattere è uno spazio, lo salto
        if (n == 0 && event->name[i] == ' ')
            i++;

        // Copy name letter by letter and check if it overflows space given
        line[n] = event->name[i];
        if (line[n] == ' ')
            lastSpace = n;
        line[++n] = 0;

        int16_t xt1, yt1;
        uint16_t w, h;

        // Gets text bounds
        display.getTextBounds(line, 0, 0, &xt1, &yt1, &w, &h);

        // Scrivo linea per linea in modo che posso usare l'interlinea
        // Char out of bounds, put in next line
        if (w > xRightBox - xLeftBox - 2 * marginTextEvent)
        {
            // Ho superato la dimensione massima, quindi torno indietro di un carattere e stampo la stringa
            i--;
            line[--n] = 0;

            // if there was a space 5 chars before, break line there
            if (n - lastSpace < 5)
            {
                i -= n - lastSpace - 1;
                line[lastSpace] = 0;
            }

            // Print text line
            display.setCursor(xStartTitle, display.getCursorY() + interlinea);
            display.println(line);

            // Clears line (null termination on first charachter)
            line[0] = 0;
            n = 0;
        }
    }

    // display last line of text
    // Può capitare che vado a capo e la prima riga è il terminatore, in quel caso non stampo niente.
    if (line[0] != 0)
    {
        display.setCursor(xStartTitle, display.getCursorY() + interlinea);
        display.println(line);
    }

    // Print time
    // Set cursor on same x but change y
    display.setCursor(xStartTitle, display.getCursorY() - 7);
    display.setFont(&FreeSansBold9pt7b);

    // if theres a location print it
    if (strlen(event->location) > 1)
    {
        display.setTextColor(colorCalendarTime);
        display.println(event->time);

        display.setCursor(xStartTitle, display.getCursorY() - 3);

        char line[128] = { 0 };
        display.setTextColor(colorCalendarLocation);

        for (int i = 0; i < strlen(event->location); ++i)
        {
            line[i] = event->location[i];
            line[i + 1] = 0;

            int16_t xt1, yt1;
            uint16_t w, h;

            // Gets text bounds
            display.getTextBounds(line, 0, 0, &xt1, &yt1, &w, &h);

            if (w > xRightBox - xLeftBox - 2 * marginTextEvent)
            {
                for (int j = i - 1; j > max(-1, i - 4); --j)
                    line[j] = '.';
                line[i] = 0;
            }
        }

        display.print(line);
    }
    else
    {
        display.setTextColor(colorCalendarTime);
        display.print(event->time);
    }

    int yDownBox = display.getCursorY() + 7;

    // Draw event rect bounds
    display.drawThickLine(xLeftBox, yUpBox, xLeftBox, yDownBox, 3, thickLineGrid);
    display.drawThickLine(xLeftBox, yDownBox, xRightBox, yDownBox, 3, thickLineGrid);
    display.drawThickLine(xRightBox, yDownBox, xRightBox, yUpBox, 3, thickLineGrid);
    display.drawThickLine(xRightBox, yUpBox, xLeftBox, yUpBox, 3, thickLineGrid);

    // Set how high is the event
    *heigthNeeded = display.getCursorY() + 12 - yUpBox;

    // Return is it overflowing
    return display.getCursorY() < maxHeigth - 100; // ipotizzo che il prossimo evento sia alto 100 pixel
    // se rimangono più di 100 pixel, il prossimo evento viene mostrato, se è più alto di 100 px vengono mostrati
    // solo i primi 100 px (3 righe)
}

// Struct event comparison function, by timestamp, used for qsort later on
int cmp(const void* a, const void* b)
{
    entry* entryA = (entry*)a;
    entry* entryB = (entry*)b;

    return (entryA->timeStamp - entryB->timeStamp);
}

// Verifico se il pattern di una stringa è presente all'inizio della str1.
bool stringContain(char* str1, const char* pattern)
{
    return strspn(str1, pattern) == strlen(pattern);
}

// Main data drawing data
void drawCalendarData()
{
    char* beginEvt = data + 1;
    char* endCal = data + strlen(data)-1;
    //Serial.println("Cal0");
    
    //uICAL::istream_String istm(data);
    //Serial.println("Cal0b");
    //auto cal = uICAL::Calendar::load(istm);
    //Serial.println("Cal1");
    //uICAL::DateTime begin("20221029T102000Z");
    //uICAL::DateTime end("20221101T103000Z");
    //Serial.println("Cal2");

    //auto calIt = uICAL::new_ptr<uICAL::CalendarIter>(cal, begin, end);
    //Serial.println("Cal3");
    //while (calIt->next()) {
    //    Serial.println("Cal4");

    //    uICAL::CalendarEntry_ptr entry = calIt->current();
    //    Serial.println("Cal5");
    //    Serial.println(entry.get()->summary().c_str());
    //}

    // Parso gli ultimi 70k caratteri. Oltre sono troppo vecchi.
    /*if (n > 70000)
        n = 70000;*/

    // reset count
    entriesNum = 0;

    // Search raw data for events
    while (entriesNum < MAX_CALENDAR_EVENTS && beginEvt < endCal && strstr(beginEvt, "BEGIN:VEVENT"))
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

        // //Gestisco gli eventi ripetuti nel tempo. Per ora solo Daily e Weekly.
        char *rRule = strstr(beginEvt, "RRULE:") + 6;
        uint32_t giorniFrequenzaRipetizione = 0;
        if (rRule && rRule > beginEvt && rRule < endEvt && rRule < endCal)
        {
          if (stringContain(rRule, "FREQ=DAILY"))
          {
            giorniFrequenzaRipetizione = 1;
          }
          else if (stringContain(rRule, "FREQ=WEEKLY"))
          {
              giorniFrequenzaRipetizione = 7;
          }
          else if (stringContain(rRule, "FREQ=MONTHLY"))
          {
              giorniFrequenzaRipetizione = 30;
          }
          else if (stringContain(rRule, "FREQ=YEARLY"))
          {
              giorniFrequenzaRipetizione = 365;
          }
            Serial.printf("Frequenza: %d\n", giorniFrequenzaRipetizione);
        }
        // Sequenza
//        char *sequence = strstr(data + i, "SEQUENCE:") + 9;
//        Serial.print("Sequence: ");
//        Serial.println(sequence);

//        for (int seq = 0; seq <= atoi(sequence)

        // Porto il begin un carattere dopo l'end
        beginEvt = endEvt + 11;

        // --------  Controllo se l'evento è da visualizzare  -----------
        time_t epochFrom = getEpoch(timeStart);
        time_t epochTo = getEpoch(timeEnd);

        // In alcuni casi viene passato l'orario già corretto del TimeZone, quindi non devo modificarlo.
        if (correggiTimeZone)
        {
            epochFrom += (time_t)timeZone;
            epochTo += (time_t)timeZone;
        }
        // Creo le epoch della finestra di inizio e fine giorni da visualizzare.
        // Finestra è dalle 00:00 del primo giorno alle 23:59 dell'ultimo giorno.
        time_t epochFirstDayShown = network.getNowEpoch();
        //Serial.printf("epochFrom: %d\n", epochFrom);
        tm tm_firstDayShown;
        gmtime_r(&epochFirstDayShown, &tm_firstDayShown);
        tm_firstDayShown.tm_hour = 0;
        tm_firstDayShown.tm_min = 0;
        tm_firstDayShown.tm_sec = 0;
        epochFirstDayShown = mktime(&tm_firstDayShown);
        time_t epochLastDayShown = epochFirstDayShown + COLUMNS * ROWS * DAYS_2_SEC -1;

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

        do
        {
            if ((epochFrom >= epochFirstDayShown && epochFrom <= epochUntil) ||
                (epochTo >= epochFirstDayShown && epochTo <= epochUntil))
            {
                Serial.printf("epochFrom: %d , fRep: %d, lastDay: %d\n", epochFrom, giorniFrequenzaRipetizione, epochLastDayShown);
                bool excludeDate = false;
                char* exdate = strstr(timeEnd, "EXDATE") + 6;
                char* dtStamp = strstr(timeEnd, "DTSTAMP:");
                if (exdate < dtStamp)
                {
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
                            // esco dal ciclo ed non aggiungo tale evento alla visualizzazione. 
                            excludeDate = true;
                        }
                        exdate = strstr(exdate, "EXDATE") + 6;
                    } while (!excludeDate && exdate < dtStamp);
                }
                if (!excludeDate)
                {
                    addEventToEntry(&entries[entriesNum], epochFirstDayShown,
                        epochFrom, epochTo, String(timeStart + 8, 1) != "T", summary, location, endEvt);
                    Serial.printf("ADD entriesNum: %d, Summary: %s\n", entriesNum, entries[entriesNum].name);
                    entriesNum++;

                }
            }
            epochFrom = aggiungiEpochRipetizione(epochFrom, giorniFrequenzaRipetizione);
            epochTo = aggiungiEpochRipetizione(epochTo, giorniFrequenzaRipetizione);
                
        } while (giorniFrequenzaRipetizione > 0 && epochFrom <= epochUntil);

    }
    // Sort entries by timeStamp
    qsort(entries, entriesNum, sizeof(entry), cmp);
    Serial.printf("Eventi nel calendario: %d\n", entriesNum);
    //for (size_t i = 0; i < entriesNum; i++)
    //{
    //    Serial.printf("%d - Day: %d, Sum: %s, Time: %s, Loc: %s, TS: %d\n", i, entries[i].day ,entries[i].name,
    //        entries[i].time,
    //        entries[i].location,
    //        entries[i].timeStamp);
    //}
    // Events displayed and overflown counters
    int columns[COLUMNS * ROWS] = { 0 };
    bool clogged[COLUMNS * ROWS] = { 0 };
    int cloggedCount[COLUMNS * ROWS] = { 0 };

    // Displaying events one by one
    for (int i = 0; i < entriesNum; ++i)
    {
        //Serial.println(entries[i].day);
        // If column overflowed just add event to not shown
        if (entries[i].day != -1 && clogged[entries[i].day])
            ++cloggedCount[entries[i].day];
        if (entries[i].day == -1 || clogged[entries[i].day])
            continue;

        // We store how much height did one event take up
        int shift = 0;
        int padDown = 5;
        int padUp = 10;
        bool s = drawEvent(&entries[i], entries[i].day, columns[entries[i].day] + (entries[i].day / COLUMNS) * cellHeight + marginUp + headerCalendarName + headerDay + headerWeather + padUp,
            (entries[i].day / COLUMNS) * cellHeight + marginUp + headerCalendarName + cellHeight, &shift);

        //Serial.printf("Shift height event: %d\n", shift);
        columns[entries[i].day] += shift + padDown;

        // If it overflowed, set column to clogged and add one event as not shown
        if (!s)
        {
            Serial.println("Clogged event");
            //            ++cloggedCount[entries[i].day];
            clogged[entries[i].day] = 1;
        }
    }

    // Display not shown events info
    for (int i = 0; i < COLUMNS * ROWS; ++i)
    {
        if (clogged[i] && cloggedCount[i] > 0)
        {
            // Draw notification showing that there are more events than drawn ones
            int marginMoreEvent = 6;
            int xBegin = marginLeft + cellWidth * (i % COLUMNS) + marginMoreEvent;
            int yBegin = marginUp + headerCalendarName + (i / COLUMNS) * cellHeight + cellHeight - 23;
            display.fillRoundRect(xBegin, yBegin, cellWidth - 2 * marginMoreEvent, 20, 10, INKPLATE_YELLOW);
            display.drawRoundRect(xBegin, yBegin, cellWidth - 2 * marginMoreEvent, 20, 10, INKPLATE_ORANGE);
            display.setCursor(15 + xBegin, yBegin + 15);
            //            display.setTextColor(7, 0);
            display.setTextColor(INKPLATE_BLACK, INKPLATE_WHITE);
            display.setFont(&FreeSansBold9pt7b);
            display.print(cloggedCount[i]);
            display.print(" more event");
            // Se c'è più di un evento, metto il plurale ad events
            if (cloggedCount[i] > 1)
            {
                display.print("s");
            }
        }
    }
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
    for (size_t i = 0; i < lengthText-1; i++)
    {
        if (text[i] == '\\' && text[i+1] == 'n') {
            text[i++] = ' ';
            text[i] = ' ';
        }
    }
}

time_t aggiungiEpochRipetizione(time_t epoch, uint16_t giorniFrequenzaRipetizione)
{
    time_t epochOut;
    switch (giorniFrequenzaRipetizione)
    {
        case 1:
        case 7:
            epochOut =  epoch + giorniFrequenzaRipetizione * DAYS_2_SEC;
            break;
        case 30:
            tm tm_epoch;
            gmtime_r(&epoch, &tm_epoch);
            if (tm_epoch.tm_mon == 12)
            {
                tm_epoch.tm_mon = 1;
                tm_epoch.tm_year += 1;
            }
            else {
                tm_epoch.tm_mon += 1;
            }
            epochOut = mktime(&tm_epoch);
            break;
        case 365:
            tm tm_epochY;
            gmtime_r(&epoch, &tm_epochY);
            tm_epochY.tm_year += 1;
            epochOut = mktime(&tm_epochY);
            break;
    }
    return epochOut;
}

// ====  WEATHER drawing  ====
// ICON
void drawWeatherIcon(int beginX, int beginY, int day)
{
    // Icon
    for (int i = 0; i < 11; ++i)
    {
        // If found draw specified icon
        if (strcmp(abbr_days[day], abbrs[i]) == 0)
            display.drawBitmap3Bit(beginX, beginY, color_icons[i], color_icons_width[i], color_icons_height[i]);
    }
}
// weather info label
void drawWeatherLabel(int x, int y, int day)
{
    //    // Searching for weather state abbreviation
    //    for (int i = 0; i < 10; ++i)
    //    {
    //        // If found draw specified icon
    //        if (strcmp(abbrs[i], currentWeatherAbbr) == 0)
    //            display.drawBitmap(50, 50, logos[i], 152, 152, BLACK);
    //    }

        // Draw weather state
    display.setTextColor(colorWeatherName, INKPLATE_WHITE);
    display.setFont(&FreeSans16pt7b);
    display.setCursor(x, y);
    display.println(nameWeather[day]);

}
// TEMPERATURE
void drawWeatherTemp(int x, int y, int day)
{
    int xTermometro = x + 41;
    int xTempMax = x;
    int xTempMin = x;
     // Check temp max width
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(temps_max[day], 0, 0, &x1, &y1, &w, &h);
    if (xTempMax + w > xTermometro)
        xTempMax = xTermometro - w;
    // Check temp min width
    display.getTextBounds(temps_min[day], 0, 0, &x1, &y1, &w, &h);
    if (xTempMin + w > xTermometro)
        xTempMin = xTermometro - w;

    // Temp Max
    display.setFont(&FreeSansSerifBold14pt7b);
    display.setTextColor(colorWeatherTempMax);
    display.setCursor(xTempMax, y);
    display.print(temps_max[day]);
    /*display.setFont(&FreeSans9pt7b);
    display.println("° C");*/
    // Temp Min
    display.setFont(&FreeSansSerifBold14pt7b);
    display.setTextColor(colorWeatherTempMin);
    display.setCursor(xTempMin, y + 30);
    display.print(temps_min[day]);
    /*display.setFont(&FreeSans9pt7b);
    display.println(F("° C"));*/

    display.drawBitmap3Bit(xTermometro, y - 28, termometro, termometro_w, termometro_h);
}
// PREDICTABILITY
void drawWeatherPredictability(int x, int y, int day)
{
    // Predictability
    display.setFont(&FreeSans16pt7b);
    display.setTextColor(colorWeatherName);
    //display.setCursor(x, y);
    drawCentreString(predictability[day], x, y);
    //display.print(predictability[day]);
    display.setFont(&FreeSans9pt7b);
    display.setCursor(display.getCursorX(), display.getCursorY()-2);
    display.print("%");
}

void drawWeatherStrip()
{
    int padLeft = 15;
    int padUp = 2;
    int padDown = 5;
    for (int day = 0; day < COLUMNS * ROWS; day++)
    {
        int xBegin = marginLeft + cellWidth * (day % COLUMNS) + padLeft;
        int yBegin = marginUp + headerCalendarName + headerDay + (day / COLUMNS) * cellHeight + padUp;
        drawWeatherIcon(xBegin, yBegin + 3, day);
        drawWeatherLabel(xBegin, yBegin + headerWeather - 5, day);
        drawWeatherTemp(xBegin + icon_height + 20, yBegin + 36, day);
        // If the weather icon is with rain, print the rain probability
        if (strcmp(abbr_days[day], "09d") == 0 ||
            strcmp(abbr_days[day], "10d") == 0 ||
            strcmp(abbr_days[day], "11d") == 0 ||
            strcmp(abbr_days[day], "13d") == 0) // Snow
        {
            drawWeatherPredictability(xBegin + icon_height + 41, yBegin + headerWeather - 16, day);
        }
        else
        {
            uint8_t index_moon = (uint8_t)round(moon_phase[day] * sizeof(moon_phases_1bit) / sizeof(moon_phases_1bit[0]));
            Serial.print("index moon: ");
            Serial.println(index_moon);
            display.drawBitmap(xBegin + cellWidth - moon_phase_icon_size - padLeft - 6,
                               yBegin + headerWeather - moon_phase_icon_size,
                               moon_phases_1bit[index_moon],
                               moon_phase_icon_size, moon_phase_icon_size,
                               INKPLATE_WHITE, INKPLATE_BLACK);
        }

        // Riga orizzontale in basso. Delimita meteo da eventi calendario
        display.drawThickLine(xBegin - padLeft + thickLineGrid, yBegin + headerWeather + padDown, xBegin - padLeft + cellWidth, yBegin + headerWeather + padDown, colorGrid, thickLineGrid);
    }
}
void drawSettings()
{                              // This function updates screen with new data (text)
    display.clearDisplay();               // Clear frame buffer of display
    display.setFont(&FreeSans16pt7b);
    display.setTextWrap(false);            // If text does not fit on screen, send it to new line

    display.setTextColor(INKPLATE_BLACK);
    display.setCursor(15, 60); // Print out instruction on how to connect to Inkplate WiFi and how to open a web page
    display.print("1. Connect to ");
    display.setTextColor(INKPLATE_RED);
    display.print(wiFiAPSettings->SSID_AP_Settings);
    display.setTextColor(INKPLATE_BLACK);
    display.println(" WiFi");

    display.setCursor(15, 120);
    display.println("2. Open your web browser and");
    display.setCursor(display.getCursorX() + 60, display.getCursorY());
    display.print("go to: ");

    display.setTextColor(INKPLATE_GREEN);
    //display.setCursor(35, 140);
    display.print("http://");
    display.print(wiFiAPSettings->serverIP);
    display.println('/');
    display.println();

    display.setTextColor(INKPLATE_BLACK);
    display.setCursor(15, 220);
    display.println("3. Fill in the information and");
    display.setCursor(display.getCursorX() + 60, display.getCursorY());
    display.print("press ");
    display.setTextColor(INKPLATE_ORANGE);
    display.print("Send to FramEInk! ");

    //display.fillRect(10, 240, 780, 4, INKPLATE_YELLOW);
    
    display.setTextColor(INKPLATE_RED);
    display.setCursor(35, 300);
    drawCentreString("This page will remain", WIDTH / 2, 320);
    display.println("");
    //fprintf("active for %s minutes", wiFiAPSettings->settingDuration);
    drawCentreString((String("active for ") + String(wiFiAPSettings->settingDuration) + String (" minutes")).c_str(), WIDTH / 2, display.getCursorY());
    
    // Draw rounded orange rect
    uint8_t thick = 5;
    for (size_t i = 0; i < thick; i++)
    {
        display.drawRoundRect(55+i, 280+i, WIDTH - 1 - 110 - 2*i, display.getCursorY() - 280 + 30 - 2*i, 10, INKPLATE_ORANGE);
    }

    // Draw green rect
    for (size_t i = 0; i < thick; i++)
    {
        display.drawRect(i, i, WIDTH - 1 - 2 * i, HEIGHT - 1 - 2 * i, INKPLATE_GREEN);
    }

    display.display(); // Send everything to screen (refresh the screen)
}

void drawCentreString(const char* text, int x, int y)
{
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h); //calc width of new string

    //Serial.println(x);
    //Serial.println(y);
    //Serial.println(w);
    //Serial.println(h);
    //Serial.println("");
    display.setCursor(x - (w / 2), y + (h / 2));
    display.print(text);
}

