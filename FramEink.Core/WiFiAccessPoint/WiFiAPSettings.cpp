// 
// 
// 

#include "WiFiAPSettings.h"
#include "HTMLSettingsPage.h"   //Include .h file where we stored out html code of our web page

//#define ssid "FramEInk"

WebServer server(80); // Create Web server on port 80 (HTTP port number)

bool settingsDone;
String User_SSID;
String User_PWD;
String User_Latitude;
String User_Longitude;
String User_ICALID;

WiFiAPSettingsClass::WiFiAPSettingsClass()
{
}
//WiFiAPSettingsClass::WiFiAPSettingsClass(Inkplate* _display)
//{
//    display = _display;
//}

void WiFiAPSettingsClass::init()
{
    // Init EEPROM library with 512 of EEPROM size. Do not change this value, it can wipe waveform data!
    EEPROM.begin(512);
    Serial.println("begin...");


    readEEPROM();
    Serial.println("readEEPROM...");

    WiFi.begin();            // Init. WiFi library
    WiFi.mode(WIFI_AP);      // Set WiFi to Access point mode
    WiFi.softAP(SSID_Settings); // Set SSID (WiFi name) and password for Access point

    serverIP = WiFi.softAPIP(); // Get the server IP address

    settingsDone = false;
    server.on("/", this->handleRoot); // If you open homepage, go to handle root function
    server.on(UriBraces("/string/{}"), handleString); // If you send some text to Inkplate, go to handleString function. Note that {} brackets at
    // the end of address. That means that web address has some arguments (our text!).
    server.begin();          // Start the web server
}

void WiFiAPSettingsClass::loop()
{
    unsigned long t0 = millis();
    while (!settingsDone && millis() - t0 < settingDuration * 60 * 1000)
    {
        server.handleClient(); // You have to constantly read if there is any new client connected to web server
    }
    // Copy the settings parameters to public variables
    SSID_User = User_SSID;
    PWD_User = User_PWD;
    Latitude_User = User_Latitude;
    Longitude_User = User_Longitude;
    ICALID_User = User_ICALID;
}

void WiFiAPSettingsClass::updateHTML()
{ // This function will send response to client and send HTML code of our web page
    server.send(200, "text/html", strHTML);
}

void WiFiAPSettingsClass::handleRoot()
{ // This function will send response to client if client open a root (homepage) of our web page
    updateHTML();
}

void WiFiAPSettingsClass::handleString()
{ // This function will send response to client, send HTML code of web page, get the text from argument sent in web page
    // address and refresh screen with new text
    
    User_SSID = server.arg(0);
    User_PWD = server.arg(1);
    User_Latitude = server.arg(2);
    User_Longitude = server.arg(3);
    User_ICALID = server.arg(4);
    //updateHTML();
    writeEEPROM();
    settingsDone = true;
}

void WiFiAPSettingsClass::writeEEPROM()
{
    // TODO: controllare la lunghezza delle stringhe che non sia > 255 e che il totale non ecceda la dimensione della eeprom
    int str1AddrOffset = writeStringToEEPROM(EEPROM_START_ADDR, User_SSID);
    int str2AddrOffset = writeStringToEEPROM(str1AddrOffset, User_PWD);
    int str3AddrOffset = writeStringToEEPROM(str2AddrOffset, User_Latitude);
    int str4AddrOffset = writeStringToEEPROM(str3AddrOffset, User_Longitude);
    int str5AddrOffset = writeStringToEEPROM(str4AddrOffset, User_ICALID);
    EEPROM.commit();
}

void WiFiAPSettingsClass::readEEPROM() {
    int newStr1AddrOffset = readStringFromEEPROM(EEPROM_START_ADDR, &User_SSID);
    int newStr2AddrOffset = readStringFromEEPROM(newStr1AddrOffset, &User_PWD);
    int newStr3AddrOffset = readStringFromEEPROM(newStr2AddrOffset, &User_Latitude);
    int newStr4AddrOffset = readStringFromEEPROM(newStr3AddrOffset, &User_Longitude);
    int newStr5AddrOffset = readStringFromEEPROM(newStr4AddrOffset, &User_ICALID);
}

int WiFiAPSettingsClass::writeStringToEEPROM(int addrOffset, const String& strToWrite)
{
    byte len = strToWrite.length();
    EEPROM.write(addrOffset, len);
    for (int i = 0; i < len; i++)
    {
        EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
    }
    return addrOffset + 1 + len;
}
int WiFiAPSettingsClass::readStringFromEEPROM(int addrOffset, String* strToRead)
{
    int newStrLen = EEPROM.read(addrOffset);
    Serial.printf("Length EEPROM READ: %d\n", newStrLen);
    // If the array was not initialized, return
    if (newStrLen == 255)
        return 0;

    char data[255];
    if (addrOffset + newStrLen >= EEPROM_SIZE)
        return -1;
    for (int i = 0; i < newStrLen; i++)
    {
        data[i] = EEPROM.read(addrOffset + 1 + i);
    }
    data[newStrLen] = '\0';
    *strToRead = String(data);
    Serial.printf("EEPROM Read: %s\n", * strToRead);
    return addrOffset + 1 + newStrLen;
}


WiFiAPSettingsClass WiFiAPSettings;

