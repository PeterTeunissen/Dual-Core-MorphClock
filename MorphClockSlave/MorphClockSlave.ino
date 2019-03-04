#include <stdio.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Adafruit_WS2801.h"
#include "SPI.h"
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include "ESP8266HTTPClient.h"
#include <Ticker.h>
#include <EEPROM.h>
#include "html_pages.h"
#include <ArduinoJson.h>
#include <NtpClientLib.h>

#define ONE_WIRE_BUS_PIN 12 // D6
#define NUMPIXELS 14  // Number of LED's in your strip

//#define LOG_MAX 1

const int LED_PIN = 2;
const int SENSOR_INTERVAL = 5;
unsigned long lastTimeSent = 0;
uint8_t dataPin  = 2;  // D4  Yellow wire on Adafruit Pixels
uint8_t clockPin = 14; //D5    Green wire on Adafruit Pixels
int mode_flag = 1;
long red_int = 0;
long green_int = 0;
long blue_int = 255;
int brightness = 255;
String rgb_now = "#0000ff";    //global rgb state values for use in various html pages
bool isReading = false;
char tempBuf[10];
char tempType=' ';
String timeZone="";
char timeZoneBuf[20];
char timeZoneOffset[10];
int prevMin;

Ticker ticker;
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature sensors(&oneWire);

Adafruit_WS2801 strip = Adafruit_WS2801(NUMPIXELS, dataPin, clockPin);

MDNSResponder mdns;
ESP8266WebServer server(80);

