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
unsigned long systemAwakeSeconds = 12;

#include "TH.h"
#include "WeatherServer.h"
WeatherServer server(systemSleepSeconds);

#include <Adafruit_BME280.h>
Adafruit_BME280 bme(0);
bool bme_ok = false;

bool determineLocalTemperature = true;
bool startedWithPattern = false;
bool displayUpdated = false;
unsigned long systemStart = 0;

uint8_t cyclesWithoutUpdateThreshold = 5;

#include "CrcableData.h"

class SystemState : public CrcableData {
public:
  float displayedInternalTemperature = -100;
  float displayedInternalHumidity = -100;
  uint8_t cyclesWithoutShow = 0;
  bool externalDataValid = false;
  float lastExternalTemperature = 0;
  float lastExternalHumidity = 0;
  unsigned long lastExternalValuesMillis = 0;
  unsigned long accumulatedSystemMillis = 0;
};

SystemState systemState;
TH localData;

//ADC_MODE(ADC_VCC);

void setup()
{
  systemStart = millis();
  
  Serial.begin(115200);
  Serial.println(" Setup"); // Most importantly: move to a new line after boot message

/*
  Serial.print("Chip id ");
  Serial.print(ESP.getChipId());
  Serial.print(" Flash size ");
  Serial.print(ESP.getFlashChipRealSize());
  Serial.print(" Vcc ");
  Serial.println(ESP.getVcc());
  */
  
  DisplayStateWrapper displayState;
  bool displayStateValid = displayState.readFromRtc(1, sizeof(DisplayStateWrapper));
    
  SystemState systemStateFromRtc;
  bool systemStateValid = systemStateFromRtc.readFromRtc(0, sizeof(SystemState));
  if (!systemStateValid) {
    Serial.println("Did not get last system data ");
  } else {
    systemState = systemStateFromRtc;
  }

  display.initWave29(displayStateValid ? &displayState : NULL);
  display.setRotation(1);

  if (!displayStateValid) {
    Serial.println("Doing first display update with pattern");
    display.initFullMode();
    display.fillScreen(0x36); // a line pattern
    display.update();
    display.initPartialMode();
    startedWithPattern = true;
  } else {
    display.initPartialMode();
    display.fillScreen(EPD_WHITE);
  }

  server.begin();
  
  if (!bme.begin()) {
    Serial.println("No BME!");
  } else {
    bme_ok = true;
    delay(100); // Allow things to settle after begin()
  }
}

void loop()
{
  unsigned long loopStart = millis();
  
  if (determineLocalTemperature) {
    determineLocalTemperature = false;

    localData.dataValid = false;
    
    if (bme_ok) {
      float t = bme.readTemperature() - 1;
      float h = bme.readHumidity();

      if (isnan(t) || isnan(h)) {
        Serial.println("First local temp read was invalid");
        delay(100);
        t = bme.readTemperature() - 1;
        h = bme.readHumidity();
      }
   
      if (!isnan(t) && !isnan(h) && t > -100 && t < 100) {
        localData.dataValid = true;
        localData.temperature = round(t * 5) / 5.0f;
        localData.humidity = round(h * 5) / 5.0f;

        updateVaporPressure(&localData);

        if (startedWithPattern) {
          // Show something at the very first start
          TH invalidData;
          updateDisplay(&localData, &invalidData);
        }
      } else {
        localData.dataValid = false;
      }
    }
  }

  // TODO blank values if not up to date (> 5 minutes?)

  unsigned long systemActive = millis() - systemStart;
  unsigned int secondsUntilOff = 0;
  if ((systemAwakeSeconds - 2)*1000 > systemActive) // -2 lie a little bit here; more time to connect
    secondsUntilOff = ((systemAwakeSeconds - 2)*1000 - systemActive) / 1000;

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
    boolean internalIsToBeShown = systemState.displayedInternalTemperature == -100;

    if (!internalIsToBeShown) {
      if (systemState.cyclesWithoutShow > cyclesWithoutUpdateThreshold) {
        Serial.println("Update after no shows");
        internalIsToBeShown = true;
      } else {
        TH lastInternalData;
        lastInternalData.temperature = systemState.displayedInternalTemperature;
        lastInternalData.humidity = systemState.displayedInternalHumidity;
        updateVaporPressure(&lastInternalData);
  
        if (abs(systemState.displayedInternalTemperature - localData.temperature) > 0.21
            || abs(lastInternalData.vaporPressure - localData.vaporPressure) > 0.29) {
            internalIsToBeShown = true;
  
            Serial.print("Updating because of difference ");
            Serial.println(abs(systemState.displayedInternalTemperature - localData.temperature));
        } else {
          Serial.println("No update. No external data and no internal change.");
          // NOTE the "update at least every X tries" is handled indirectly by the external source
        }
      }
    }

    if (internalIsToBeShown) {
      if (systemState.externalDataValid) {
        externalData.dataValid = true;
        externalData.temperature = systemState.lastExternalTemperature;
        externalData.humidity = systemState.lastExternalHumidity;
        updateVaporPressure(&externalData);
      }
      
      updateDisplay(&localData, &externalData);
    } else {
      systemState.cyclesWithoutShow++;
    }
  }

  unsigned long loopEnd = millis();

  // TODO also use a time threshold for update need
  if (sleepNow || shouldSleepNow) {
    unsigned long systemActive = loopEnd - systemStart;
    
    unsigned long sleepMillis = 1;
    if (systemSleepSeconds * 1000 > systemActive)
      sleepMillis = systemSleepSeconds * 1000 - systemActive;
    
    Serial.print("Sleeping ");
    if (shouldSleepNow && !sleepNow)
      Serial.print("in vain ");
    Serial.println(sleepMillis);

    DisplayStateWrapper *displayState = display.getState();
    displayState->writeToRtc(1, sizeof(DisplayStateWrapper));

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
  unsigned long totalOnMillis = systemState.accumulatedSystemMillis + (millis() - systemStart);

  // TODO check for validity of internal (or external) data?
  
  display.displayValues(local, 1, totalOnMillis);
  display.displayValues(external, 2, totalOnMillis - systemState.lastExternalValuesMillis);

  systemState.displayedInternalTemperature = local->temperature;
  systemState.displayedInternalHumidity = local->humidity;
  
  // TODO consider age of shown data vs tempUpdateThreshold

  display.updatePartOrFull();
  displayUpdated = true;
  systemState.cyclesWithoutShow = 0;
}

void updateVaporPressure(TH *th)
{
  // The Magnus formula
  //
  
  float a = 7.5f;
  float b = 237.3f;

  if (th->temperature < 0) {
    a = 7.6f;
    b = 240.7f;
  }
  
  float saturationVaporPressure = 6.1078f * pow(10, ((a*th->temperature)/(b+th->temperature)));

  float vaporPressure = th->humidity/100 * saturationVaporPressure;

  th->vaporPressure = vaporPressure;
}

