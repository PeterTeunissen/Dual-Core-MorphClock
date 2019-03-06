/*
remix from HarryFun's great Morphing Digital Clock idea https://github.com/hwiguna/HariFun_166_Morphing_Clock
follow the great tutorial there and eventually use this code as alternative

provided 'AS IS', use at your own risk
 * mirel.t.lazar@gmail.com
 */

#include <NtpClientLib.h>
#include <ESP8266WiFi.h>

#define double_buffer
#include <PxMatrix.h>

extern "C" {
  #include "user_interface.h"
}

//#define USE_ICONS
//#define USE_FIREWORKS
//#define USE_WEATHER_ANI

//#include <Adafruit_GFX.h>    // Core graphics library
//#include <Fonts/FreeMono9pt7b.h>

//=== WIFI MANAGER ===
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
char wifiManagerAPName[] = "MorphClk";
char wifiManagerAPPassword[] = "MorphClk";

//== SAVING CONFIG ==
#include "FS.h"
#include <ArduinoJson.h>
bool shouldSaveConfig = false; // flag for saving data

//callback notifying us of the need to save config
void saveConfigCallback() {
  //Serial.println("Should save config");
  shouldSaveConfig = true;
}

#ifdef ESP8266
  #include <Ticker.h>
  Ticker display_ticker;
  #define P_LAT 16
  #define P_A 5
  #define P_B 4
  #define P_C 15
  #define P_D 12
  #define P_E 0
  #define P_OE 2
#endif

// Pins for LED MATRIX
PxMATRIX display(64, 32, P_LAT, P_OE, P_A, P_B, P_C, P_D, P_E);

//=== SEGMENTS ===
int cin = 25; //color intensity
#include "Digit.h"
Digit digit0(&display, 0, 63 - 1 - 9*1, 8, display.color565(0, 0, 255));
Digit digit1(&display, 0, 63 - 1 - 9*2, 8, display.color565(0, 0, 255));
Digit digit2(&display, 0, 63 - 4 - 9*3, 8, display.color565(0, 0, 255));
Digit digit3(&display, 0, 63 - 4 - 9*4, 8, display.color565(0, 0, 255));
Digit digit4(&display, 0, 63 - 7 - 9*5, 8, display.color565(0, 0, 255));
Digit digit5(&display, 0, 63 - 7 - 9*6, 8, display.color565(0, 0, 255));

#ifdef ESP8266
  // ISR for display refresh
  void display_updater() {
    //display.displayTestPattern(70);
    display.display(70);
  }
#endif

void getWeather();

void configModeCallback (WiFiManager *myWiFiManager) {
  //Serial.println(F("Entered config mode"));
  Serial.println(WiFi.softAPIP());
}

char readBuf[10];
int oldTemp;
int newTemp;
char readMode= ' ';
char timezone[5] = "-5";
char military[3] = "Y";     // 24 hour mode? Y/N
char u_metric[3] = "N";     // use metric for units? Y/N
char date_fmt[7] = "M/D/Y"; // date format: D.M.Y or M.D.Y or M.D or D.M or D/M/Y.. looking for trouble
int digitColor;
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println(F("Err opn cnfg"));
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println(F("Cnfg larger 1024"));
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println(F("Failed to parse config file"));
    return false;
  }

  if (json.get<const char*>("timezone")) {
    strcpy(timezone, json["timezone"]);
  } else {
    Serial.println(F("timezone not set. Using: -5"));    
  }
  
  if (json.get<const char*>("military")) {
    strcpy(military, json["military"]);
  } else {
    Serial.println(F("military not set. Using: Y"));    
  }
  //avoid reboot loop on systems where this is not set
  if (json.get<const char*>("metric")) {
    strcpy(u_metric, json["metric"]);
  } else {
    Serial.println(F("metric not set,: Y"));
  }
  if (json.get<const char*>("date-format")) {
    strcpy(date_fmt, json["date-format"]);
  } else {
    Serial.println(F("date fmt not set: D.M.Y"));
  }
  
  return true;
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["timezone"] = timezone;
  json["military"] = military;
  json["metric"] = u_metric;
  json["date-format"] = date_fmt;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println(F("Err open cnf for w"));
    return false;
  }

//  Serial.println ("Saving configuration to file:");
//  Serial.print ("timezone=");
//  Serial.println (timezone);
//  Serial.print ("military=");
//  Serial.println (military);
//  Serial.print ("metric=");
//  Serial.println (u_metric);
//  Serial.print ("date-format=");
//  Serial.println (date_fmt);

  json.printTo(configFile);
  return true;
}