void tick() {
  //toggle state
  int state = digitalRead(LED_PIN);  // get the current state of GPIO1 pin
  digitalWrite(LED_PIN, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(F("Entering config mode"));
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN,OUTPUT);
  
  EEPROM.begin(5);

  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  Serial.println(F("Starting strip"));
  strip.begin();
  strip.show();

  Serial.println(F("Starting sensors"));
  sensors.begin();  
  Serial.println(sensors.getDeviceCount());

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  if (analogRead(A0) < 500) {
    Serial.println(F("Entering config mode"));
    if (!wifiManager.startConfigPortal("ClockSlaveConfig")) {
      Serial.println(F("Failed to connect and hit timeout"));
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  }
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println(F("Failed to connect and hit timeout"));
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println(F("Connected..."));

  ticker.detach();

  //keep LED on
  digitalWrite(BUILTIN_LED, LOW);

  if (mdns.begin("ClockSlave_", WiFi.localIP() ) ) {
    Serial.println(F("MDNS responder started"));
  }

  red_int = EEPROM.read(0);       //restore colour to last used value. Ensures RGB lamp is same colour as when last switched off
  green_int = EEPROM.read(1);
  blue_int = EEPROM.read(2);
  brightness = EEPROM.read(3);
  mode_flag = EEPROM.read(4);
 
  server.on ( "/", handleIndex );
  server.onNotFound ( handleNotFound );
  
  server.on ( "/switch_on", handleSwitchOn);
  server.on ( "/switch_off", handleSwitchOff);
  server.on ( "/set_colour", handleSetColour);
  server.on ( "/set_colour_hash", handleColour );
  server.on ( "/set_brightness", handleSetBrightness);
  server.on ( "/set_bright_val", handleBrightness);
  server.on ( "/select_mode", handleSelectMode);
  server.on ( "/set_mode1", handle_mode_1); 
  server.on ( "/set_mode2", handle_mode_2);
  server.on ( "/set_mode3", handle_mode_3);

  server.begin();

  NTP.begin("pool.ntp.org", 0, false);
  NTP.setInterval(5*60); 
  
  handleSwitchOn();
}

void getTimeZone() {
  WiFiClient wifi;
  HTTPClient http;
  String timeZoneJson="";
  strcpy(timeZoneBuf,"America/New_York");
  strcpy(timeZoneOffset,"0");
  
  if (http.begin(wifi, "http://ip-api.com/json/")) {    
    #ifdef LOG_MAX
      Serial.print(F("[HTTP] GET...\n"));
    #else  
      Serial.print(F("."));
    #endif  
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      #ifdef LOG_MAX
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      #else  
        Serial.print(F("."));
      #endif  

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        timeZoneJson = http.getString();
        #ifdef LOG_MAX
          Serial.println(timeZoneJson);
        #endif  

        StaticJsonBuffer<1000> jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(timeZoneJson.c_str());
      
        if (!json.success ()) {
          Serial.println(F("Failed to parse ip-api response"));
          return;
        }
                
        if (json.get<const char*>("timezone")) {
          strcpy(timeZoneBuf, json["timezone"]);
          #ifdef LOG_MAX
            Serial.print(F("timezone name in json:"));    
            Serial.println(timeZoneBuf);    
          #else  
            Serial.print(F("."));
          #endif
        } else {
          Serial.println(F("timezone not found in json. Using: -5"));    
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }  
}

void getTimeZoneOffset() {
  //http://api.timezonedb.com/v2.1/get-time-zone?key=31VLCCL5BAKD&format=json&by=zone&zone=America/New_York
  WiFiClient wifi;
  HTTPClient http;

  String url = "http://api.timezonedb.com/v2.1/get-time-zone?key=31VLCCL5BAKD&format=json&by=zone&zone=" + String(timeZoneBuf);
  
  if (http.begin(wifi,url)) {    
    #ifdef LOG_MAX
      Serial.print("[HTTP] GET...\n");
    #else  
      Serial.print(F("."));
    #endif
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      #ifdef LOG_MAX
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      #endif  

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {

        strcpy(timeZoneOffset,"");
        
        String timeZoneJson = http.getString();
        #ifdef LOG_MAX
          Serial.println(timeZoneJson);
        #else  
          Serial.print(F("."));
        #endif  

        StaticJsonBuffer<1000> jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(timeZoneJson.c_str());
      
        if (!json.success ()) {
          Serial.println(F("Failed to parse time-api response"));
          return;
        }

        char b[15];
                
        if (json.get<const char*>("gmtOffset")) {
          strcpy(b, json["gmtOffset"]);
          int i = atoi(b);
          int h=i/3600;
          sprintf(timeZoneOffset,"%d",h);
          #ifdef LOG_MAX
            Serial.print(F("gmtOffset in json:"));    
            Serial.println(timeZoneOffset);    
          #else  
            Serial.print(F("."));
          #endif  
        } else {
          Serial.println(F("gmtOffset not found in json. Using: 0"));    
        }
      }
    }
  }
}

// Create a 24 bit color value from R,G,B
uint32_t getColor(byte r, byte g, byte b)
{
  uint32_t c;
  c = r;
  c <<= 8;
  c |= g;
  c <<= 8;
  c |= b;
  return c;
}

void handleIndex() {
  Serial.println(F("Request for index page received"));
  server.send(200, "text/html", page_contents);
}

void handleNotFound() {
  Serial.println(F("HandleNotFound"));
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}

void handleSwitchOn() {
  Serial.println(F("HandleSwitchOn"));
  mode_flag = EEPROM.read(4);                       // start-up in last saved mode
  delay(100);
  switch(mode_flag){
      case 1:handle_mode_1();
      break;
      case 2:handle_mode_2();
      break;
      case 3:handle_mode_3();
      break;
      default:                  
        light_up_all();                          //Default to fixed colour should the EEProm become corrupted
      break;
  }
  server.send ( 200, "text/html", "<SCRIPT language='JavaScript'>window.location='/';</SCRIPT>" );
};  
  
void handleSwitchOff() {
  Serial.println(F("HandleSwitchOff"));
  mode_flag=1;                                       //go to default fixed color mode and turn off all pixels
  delay(100);
  turn_off_all();
  server.send ( 200, "text/html", "<SCRIPT language='JavaScript'>window.location='/';</SCRIPT>" );
}
  
void handleSetColour() {     
  Serial.println(F("HandleSetColour"));
  String x = String(colour_picker);
  x.replace("%hash_value%",rgb_now);
  #ifdef LOG_MAX
    Serial.print("x=");
    Serial.println(x);
  #endif
//  server.send ( 200, "text/html", colour_picker);
  server.send ( 200, "text/html", x);
}
   
void handleSetBrightness(){
  Serial.println(F("HandleSetBrightness"));
  String x = String(bright_set);
  x.replace("%brightness%",String(brightness));
//  server.send ( 200, "text/html", bright_set);
  server.send ( 200, "text/html", x);
}  
    
void handleSelectMode(){
  Serial.println(F("HandleSelectMode"));
  server.send ( 200, "text/html", mode_page );
}

void handleColour(){
  Serial.println(F("HandleColour"));
  char buf_red[3];                               //char buffers to hold 'String' value converted to char array
  char buf_green[3];                       
  char buf_blue[3];                       
  String message = server.arg(0);                //get the 1st argument from the url which is the hex rgb value from the colour picker ie. #rrggbb (actually %23rrggbb)
  rgb_now = message; 
  rgb_now.replace("%23", "#");                   // change %23 to # as we need this in one of html pages
  #ifdef LOG_MAX
    Serial.print(F("Set color to:"));
    Serial.println(rgb_now);
  #endif
  String red_val = rgb_now.substring(1,3);       //extract the rgb values
  String green_val = rgb_now.substring(3,5); 
  String blue_val = rgb_now.substring(5,7);

  mode_flag=1;                                   //get to fixed colour mode if not already

  red_val.toCharArray(buf_red,3);                //convert hex 'String'  to Char[] for use in strtol() 
  green_val.toCharArray(buf_green,3);           
  blue_val.toCharArray(buf_blue,3);             

  red_int = gamma_adjust[strtol( buf_red, NULL, 16)];          //convert hex chars to ints and apply gamma adjust
  green_int = gamma_adjust[strtol( buf_green, NULL, 16)];
  blue_int = gamma_adjust[strtol( buf_blue, NULL, 16)];

  EEPROM.write(0,red_int);                  //write the colour values to EEPROM to be restored on start-up
  EEPROM.write(1,green_int);
  EEPROM.write(2,blue_int);
  EEPROM.commit();

  light_up_all(); 
  String java_redirect = "<SCRIPT language='JavaScript'>window.location='/set_colour?";
        java_redirect += message;                                                 //send hash colour value in URL to update the colour picker control
        java_redirect += "';</SCRIPT>";
  server.send ( 200, "text/html", java_redirect );                                 // all done! - take user back to the colour picking page                                                                                          
}

void handleBrightness() {
  Serial.println(F("HandleBrightness"));
  String message = server.arg(0);                //get the 1st argument from the url which is the brightness level set by the slider
  String bright_val = message.substring(0,3);    //extract the brightness value from the end of the argument in the URL
  brightness =  bright_val.toInt();
  EEPROM.write(3,brightness);                    //write the brightness value to EEPROM to be restored on start-up
  EEPROM.commit();

  #ifdef LOG_MAX
    Serial.print(F("Set brightness to:"));
    Serial.println(brightness);
  #endif
  String java_redirect = "<SCRIPT language='JavaScript'>window.location='/set_brightness?";
          java_redirect += brightness;                                              //send brightness value in URL to update the slider control
          java_redirect += "';</SCRIPT>";
  server.send ( 200, "text/html", java_redirect);                                  // all done! - take user back to the brightness selection page                                                                                          
}

void light_up_all() {
  for(int i=0;i<NUMPIXELS;i++){      
    strip.setPixelColor(i, getColor(brightness*red_int/255,brightness*green_int/255,brightness*blue_int/255));                            // Set colour with gamma correction and brightness adjust value. 
    strip.show();
  }                                                                                                                            
}

void turn_off_all() {
  Serial.println(F("HandleTurnOffAll"));
  mode_flag=999;                                       //go to non-existent mode and turn off all pixels
  EEPROM.write(4,mode_flag);                          //write mode to EEProm so can be restored on start-up
  EEPROM.commit();
  for(int i=0;i<NUMPIXELS;i++){                                                                                                              // pixels.Color takes RGB values, from 0,0,0 up to 255,255,255
    strip.setPixelColor(i, getColor(0,0,0));                                                                                               // Turn off led strip
    strip.show();
  }                                                                                                                            // This sends the updated pixel color to the hardware.
}  

int j;
int m;
long lastFire;
int wait=1;

void handle_mode_1(){                                  //fixed colour mode
  Serial.println(F("HandleMode1"));
  mode_flag = 1;
  EEPROM.write(4,mode_flag);                          //write mode to EEProm so can be restored on start-up
  EEPROM.commit();
  server.send ( 200, "text/html","<SCRIPT language='JavaScript'>window.location='/';</SCRIPT>");
                                   
  light_up_all();                           //set mode to default state - all led's on, fixed colour. This loop will service any brightness changes
}

void handle_mode_2(){                                 //colour fade mode
  Serial.println(F("HandleMode2"));
  mode_flag = 2;
  EEPROM.write(4,mode_flag);                         //write mode to EEProm so can be restored on start-up
  EEPROM.commit();
  server.send ( 200, "text/html","<SCRIPT language='JavaScript'>window.location='/';</SCRIPT>");

  j=0;
  m=256;
  lastFire=0;
}

void handle_mode_3(){                                //rainbow mode
  Serial.println(F("HandleMode3"));
  mode_flag = 3;
  EEPROM.write(4,mode_flag);                        //write mode to EEProm so can be restored on start-up
  EEPROM.commit();
  server.send ( 200, "text/html","<SCRIPT language='JavaScript'>window.location='/';</SCRIPT>");

  j=0;
  m=256*5;
  lastFire=0;    
}

void colorLoop() {
  int i;
  if (mode_flag==2) {
    if (millis()-lastFire>(wait*200)) {
      lastFire = millis();
      j++;
      if (j>m) {
        j=0;
      } else {
        for(i=0; i<NUMPIXELS; i++) {
          if (mode_flag!=2){
            return;
          }
          strip.setPixelColor(i, Wheel((j) & 255));
          strip.show();
        } 
      }
    }    
  }
      
  if (mode_flag==3) {
    if (millis()-lastFire>(wait*50)) {
      lastFire = millis();
      j++;
      if (j>m) {
        j=0;
      } else {
        for(i=0; i < NUMPIXELS; i++) {
          if (mode_flag!=3){
            return;
          }
          strip.setPixelColor(i,Wheel(((i * 256 / NUMPIXELS) + j) & 255));
        }
        strip.show();      
      }
    }
  }      
}

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return getColor(brightness*(255 - WheelPos * 3)/255, 0, brightness*(WheelPos * 3)/255);  //scale the output values by a factor of global 'brightness' so that the brightness remains as set
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return getColor(0,brightness*(WheelPos * 3)/255, brightness*(255 - WheelPos * 3)/255);
  }
  WheelPos -= 170;
  return getColor(brightness*(WheelPos * 3)/255, brightness*(255 - WheelPos * 3)/255, 0);
}

