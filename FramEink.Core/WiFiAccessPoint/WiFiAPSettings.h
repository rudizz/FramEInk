// WiFiAPSettings.h

#ifndef _WIFIAPSETTINGS_h
#define _WIFIAPSETTINGS_h

#include "arduino.h"
//#include <Inkplate.h>   //Include Inkplate library to the sketch

#include <WebServer.h>  //Include ESP32 library for Web server
#include <WiFi.h>       //Include ESP32 WiFi library
#include <WiFiClient.h> //Include ESP32 WiFi library for AP
#include <uri/UriBraces.h>

#include "EEPROM.h"   // Include ESP32 EEPROM library

#define EEPROM_START_ADDR 0  // Start EEPROM address for user data. Addresses below address 76 are waveform data!
#define EEPROM_SIZE       512 // How much data to write to EEPROM in this example

class WiFiAPSettingsClass
{
 protected:
	 static void updateHTML();
	 static void handleRoot();
	 static void handleString();
	 static void writeEEPROM();
	 static void readEEPROM();
	 static int writeStringToEEPROM(int addrOffset, const String& strToWrite);
	 static int readStringFromEEPROM(int addrOffset, String* strToRead);
	 void insertParamInHTMLPage(String paramText, String paramValue);
	 bool DEBUG_PRINT = false;

 public:
	 WiFiAPSettingsClass();
	 void initAP();
	 void loop();
	  //Inkplate* display;
	 const char* SSID_AP_Settings = "FramEInk";
	 IPAddress serverIP;
	 const uint16_t settingDuration = 10; // [min]
	 String SSID_User;
	 String PWD_User;
	 String Latitude_User;
	 String Longitude_User;
	 String ICALID_User;
	 time_t Last_Awake_Epoch;

};

//extern WiFiAPSettingsClass WiFiAPSettings;

#endif