#include "TinyFont.h"
const byte row0 = 2+0*10;
const byte row1 = 2+1*10;
const byte row2 = 2+2*10;
void wifi_setup() {
  //-- Config --
  if (!SPIFFS.begin()) {
    Serial.println(F("Fail mount FS"));
    return;
  }
  loadConfig ();

  //-- Display --
  display.fillScreen(display.color565 (0, 0, 0));
  display.setTextColor(display.color565 (0, 0, 255));

  //-- WiFiManager --
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  WiFiManagerParameter timeZoneParameter("timeZone", "TimeZn(hrs)", timezone, 5); 
  wifiManager.addParameter(&timeZoneParameter);
  WiFiManagerParameter militaryParameter("military", "24Hr (Y/N)", military, 3); 
  wifiManager.addParameter(&militaryParameter);
  WiFiManagerParameter metricParameter("metric", "Metric?(Y/N)", u_metric, 3); 
  wifiManager.addParameter(&metricParameter);
  WiFiManagerParameter dmydateParameter("date_fmt", "DateFMTt (D.M.Y)", date_fmt, 6); 
  wifiManager.addParameter(&dmydateParameter);

  if (analogRead(A0)<512) {
    Serial.println(F("Config mode"));

    display.setCursor(0, row0);
    display.print("AP:");
    display.print(wifiManagerAPName);

    display.setCursor(0, row1);
    display.print("Pw:");
    display.print(wifiManagerAPPassword);

    display.setCursor(0, row2);
    display.print("192.168.4.1");

    wifiManager.startConfigPortal (wifiManagerAPName, wifiManagerAPPassword);

    display.fillScreen(display.color565(0, 0, 0));
  } else {
    Serial.println(F("Normal mode"));

    //display.setCursor (2, row1);
    //display.print ("connecting");
    TFDrawText(&display, String("   CONNECTING   "), 0, 13, display.color565(0, 0, 255));

    //fetches ssid and pass from eeprom and tries to connect
    //if it does not connect it starts an access point with the specified name wifiManagerAPName
    //and goes into a blocking loop awaiting configuration
    wifiManager.autoConnect(wifiManagerAPName);
  }
  
//  Serial.print(F("timezone="));
//  Serial.println(timezone);
//  Serial.print(F("military="));
//  Serial.println(military);
//  Serial.print(F("metric="));
//  Serial.println(u_metric);
//  Serial.print(F("date-format="));
//  Serial.println(date_fmt);
  //timezone
  strcpy(timezone, timeZoneParameter.getValue ());
  //military time
  strcpy(military, militaryParameter.getValue ());
  //metric units
  strcpy(u_metric, metricParameter.getValue ());
  //date format
  strcpy(date_fmt, dmydateParameter.getValue ());
  //display.fillScreen (0);
  //display.setCursor (2, row1);
  TFDrawText(&display, String("     ONLINE     "), 0, 13, display.color565(0, 0, 255));
  Serial.print(F("WiFi connected, IP address: "));
  Serial.println(WiFi.localIP());
  //
  //start NTP
  NTP.begin("pool.ntp.org", String(timezone).toInt(), false);
  NTP.setInterval(10);//force rapid sync in 10sec

  if (shouldSaveConfig) {
    saveConfig ();
  }
  
  getWeather ();
}

