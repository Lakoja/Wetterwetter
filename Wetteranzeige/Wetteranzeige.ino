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

#include "Wave29Display.h"
Wave29Display display;

unsigned long systemSleepSeconds = 180; // including the awaked time
unsigned long systemAwakeSeconds = 10;

#include "TH.h"
#include "WeatherServer.h"
WeatherServer server(systemSleepSeconds);

#include <Adafruit_BME280.h>
Adafruit_BME280 bme(0);
bool bme_ok = false;

bool determineLocalTemperature = true;
bool displayUpdated = false;
unsigned long systemStart = 0;

unsigned long tempUpdateThreshold = 6000L;

#include "CrcableData.h"

class SystemState : public CrcableData {
public:
  bool externalDataValid = false;
  float lastExternalTemperature = 0;
  float lastExternalHumidity = 0;
  unsigned long lastExternalValuesMillis = 0;
  unsigned long accumulatedSystemMillis = 0;
};

SystemState systemState;
TH localData;

void setup()
{
  systemStart = millis();
  
  Serial.begin(115200);
  Serial.println(" Setup"); // Most importantly: move to a new line after boot message
  
  DisplayState displayState;
  bool displayStateValid = displayState.readFromRtc(1, sizeof(DisplayState));
  if (displayStateValid)
    displayState.isInitialized = false; //TODO ?
    
  SystemState systemStateFromRtc;
  bool systemStateValid = systemStateFromRtc.readFromRtc(0, sizeof(SystemState));
  if (!systemStateValid) {
    Serial.println("Did not get last system data ");
  } else {
    systemState = systemStateFromRtc;
  }
    
  display.init(displayStateValid ? &displayState : NULL);
  display.setRotation(1);

  if (!displayStateValid) { // || !displayState.isInitialized) {
    Serial.println("Doing first display update with pattern");
    display.initFullMode();
    display.fillScreen(0x36); // a line pattern
    display.update();
    display.initPartialMode();
  } else {
    display.initPartialMode();
    display.fillScreen(EPD_WHITE);
  }

  uint32_t m0 = millis();

  server.begin();
  uint32_t m1 = millis();
  
  if (!bme.begin()) {
    Serial.println("No BME!");
  } else {
    bme_ok = true;
  }
    uint32_t m2 = millis();

    /*
    Serial.print("Server and BME setup ");
    Serial.print(m1-m0);
    Serial.print(" + ");
    Serial.println(m2-m1);
    */
}

void loop()
{
  unsigned long loopStart = millis();
  
  if (determineLocalTemperature) {
    determineLocalTemperature = false;

    localData.dataValid = false;
    localData.temperature = 54.0;
    localData.humidity = 231.2;
    
    if (bme_ok) {
      float t = bme.readTemperature() - 1;
      float h = bme.readHumidity();

      if (isnan(t) || isnan(h)) {
        delay(100);
        t = bme.readTemperature() - 1;
        h = bme.readHumidity();
      }

  /*
      Serial.print("Local ");
      Serial.print(t);
      Serial.print(' ');
      Serial.println(h);
  */    
      if (!isnan(t) && !isnan(h) && t > -100 && t < 100) {
        localData.dataValid = true;
        localData.temperature = round(t * 5) / 5.0f;
        localData.humidity = round(h * 5) / 5.0f;

        updateVaporPressure(&localData);
      } else {
        localData.dataValid = false;
      }
    }

    //display.displayValues(&thOne, 1, systemState.accumulatedSystemMillis + (millis() - systemStart));
  }

  // TODO blank values if not up to date (> 5 minutes?)

  unsigned long systemActive = millis() - systemStart;
  unsigned int secondsUntilOff = 0;
  if (10*1000 > systemActive)
    secondsUntilOff = (10*1000 - systemActive) / 1000;

  TH externalData;
  bool dataReceived = server.receiveData(&externalData, secondsUntilOff);

  bool sleepNow = false;
  if (dataReceived && externalData.dataValid) {
    updateVaporPressure(&externalData);
    
    systemState.externalDataValid = true;
    systemState.lastExternalTemperature = externalData.temperature;
    systemState.lastExternalHumidity = externalData.humidity;
    systemState.lastExternalValuesMillis = systemState.accumulatedSystemMillis + (millis() - systemStart);

    updateDisplay(&localData, &externalData);
    sleepNow = true;
  }
  
  bool shouldSleepNow = millis() - systemStart > systemAwakeSeconds * 1000;

  if (shouldSleepNow && !displayUpdated) {
    if (systemState.externalDataValid) {
      externalData.dataValid = true;
      externalData.temperature = systemState.lastExternalTemperature;
      externalData.humidity = systemState.lastExternalHumidity;
      updateVaporPressure(&externalData);
    }
    
    updateDisplay(&localData, &externalData);
  }

  unsigned long loopEnd = millis();
  
  if (sleepNow || shouldSleepNow) {
    unsigned long systemActive = loopEnd - systemStart;
    
    unsigned long sleepMillis = 1;
    if (systemSleepSeconds * 1000 > systemActive)
      sleepMillis = systemSleepSeconds * 1000 - systemActive;
    Serial.print("Sleeping ");
    Serial.println(sleepMillis);

    DisplayState *displayState = display.getState();
    displayState->writeToRtc(1, sizeof(DisplayState));

    systemState.accumulatedSystemMillis = systemState.accumulatedSystemMillis + systemActive + sleepMillis;
    systemState.writeToRtc(0, sizeof(SystemState));
    
    ESP.deepSleep(sleepMillis * 1000);
  }

  unsigned long loopTook = loopEnd - loopStart;
  if (loopTook < 100) {
    delay(100 - loopTook);
  } else {
    delay(1); // yield
  }
}

void updateDisplay(TH *local, TH *external)
{
  // TODO / NOTE the local time here is meaningless
  display.displayValues(local, 1, systemState.accumulatedSystemMillis + (millis() - systemStart));
  display.displayValues(external, 2, systemState.lastExternalValuesMillis);

  // TODO consider age of shown data vs tempUpdateThreshold

  display.updatePartOrFull();
  displayUpdated = true;
}

void updateVaporPressure(TH *th)
{
  // The Magnus formula (T >= 0)
  // TODO < 0 a = 7.6, b = 240.7 für T < 0 über Wasser (Taupunkt)
  
  float saturationVaporPressure = 6.1078f * pow(10, ((7.5f*th->temperature)/(237.3f+th->temperature)));

  float vaporPressure = th->humidity/100 * saturationVaporPressure;

  th->vaporPressure = vaporPressure;
}