void rainbow(uint8_t wait) {
  int i, j;
   
  for (j=0; j < 256; j++) {     // 3 cycles of all 256 colors in the wheel
    for (i=0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel( (i + j) % 255));
    }  
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

int prevSec;

void loop() {

  mdns.update();
  server.handleClient();
  colorLoop();

  while(Serial.available()) {
    char c = Serial.read();
    if (c=='>') {
      strcpy(tempBuf,"");
      isReading = true;          
    }

    // The master will tell us if we should report Celcius or Fahrenheit.
    if (isReading==true && (c=='F' || c=='C')) {
      strncat(tempBuf,&c,1);  
    }

    if (strlen(tempBuf)>3) {
      Serial.println("Buffer overrun");
      isReading = false;
    }
    
    if (c=='<' && isReading==true) {
      isReading = false;
      tempType=' ';
      if (strcmp(tempBuf,"C")==0) {
        tempType='C';
        Serial.println(F("Switch to C"));
      }
      if (strcmp(tempBuf,"F")==0) {
        tempType='F';
        Serial.println(F("Switch to F"));
      }
    }
  }

  if (second()!=prevSec) {
    prevSec = second();

    if (strlen(timeZoneOffset)==0) {
      getTimeZone();
      getTimeZoneOffset();            
    }
    
    // Sync time zone offset every 5 and 35 seconds
    if (second()==5 || second()==35){
      #ifdef LOG_MAX
        Serial.print(F("Bef:"));
        Serial.print(NTP.getTimeDateString(now()));
      #else
        Serial.println(F("."));  
      #endif
      prevSec = second();
  
      if ((hour()>23 || hour()<5) && (minute()>57 || minute()<4)) {
        getTimeZone();
        getTimeZoneOffset();      
      }
      
      #ifdef LOG_MAX
        Serial.print(F("Aft:"));
        Serial.println(NTP.getTimeDateString(now()));
      #else
        Serial.println(F("."));  
      #endif
    }

    if (second()==15 || second()==45){
      if (strlen(timeZoneOffset)>0) {
        Serial.print(F("tz:["));
        Serial.print(timeZoneOffset);
        Serial.println(F("]"));      
      }
    }

    if (second()==20 || second()==50) {
      digitalWrite(LED_PIN, LOW);
  
      sensors.setWaitForConversion(false);
      sensors.requestTemperatures();  
      sensors.setWaitForConversion(true);
  
      float temp = -1000.0;
      if (tempType=='C') {
        Serial.println(F("Send C"));
        temp = sensors.getTempCByIndex(0);
      } 
      if (tempType=='F') {
        Serial.println(F("Send F"));
        temp = sensors.getTempFByIndex(0);
      }
  
      // Implements the 'protocol': a greater-then sign followed by value, followed by less-then sign.
      if (temp!=-1000.0) {
        Serial.print(F("tmp: >"));
        Serial.print(temp);
        Serial.println(F("<"));
      }
  
      digitalWrite(LED_PIN, HIGH);
      lastTimeSent = millis();      
    }
  }  
}