byte hh;
byte hh24;
byte mm;
byte ss;
byte ntpsync = 1;
//
void setup() {	
  Serial.begin(115200);
  //display setup
  display.begin (16);
  display.setDriverChip(FM6126A);

  Serial.print(F("Mem1a:"));
  Serial.println(system_get_free_heap_size());

#ifdef ESP8266
  display_ticker.attach(0.002, display_updater);
#endif

  Serial.print(F("Mem1b:"));
  Serial.println(system_get_free_heap_size());

  wifi_setup ();

  Serial.print(F("Mem1c:"));
  Serial.println(system_get_free_heap_size());

  NTP.onNTPSyncEvent ([](NTPSyncEvent_t ntpEvent) {
    if (ntpEvent) {
      Serial.print("TimeSync err:");
      if (ntpEvent == noResponse)
        Serial.println(F("NTP not reach"));
      else if (ntpEvent == invalidAddress)
        Serial.println(F("Invalid NTP address"));
    } else {
      Serial.print(F("Got NTP time:"));
      Serial.println(NTP.getTimeDateString (NTP.getLastNTPSync ()));
      ntpsync = 1;
    }
  });
  //prep screen for clock display
  display.fillScreen(0);
  int cc_gry = display.color565(128, 128, 128);
  digitColor = cc_gry;
  //reset digits color
  digit0.setColor(cc_gry);
  digit1.setColor(cc_gry);
  digit2.setColor(cc_gry);
  digit2.setColonLeft(false);
  digit3.setColor(cc_gry);
  digit4.setColonLeft(false);
  digit4.setColor(cc_gry);
  digit5.setColor(cc_gry);

  digit2.DrawColon(cc_gry);
  digit4.DrawColon(cc_gry);

  if (military[0] == 'N') {
    digit0.setSize(3);
    digit0.setY(digit0.getY() + 6);
    digit0.setX(digit0.getX() - 1);
    digit1.setSize(3);
    digit1.setX(digit1.getX() + 2);
    digit1.setY(digit1.getY() + 6);
  }

  strcpy(readBuf,"");
  }

//open weather map api key 
String apiKey   = "aec6c8810510cce7b0ee8deca174c79a"; //e.g a hex string like "abcdef0123456789abcdef0123456789"
//the city you want the weather for 
String location = "Phoenixville,US"; //e.g. "Paris,FR"
char server[]   = "api.openweathermap.org";
WiFiClient client;
int tempMin = -10000;
int tempMax = -10000;
int tempM = -10000;
int presM = -10000;
int humiM = -10000;
String condS = "";
void getWeather() {
  if (!apiKey.length()) {
    Serial.println(F("No API KEY for weather")); 
    return;
  }
  Serial.print(F("i:conn to weather")); 
  // if you get a connection, report back via serial: 
  if (client.connect(server, 80)) { 
    Serial.println(F("connected.")); 
    // Make a HTTP request: 
    client.print("GET /data/2.5/weather?"); 
    client.print("q="+location); 
    client.print("&appid="+apiKey); 
    client.print("&cnt=1"); 
    (*u_metric=='Y')?client.println ("&units=metric"):client.println ("&units=imperial");
    client.println("Host: api.openweathermap.org"); 
    client.println("Connection: close");
    client.println(); 
  } else { 
    Serial.println(F("w:fail connect"));
    return;
  } 
  delay(200);
  String sval = "";
  int bT, bT2;
  //do your best
  String line = client.readStringUntil('\n');
  if (!line.length ()) {
    Serial.println(F("w:fail weather"));
  } else {
//    Serial.print(F("weather:")); 
//    Serial.println(line); 
//    //weather conditions - "main":"Clear",
//    bT = line.indexOf("\"main\":\"");
//    if (bT > 0) {
//      bT2 = line.indexOf("\",\"", bT + 8);
//      sval = line.substring(bT + 8, bT2);
//      Serial.print(F("cond "));
//      Serial.println(sval);
//      //0 - unk, 1 - sunny, 2 - cloudy, 3 - overcast, 4 - rainy, 5 - thunders, 6 - snow
//      condM = 0;
//      if (sval.equals("Clear")) 
//        condM = 1;
//      else if (sval.equals("Clouds"))
//        condM = 2;
//      else if (sval.equals("Overcast"))
//        condM = 3;
//      else if (sval.equals("Rain"))
//        condM = 4;
//      else if (sval.equals("Drizzle"))
//        condM = 4;
//      else if (sval.equals("Thunderstorm"))
//        condM = 5;
//      else if (sval.equals("Snow"))
//        condM = 6;
//      //
//      condS = sval;
//      Serial.print(F("condM "));
//      Serial.println(condM);
//    }
    //tempM
    bT = line.indexOf("\"temp\":");
    if (bT > 0) {
      bT2 = line.indexOf(",\"", bT + 7);
      sval = line.substring (bT + 7, bT2);
      Serial.print(F("temp: "));
      Serial.println (sval);
      tempM = sval.toInt ();
    } else {
      Serial.println(F("temp NF!"));
    }
    //tempMin
    bT = line.indexOf("\"temp_min\":");
    if (bT > 0) {
      bT2 = line.indexOf(",\"", bT + 11);
      sval = line.substring (bT + 11, bT2);
      Serial.print(F("temp min: "));
      Serial.println(sval);
      tempMin = sval.toInt ();
    } else {
      Serial.println(F("temp_min NF!"));
    }
    //tempMax
    bT = line.indexOf("\"temp_max\":");
    if (bT > 0) {
      bT2 = line.indexOf("},", bT + 11);
      sval = line.substring (bT + 11, bT2);
      Serial.print("temp max: ");
      Serial.println(sval);
      tempMax = sval.toInt ();
    } else {
      Serial.println("temp_max NF!");
    }
    //pressM
    bT = line.indexOf("\"pressure\":");
    if (bT > 0) {
      bT2 = line.indexOf(",\"", bT + 11);
      sval = line.substring (bT + 11, bT2);
      Serial.print(F("press "));
      Serial.println(sval);
      presM = sval.toInt();
    } else {
      Serial.println(F("pressure NF!"));
    }
    //humiM
    bT = line.indexOf("\"humidity\":");
    if (bT > 0) {
      bT2 = line.indexOf (",\"", bT + 11);
      sval = line.substring (bT + 11, bT2);
      Serial.print(F("humi "));
      Serial.println (sval);
      humiM = sval.toInt();
    } else {
      Serial.println(F("humidity NF!"));
    }
  }//connected
}

