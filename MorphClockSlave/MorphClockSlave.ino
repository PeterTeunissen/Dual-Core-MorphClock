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
#include <Ticker.h>
#include <EEPROM.h>
#include "html_pages.h"


#define ONE_WIRE_BUS_PIN 12 // D6
#define NUMPIXELS 32                 // Number of LED's in your strip

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
  
  handleSwitchOn();
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
  Serial.print("x=");
  Serial.println(x);
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
  Serial.print(F("Set color to:"));
  Serial.println(rgb_now);
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

  Serial.print(F("Set brightness to:"));
  Serial.println(brightness);

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
                                   
//  while(mode_flag==1){                                // Check the mode hasn't been changed whilst we wait, if so - leave immediately
    light_up_all();                           //set mode to default state - all led's on, fixed colour. This loop will service any brightness changes
//    loop();                                   // Not much to do except service the main loop       
//  }
}

void handle_mode_2(){                                 //colour fade mode
  Serial.println(F("HandleMode2"));
  mode_flag = 2;
  EEPROM.write(4,mode_flag);                         //write mode to EEProm so can be restored on start-up
  EEPROM.commit();
  server.send ( 200, "text/html","<SCRIPT language='JavaScript'>window.location='/';</SCRIPT>");
//  uint16_t i, j, k;
//  int wait = 10;  //DON'T ever set this more than '10'. Use the 'k' value in the loop below to increase delays. This prevents the watchdog timer timing out on the ESP8266

  j=0;
  m=256;
  lastFire=0;

//  while(mode_flag==2){
//    for(j=0; j<256; j++) {
//      loop();
//      for(i=0; i<NUMPIXELS; i++) {
//        if (mode_flag!=2){
//          return;
//        }                    //the mode has been changed - get outta here!
//        loop();
//        strip.setPixelColor(i, Wheel((j) & 255));
//        strip.show();
//      } 
//      loop(); 
//        
//      for(k=0; k < 200; k++){                          // Do ten loops of the 'wait' and service loop routine inbetween. Total wait = 10 x 'wait'. This prevents sluggishness in the browser html front end menu.
//        if (mode_flag!=2){
//          return;
//        }                    //the mode has been changed - get outta here!
//        delay(wait);
//        loop();
//      }
//      loop();
//    }
//  }
}

void handle_mode_3(){                                //rainbow mode
  Serial.println(F("HandleMode3"));
  mode_flag = 3;
  EEPROM.write(4,mode_flag);                        //write mode to EEProm so can be restored on start-up
  EEPROM.commit();
  server.send ( 200, "text/html","<SCRIPT language='JavaScript'>window.location='/';</SCRIPT>");
//  uint16_t i, j, k;
//  int wait = 10;  //DON'T ever set this more than '10'. Use the 'k' value in the loop below to increase delays. This prevents the watchdog timer timing out on the ESP8266

  j=0;
  m=256*5;
  lastFire=0;
    
//  while(mode_flag==3){                               // do this indefenitely or until mode changes
//    for(j=0; j < 256*5; j++) {                        // 5 cycles of all colors on wheel
//      if (mode_flag!=3){
//        return;
//      }                    //the mode has been changed - get outta here!
//      loop();
//      for(i=0; i < NUMPIXELS; i++) {
//        loop();
//        strip.setPixelColor(i,Wheel(((i * 256 / NUMPIXELS) + j) & 255));
//        if (mode_flag!=3){
//          return;
//        }                    //the mode has been changed - get outta here!
//      }
//      strip.show();
//
//      for(k=0; k < 50; k++){                         // Do ten loops of the 'wait' and service loop routine inbetween. Total wait = 10 x 'wait'. This prevents sluggishness in the browser html front end menu.
//        if (mode_flag!=3){
//          return;
//        }                    //the mode has been changed - get outta here!
//        delay(wait);
//        loop();                    
//      }
//    }
//  }
}

void colorLoop() {
  int i;
  if (mode_flag==2) {
    if (millis()-lastFire>(wait*200)) {
      lastFire = millis();
      j++;
      Serial.print(F("mode 2 fire "));
      Serial.println(j);
      if (j>m) {
        j=0;
      } else {
        for(i=0; i<NUMPIXELS; i++) {
          if (mode_flag!=2){
            return;
          }
          strip.setPixelColor(i, Wheel((j) & 255));
          Serial.print(F("mode 2 show "));
          Serial.print(j);
          Serial.print(" ");
          Serial.println(i);
          strip.show();
        } 
      }
    }    
  }
  
//  while(mode_flag==2){
//    for(j=0; j<256; j++) {
//      loop();
//      for(i=0; i<NUMPIXELS; i++) {
//        if (mode_flag!=2){
//          return;
//        }                    //the mode has been changed - get outta here!
//        loop();
//        strip.setPixelColor(i, Wheel((j) & 255));
//        strip.show();
//      } 
//      loop(); 
//        
//      for(k=0; k < 200; k++){
//        if (mode_flag!=2){
//          return;
//        }
//        delay(wait);
//        loop();
//      }
//      loop();
//    }
//  }
    
  if (mode_flag==3) {
    if (millis()-lastFire>(wait*50)) {
      lastFire = millis();
      j++;
      Serial.print(F("mode 3 fire "));
      Serial.println(j);
      if (j>m) {
        j=0;
      } else {
        for(i=0; i < NUMPIXELS; i++) {
          if (mode_flag!=3){
            return;
          }
          strip.setPixelColor(i,Wheel(((i * 256 / NUMPIXELS) + j) & 255));
        }
        Serial.print(F("mode 3 show "));
        Serial.println(j);
        strip.show();      
      }
    }
  }

//    for(j=0; j < 256*5; j++) {                        // 5 cycles of all colors on wheel
//      if (mode_flag!=3){
//        return;
//      }                    //the mode has been changed - get outta here!
//      loop();
//      for(i=0; i < NUMPIXELS; i++) {
//        loop();
//        strip.setPixelColor(i,Wheel(((i * 256 / NUMPIXELS) + j) & 255));
//        if (mode_flag!=3){
//          return;
//        }                    //the mode has been changed - get outta here!
//      }
//      strip.show();
//
//      for(k=0; k < 50; k++){                         // Do ten loops of the 'wait' and service loop routine inbetween. Total wait = 10 x 'wait'. This prevents sluggishness in the browser html front end menu.
//        if (mode_flag!=3){
//          return;
//        }                    //the mode has been changed - get outta here!
//        delay(wait);
//        loop();                    
//      }
//    }
//  }
      
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
      }
      if (strcmp(tempBuf,"F")==0) {
        tempType='F';
      }
    }
  }
  
  if (millis() - lastTimeSent >= SENSOR_INTERVAL * 1000UL || lastTimeSent == 0) {

    digitalWrite(LED_PIN, LOW);

    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();  
    sensors.setWaitForConversion(true);

    float temp = -1000.0;
    if (tempType=='C') {
      Serial.println("Sending C");
      temp = sensors.getTempCByIndex(0);
    } 
    if (tempType=='F') {
      Serial.println("Sending F");
      temp = sensors.getTempFByIndex(0);
    }

    // Implements the 'protocol': a greater-then sign followed by value, followed by less-then sign.
    if (temp!=-1000.0) {
      Serial.print(">");
      Serial.print(temp);
      Serial.println("<");
    }
          
    digitalWrite(LED_PIN, HIGH);
    lastTimeSent = millis();
  }
}
