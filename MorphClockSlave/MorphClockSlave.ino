#include <stdio.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS_PIN 14

const int LED_PIN = 2;

const int SENSOR_INTERVAL = 5;
unsigned long lastTimeSent = 0;

OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,HIGH);
  sensors.begin();  

  Serial.println(sensors.getDeviceCount());
}

bool useC = false;
bool isReading = false;
char tempBuf[10];

void loop() {

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
      if (strcmp(tempBuf,"C")==0) {
        useC = true;
      }
      if (strcmp(tempBuf,"F")==0) {
        useC = false;
      }
    }
  }
  
  if (millis() - lastTimeSent >= SENSOR_INTERVAL * 1000UL || lastTimeSent == 0) {

    digitalWrite(LED_PIN, LOW);

    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();  
    sensors.setWaitForConversion(true);

    float temp;
    if (useC==true) {
      Serial.println("Sending C");
      temp = sensors.getTempCByIndex(0);
    } else {
      Serial.println("Sending F");
      temp = sensors.getTempFByIndex(0);
    }

    // Implements the 'protocol': a greater-then sign followed by value, followed by less-then sign.
    Serial.print(">");
    Serial.print(temp);
    Serial.println("<");
          
    digitalWrite(LED_PIN, HIGH);
    lastTimeSent = millis();
  }
}
