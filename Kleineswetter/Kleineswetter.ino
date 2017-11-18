/*
 * Copyright (C) 2017 Lakoja on github.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include "CrcableData.h"

Adafruit_BME280 bme(0);
bool bme_ok = false;

class SystemState : public CrcableData {
public:
  float lastTransmittedTemperatature = -100;
  float lastTransmittedHumidity = -100;
  unsigned short roundsWithoutTransmission = 0;
  unsigned long connectMillisCumulated = 0;
  unsigned short serverSleepSeconds = 60;
};

SystemState systemState;

unsigned long connectInvainThreshold = 8 * 60 * 1000L; // some minutes
unsigned int connectTryThreshold = 7000;
int shiftStartSeconds = 6;

void setup() {
  unsigned long systemStart = millis();
  
  Serial.begin(115200);
  Serial.println("Beginning"); // More importantly: move cursor to the beginning of the line

  float volts = analogRead(A0) / 1023.0;
  float realVolts = volts * 4.975; // measured for 300k and 82k bridge; is somewhat tweaked...

  if (realVolts < 3.2) {
    Serial.print("Do not start with too low battery: ");
    Serial.print(realVolts);
    Serial.println();

    ESP.deepSleep(45*60 * 1000000L); // sleep 45 minutes
  }

  SystemState systemStateFromRtc;
  bool systemStateValid = systemStateFromRtc.readFromRtc(0, sizeof(SystemState));
  if (!systemStateValid) {
    Serial.println("Did not get last system data ");
  } else {
    systemState = systemStateFromRtc;
  }

  if (!bme.begin()) { // i2c address here would be 0x76
    Serial.println("Could not start BME280.");
    ESP.deepSleep(6e6);

    // TODO this is a serious error
  } else {
    bme_ok = true;
  }

  // TODO get rid of dummy values
  float t = 49.7;
  float h = 76.5;

  if (bme_ok) {
    // delay(250); // only for i2c
    
    t = bme.readTemperature() - 1;
    h = bme.readHumidity();

    if (isnan(t) || isnan(h)) {
      delay(100);
      t = bme.readTemperature() - 1;
      h = bme.readHumidity();

      if (isnan(t) || isnan(h)) {
        Serial.println("Ooop Cannot get meaningful values..Sleeping");

        // Sleep only light (see below commented)?
        ESP.deepSleep(3e6);
      }
    }

    bme.setSampling(Adafruit_BME280::MODE_FORCED); // power off
    
    //*
    if (systemState.roundsWithoutTransmission < 5) {
      float vaporPressureLast = -100;
      if (systemState.lastTransmittedHumidity > 0) {
        vaporPressureLast = getVaporPressure(systemState.lastTransmittedTemperatature, systemState.lastTransmittedHumidity);
      }
      float vaporPressureNow = getVaporPressure(t, h);
      
      if (abs(systemState.lastTransmittedTemperatature - t) <= 0.5 
        && abs(vaporPressureLast - vaporPressureNow) <= 0.5) {

        Serial.print("T: ");
        Serial.print(t);
        Serial.print(" VP ");
        Serial.print(vaporPressureNow);
        Serial.print(" ");
        Serial.print("Do not connect. No change. Time passed since on ");
        Serial.println(millis() - systemStart);
    
        systemState.roundsWithoutTransmission++;
        systemState.writeToRtc(0, sizeof(SystemState));

        sleepNowForServer(-1, millis() - systemStart);
      } else {
        Serial.print("Connect because of change ");
        Serial.print(abs(systemState.lastTransmittedTemperatature - t));
        Serial.print(" ");
        Serial.println(abs(systemState.lastTransmittedHumidity - h));
      }
    } else {
      Serial.print("Connect anyway. Counter ");
      Serial.println(systemState.roundsWithoutTransmission);
    }
    //*/
  }

  // TODO consider system state/counters on all error sleeps!

  WiFi.mode(WIFI_STA);
  WiFi.begin("Wettergunde", "3ApCo_rtz_ppopp");

  unsigned long connectStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - connectStart > connectTryThreshold) {
      sleepNowForFailedConnect(5000L, now - systemStart);

      /*
      // This has 15mA instead of 70mA (above) or 1mA with deep sleep
      WiFi.disconnect(true);
      WiFi.setSleepMode(WIFI_MODEM_SLEEP);
      WiFi.forceSleepBegin();
      delay(5000);
      WiFi.forceSleepWake();
      connectStart = millis();
      WiFi.begin("Wettergunde", "3ApCo_rtz_ppopp");
      */
    }
    
    delay(200);
    Serial.print(".");
  }
  Serial.println(millis()-systemStart);

  WiFiClient client;
  if (!client.connect(IPAddress(192,168,122,1), 80)) {
    Serial.print("Server connect failed.. ");

    // TODO this the right sleep time for this case?
    sleepNowForFailedConnect(14000L, millis() - systemStart);
  }

  systemState.connectMillisCumulated = 0;
  systemState.writeToRtc(0, sizeof(SystemState));
  
  // NOTE/ TODO system start time must also be taken into account
  unsigned long connectionTime = millis();
  Serial.println("Server connected");

  client.setNoDelay(true);
  unsigned long waitForServerGreetingStart = millis();
  while(client.connected() && !client.available()) {
    if (millis() - waitForServerGreetingStart > 2000) {
      Serial.println("No greeting from server.. Sleeping");
      ESP.deepSleep(17e6, RF_NO_CAL);
    }
  }

  // TODO consider timeout on read?

  String greeting = client.readStringUntil('\n');

  short serverSecondsUntilOff = 5;
  
  // TODO use sleeping for these error cases?
  
  if (!greeting.startsWith("WS")) {
    Serial.print("Greeting from server bogus ");
    Serial.print(greeting);
    Serial.println(" Stopping");
    return;
  } else {
    int idx = greeting.indexOf('-');
    if (idx < 0) {
      systemState.serverSleepSeconds = greeting.substring(2).toInt();
    } else {
      systemState.serverSleepSeconds = greeting.substring(2, idx).toInt();
      serverSecondsUntilOff = greeting.substring(idx+1).toInt();

        Serial.print("Moving sleep time by ");
        Serial.println(serverSecondsUntilOff - shiftStartSeconds);
    }

    if (systemState.serverSleepSeconds < 0 || systemState.serverSleepSeconds > 600) {
      Serial.print("Bogus sleep seconds from server ");
      Serial.println(systemState.serverSleepSeconds);

      systemState.serverSleepSeconds = 60;
    }
  }


  static char numBuffer1[10];
  static char numBuffer2[10];
  static char numBuffer3[10];
  dtostrf(t, 4, 2, numBuffer1);
  dtostrf(h, 4, 2, numBuffer2);
  dtostrf(realVolts, 4, 2, numBuffer3);
  
  static char outBuffer[26];
  sprintf(outBuffer, "%s %s %s", numBuffer1, numBuffer2, numBuffer3);

  client.println(outBuffer);
  Serial.print("Sent ");
  Serial.println(outBuffer);

  systemState.lastTransmittedTemperatature = t;
  systemState.lastTransmittedHumidity = h;
  systemState.roundsWithoutTransmission = 0;

  systemState.writeToRtc(0, sizeof(SystemState));
  