void readLoop() {

  while (Serial.available()) {
    char c = Serial.read();
    if (c=='>' && readMode==' ') {
      readMode='T';
      strcpy(readBuf,"");
    }

    if (c=='[' && readMode==' ') {
      readMode='Z';
      strcpy(readBuf,"");
    }

    if (c=='<' && readMode=='T') {
      Serial.println();
      newTemp = atoi(readBuf);
      readMode=' ';      
    }

    if (c==']' && readMode=='Z') {
      Serial.println();
      strcpy(timezone,readBuf);
      NTP.setTimeZone(atoi(readBuf),0);
      saveConfig();
      readMode=' ';      
    }

    if ((readMode=='Z' || readMode=='T') && strlen(readBuf)<8 && c>'*' && c<':') {
      strncat(readBuf,&c,1);
//      Serial.print(F("readBuf:"));
      Serial.println(readBuf);
    }

    if (strlen(readBuf)>8) {
      readMode = ' ';
      Serial.println(F("readBuf overrun."));      
    }
  }  
}

int xo = 1, yo = 26;

void draw_weather() {
  int cc_wht = display.color565(cin, cin, cin);
  int cc_red = display.color565(cin, 0, 0);
  int cc_grn = display.color565(0, cin, 0);
  int cc_blu = display.color565(0, 0, cin);
  //int cc_ylw = display.color565(cin, cin, 0);
  //int cc_gry = display.color565(128, 128, 128);
  int cc_dgr = display.color565(30, 30, 30);
//  Serial.println(F("showing the weather"));
  xo = 0; 
  yo = 1;
  TFDrawText (&display, String("                "), xo, yo, cc_dgr);
  if (tempM == -10000 || humiM == -10000 || presM == -10000) {
    //TFDrawText (&display, String("NO WEATHER DATA"), xo, yo, cc_dgr);
//    Serial.println(F("!no weather data available"));
  } else {
    //weather below the clock
    //-temperature
    int lcc = cc_red;
    if (*u_metric == 'Y') {
      //C
      if (newTemp < 26)
        lcc = cc_grn;
      if (newTemp < 18)
        lcc = cc_blu;
      if (newTemp < 6)
        lcc = cc_wht;
    } else {
      //F
      if (newTemp < 79)
        lcc = cc_grn;
      if (newTemp < 64)
        lcc = cc_blu;
      if (newTemp < 43)
        lcc = cc_wht;
    }
    //
    String lstr = String(newTemp) + String((*u_metric=='Y')?"C":"F");
    Serial.print(F("tempI:"));
    Serial.println(lstr);
    TFDrawText(&display, lstr, xo, yo, lcc);

    lcc = cc_red;
    if (*u_metric == 'Y') {
      //C
      if (tempM < 26) {
        lcc = cc_grn;
      }
      if (tempM < 18) {
        lcc = cc_blu;
      }
      if (tempM < 6) {
        lcc = cc_wht;
      }
    } else {
      //F
      if (tempM < 79) {
        lcc = cc_grn;
      }
      if (tempM < 64) {
        lcc = cc_blu;
      }
      if (tempM < 43) {
        lcc = cc_wht;
      }
    }

    xo=TF_COLS*lstr.length();
    TFDrawText(&display, String("/"), xo, yo, cc_grn);

    xo+=TF_COLS;
    lstr = String(tempM) + String((*u_metric=='Y')?"C":"F");
    Serial.print(F("tempO:"));
    Serial.println(lstr);
    TFDrawText(&display, lstr, xo, yo, lcc);
    
    //weather conditions
    //-humidity
    lcc = cc_red;
    if (humiM < 65) {
      lcc = cc_grn;
    }
    if (humiM < 35) {
      lcc = cc_blu;
    }
    if (humiM < 15) {
      lcc = cc_wht;
    }
    lstr = String (humiM) + "%";
    xo = 8*TF_COLS;
    TFDrawText(&display, lstr, xo, yo, lcc);
    //-pressure
    lstr = String(presM);
    xo = 12*TF_COLS;
    TFDrawText(&display, lstr, xo, yo, cc_grn);
    //draw temp min/max
    if (tempMin > -10000) {
      xo = 0*TF_COLS; 
      yo = 26;
      TFDrawText(&display, "   ", xo, yo, 0);
      lstr = String(tempMin);// + String((*u_metric=='Y')?"C":"F");
      //blue if negative
      int ct = cc_dgr;
      if (tempMin < 0) {
        ct = cc_blu;
        lstr = String(-tempMin);// + String((*u_metric=='Y')?"C":"F");
      }
      Serial.print(F("temp min: "));
      Serial.println(lstr);
      TFDrawText(&display, lstr, xo, yo, ct);
    }
    if (tempMax > -10000) {
      TFDrawText(&display, "   ", 13*TF_COLS, yo, 0);
      //move the text to the right or left as needed
      xo = 14*TF_COLS; 
      yo = 26;
      if (tempMax < 10)
        xo = 15*TF_COLS;
      if (tempMax > 99)
        xo = 13*TF_COLS;
      lstr = String (tempMax);// + String((*u_metric=='Y')?"C":"F");
      //blue if negative
      int ct = cc_dgr;
      if (tempMax < 0) {
        ct = cc_blu;
        lstr = String(-tempMax);// + String((*u_metric=='Y')?"C":"F");
      }
      Serial.print(F("temp max: "));
      Serial.println(lstr);
      TFDrawText(&display, lstr, xo, yo, ct);
    }
  }
}

