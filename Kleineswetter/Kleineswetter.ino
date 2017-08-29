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

Adafruit_BME280 bme;
bool bme_ok = false;

class SystemState : public CrcableData {
public:
  float lastTransmittedTemperatature = -100;
  float lastTransmittedHumidity = -100;
  unsigned short roundsWithoutTransmission = 0;
  unsigned short serverSleepSeconds = 60;
};

SystemState systemState;

void setup() {
  unsigned long startTime = millis();
  
  Serial.begin(115200);

  Serial.println("Beginning");

  SystemState systemStateFromRtc;
  bool systemStateValid = systemStateFromRtc.readFromRtc(0, sizeof(SystemState));
  if (!systemStateValid) {
    Serial.println("Did not get last system data ");
  } else {
    systemState = systemStateFromRtc;
  }

  if (!bme.begin(0x76)) {
    Serial.println("Could not start BME280.");
    ESP.deepSleep(6e6);

    // TODO this is a serious error
  } else {
    bme_ok = true;
  }
  
  randomSeed(analogRead(A0));
  float t = 49.7 + random(7);
  float h = 76.5 - random(5);

  if (bme_ok) {
    delay(200);
    t = bme.readTemperature() - 1;
    h = bme.readHumidity();

    if (isnan(t) || isnan(h)) {
      delay(10);
      t = bme.readTemperature() - 1;
      h = bme.readHumidity();

      if (isnan(t) || isnan(h)) {
        Serial.println("Ooop Cannot get meaningful values..Sleeping");
        ESP.deepSleep(3e6);
      }
    }
    
    Serial.print("T ");
    Serial.print(t);
    Serial.print(" H ");
    Serial.println(h);
    
    //*
    if (systemState.roundsWithoutTransmission < 5) {
      float vaporPressureLast = -100;
      if (systemState.lastTransmittedHumidity > 0) {
        vaporPressureLast = getVaporPressure(systemState.lastTransmittedTemperatature, systemState.lastTransmittedHumidity);
      }
      float vaporPressureNow = getVaporPressure(t, h);
      
      if (abs(systemState.lastTransmittedTemperatature - t) <= 0.5 
        && abs(vaporPressureLast - vaporPressureNow) <= 0.5) {

        Serial.print("Do not connect. No change. Time passed since on ");
        Serial.println(millis() - startTime);
    
        systemState.roundsWithoutTransmission++;
        systemState.writeToRtc(0, sizeof(SystemState));
    
        sleepNowForServer(8, startTime);
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
    if (millis() - connectStart > 10000) {
      Serial.println("Cannot connect to WiFi .. Sleeping");
      ESP.deepSleep(6e6, RF_NO_CAL); // NOTE this must be taken plus the wait time above

      // TODO differentiate between successful and unsuccessful sleep time
    }
    
    delay(200);
    Serial.print(".");
  }
  Serial.print("Wifi Connected took ");
  Serial.println(millis()-startTime);

  WiFiClient client;
  if (!client.connect(IPAddress(192,168,122,1), 80)) {
    Serial.println("Server connect failed.. Sleeping");
    ESP.deepSleep(14e6, RF_NO_CAL);
  }
  
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
        Serial.println(serverSecondsUntilOff - 8);
    }

    if (systemState.serverSleepSeconds < 0 || systemState.serverSleepSeconds > 600) {
      Serial.print("Bogus sleep seconds from server ");
      Serial.println(systemState.serverSleepSeconds);

      systemState.serverSleepSeconds = 60;
    }
  }


  static char numBuffer1[10];
  static char numBuffer2[10];
  dtostrf(t, 4, 2, numBuffer1);
  dtostrf(h, 4, 2, numBuffer2);
  static char outBuffer[22];
  sprintf(outBuffer, "%s %s", numBuffer1, numBuffer2);

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

  sleepNowForServer(serverSecondsUntilOff, connectionTime);
}

void loop() {

}

void sleepNowForServer(short serverSecondsUntilOff, unsigned long connectionTime)
{
  unsigned long sleepMillis = 1;
  // TODO also check the serverSecondsUntilOff in this if
  if (systemState.serverSleepSeconds*1000 > millis() - connectionTime) {
    sleepMillis = systemState.serverSleepSeconds * 1000 - (millis() - connectionTime);
    //Serial.print("Correcting current sleep time ");
    //Serial.println(sleepMillis);
    sleepMillis += (serverSecondsUntilOff - 8) * 1000; // shift wake up time to the start of the window
    //Serial.print("To current sleep time ");
    //Serial.println(sleepMillis);
    // TODO consider passed time until here for this
  }
  Serial.print("Sleeping ");
  Serial.println(sleepMillis);
  ESP.deepSleep(sleepMillis * 1000, RF_NO_CAL);
}

float getVaporPressure(float temp, float humid)
{
  // The Magnus formula (T >= 0)
  // TODO < 0 a = 7.6, b = 240.7 für T < 0 über Wasser (Taupunkt)
  
  float saturationVaporPressure = 6.1078f * pow(10, ((7.5f*temp)/(237.3f+temp)));
  return humid/100 * saturationVaporPressure;
}