/*
  unsigned long st = millis();
  while (client.available() == 0) {
    if (millis() - st > 8000) {
      Serial.println("No reply from server.");
      client.stop();
      return;
    }
    delay(200);
    Serial.print('.');
  }
  byte data;// buffer[1];
  client.read(&data, 1);
  Serial.print(data, DEC);
  Serial.println();

  //*/

  unsigned long systemOnMillis = (millis() - connectionTime) + (connectStart - systemStart);

  sleepNowForServer(serverSecondsUntilOff, systemOnMillis);
}

void loop() {

}

void sleepNowForFailedConnect(unsigned long sleepMillis, unsigned long millisSinceSystemStart)
{
  systemState.connectMillisCumulated += millisSinceSystemStart;

  if (systemState.connectMillisCumulated >= connectInvainThreshold) {
    systemState.connectMillisCumulated = 0;
    sleepMillis = 45 * 60 * 1000L; // sleep long
  }
  
  systemState.writeToRtc(0, sizeof(SystemState));

  Serial.print("Waiting ");
  Serial.println(sleepMillis);
  
  ESP.deepSleep(sleepMillis * 1000, RF_NO_CAL); // NOTE this must be taken plus the wait time above
}

void sleepNowForServer(short serverSecondsUntilOff, unsigned long systemOnMillis)
{
  unsigned long sleepMillis = 1;
  long correctionMillis = systemOnMillis;
  if (serverSecondsUntilOff >= 0)
    correctionMillis += (shiftStartSeconds - serverSecondsUntilOff) * 1000; // shift wake up time to the start of the window
    
  if (systemState.serverSleepSeconds*1000 > correctionMillis) {
    sleepMillis = systemState.serverSleepSeconds * 1000 - correctionMillis;
  } else {
    Serial.print("Do not adapt sleep millis ");
    Serial.print(systemState.serverSleepSeconds*1000);
    Serial.print(" ");
    Serial.print(correctionMillis);
    Serial.print(" ");
    Serial.print(serverSecondsUntilOff);
    Serial.print(" ");
  }
  Serial.print("Sleeping ");
  Serial.println(sleepMillis);
  
  ESP.deepSleep(sleepMillis * 1000, RF_NO_CAL);
}

float getVaporPressure(float temp, float humid)
{
  // The Magnus formula
  //
  
  float a = 7.5f;
  float b = 237.3f;

  if (temp < 0) {
    a = 7.6f;
    b = 240.7f;
  }
  
  float saturationVaporPressure = 6.1078f * pow(10, ((a*temp)/(b+temp)));

  return humid/100 * saturationVaporPressure;
}