//
void draw_date() {
  int cc_grn = display.color565(0, cin, 0);
  Serial.println(F("showing the date"));
  //for (int i = 0 ; i < 12; i++)
    //TFDrawChar (&display, '0' + i%10, xo + i * 5, yo, display.color565 (0, 255, 0));
  //date below the clock
  long tnow = now();
  String lstr = "";
  for (int i = 0; i < 5; i += 2) {
    switch (date_fmt[i]) {
      case 'D':
        lstr += (day(tnow) < 10 ? "0" + String(day(tnow)) : String(day(tnow)));
        if (i < 4)
          lstr += date_fmt[i + 1];
        break;
      case 'M':
        lstr += (month(tnow) < 10 ? "0" + String(month(tnow)) : String(month(tnow)));
        if (i < 4)
          lstr += date_fmt[i + 1];
        break;
      case 'Y':
        lstr += String(year(tnow));
        if (i < 4)
          lstr += date_fmt[i + 1];
        break;
    }
  }
  //
  if (lstr.length()) {
    //
    xo = 3*TF_COLS; 
    yo = 26;
    TFDrawText(&display, lstr, xo, yo, cc_grn);
  }
}

byte prevhh = 0;
byte prevmm = 0;
byte prevss = 0;
long tnow;
void loop() {
  int cc_grn = display.color565(0, cin, 0);
  //time changes every miliseconds, we only want to draw when digits actually change
  tnow = now();
  //
  hh = hour(tnow);   //NTP.getHour ();
  hh24 = hh;
  mm = minute(tnow); //NTP.getMinute ();
  ss = second(tnow); //NTP.getSecond ();
  //
  if (ntpsync) {

//    Serial.print(F("Mm3:"));
//    Serial.println(system_get_free_heap_size());

    ntpsync = 0;
    //
    prevss = ss;
    prevmm = mm;
    prevhh = hh;
    //brightness control: dimmed during the night(25), bright during the day(150)
    if (hh >= 20 && cin == 150) {
      cin = 25;
//      Serial.println(F("night mode brightness"));
    }
    if (hh < 8 && cin == 150) {
      cin = 25;
//      Serial.println(F("night mode brightness"));
    }
    //during the day, bright
    if (hh >= 8 && hh < 20 && cin == 25) {
      cin = 150;
//      Serial.println(F("day mode brightness"));
    }
    //we had a sync so draw without morphing
    int cc_gry = display.color565 (128, 128, 128);
    int cc_dgr = display.color565 (30, 30, 30);
    //dark blue is little visible on a dimmed screen
    //int cc_blu = display.color565 (0, 0, cin);
    cc_grn = display.color565(0, cin, 0);
    int cc_col = cc_gry;
    //
    if (cin == 25) {
      cc_col = cc_dgr;
    }
    //reset digits color
    digit0.setColor(cc_col);
    digit1.setColor(cc_col);
    digit2.setColor(cc_col);
    digit3.setColor(cc_col);
    digit4.setColor(cc_col);
    digit5.setColor(cc_col);
    digitColor = cc_col;
    //clear screen
    display.fillScreen (0);
    //date and weather
    draw_weather();
    draw_date();
    //
    digit2.DrawColon(cc_col);
    digit4.DrawColon(cc_col);
    //military time?
    if (military[0] == 'N') {
      hh = hourFormat12(tnow);
    }
    //
    digit0.Draw(ss % 10);
    digit1.Draw(ss / 10);
    digit2.Draw(mm % 10);
    digit3.Draw(mm / 10);
    digit4.Draw(hh % 10);

    if (military[0] == 'N') {
      TFDrawChar(&display, (isAM()?'A':'P'), 63 - 1 + 3 - 9 * 2, 19, cc_grn);
      TFDrawChar(&display, 'M', 63 - 1 - 2 - 9 * 1, 19, cc_grn);
      if (hh/10==0) {
        digit5.hide(); 
      }     
    } else {
      digit5.Draw(hh / 10);
    }
       
  } else {

    readLoop();
    
    //seconds
    if (ss != prevss) {
//      Serial.print(F("Mm4:"));
//      Serial.println(system_get_free_heap_size());

      int s0 = ss % 10;
      int s1 = ss / 10;
      if (s0 != digit0.Value ()) digit0.Morph (s0);
      if (s1 != digit1.Value ()) digit1.Morph (s1);
      //ntpClient.PrintTime();
      prevss = ss;
      //refresh weather every 5mins at 30sec in the minute
      if (ss==15 || ss==45) {
        Serial.print(">");
        Serial.print(String((*u_metric=='Y')?"C":"F"));
        Serial.println("<");
      }
      if (ss == 30 && ((mm % 5) == 0)) {
        getWeather ();
      }
    }
    //minutes
    if (mm != prevmm) {
      int m0 = mm % 10;
      int m1 = mm / 10;
      if (m0 != digit2.Value ()) digit2.Morph (m0);
      if (m1 != digit3.Value ()) digit3.Morph (m1);
      prevmm = mm;
      //
      draw_weather();
      //drawTemp();
    }
    //hours
    if (hh != prevhh) {
      prevhh = hh;
      //
      draw_date();
      //brightness control: dimmed during the night(25), bright during the day(150)
      if (hh == 20 || hh == 8) {
        ntpsync = 1;
        //bri change is taken care of due to the sync
      }
      //military time?
      if (military[0] == 'N') {
        hh = hourFormat12(tnow);
      }
      //
      int h0 = hh % 10;
      int h1 = hh / 10;
      if (h0 != digit4.Value ()) digit4.Morph (h0);
      //if (h1 != digit5.Value ()) digit5.Morph (h1);

      if (military[0] != 'N') {
        if (h1 != digit5.Value()) {
          digit5.setColor(digitColor);
          digit5.Morph(h1);
        }
      } else {
        TFDrawChar(&display, (isAM()?'A':'P'), 63 - 1 + 3 - 9 * 2, 19, cc_grn);
        TFDrawChar(&display, 'M', 63 - 1 - 2 - 9 * 1, 19, cc_grn);
        if (h1 == 0) {
          digit5.hide();
        } else {
          if (h1 != digit5.Value()) {
            digit5.setColor(digitColor);
            digit5.Morph(h1);
          }
        }
      }

    }//hh changed
  }
  //set NTP sync interval as needed
  if (NTP.getInterval() < 3600 && year(now()) > 1970) {
    //reset the sync interval if we're already in sync
    NTP.setInterval(3600 * 24);//re-sync every 24 hours
  }
}
