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

unsigned long systemSleepSeconds = 240; // including the awaked time
unsigned long systemAwakeSeconds = 14;

#include "TH.h"
#include "WeatherServer.h"
WeatherServer server(systemSleepSeconds);

#include <Adafruit_BME280.h>
Adafruit_BME280 bme(0);

#include "CrcableData.h"

class SystemState : public CrcableData {
public:
  float displayedInternalTemperature = -100;
  float displayedInternalHumidity = -100;
  uint8_t cyclesWithoutShow = 0;
  bool externalDataValid = false; // TODO use TH directly?
  float lastExternalTemperature = 0;
  float lastExternalHumidity = 0;
  float lastExternalVoltage = 0;
  unsigned long lastExternalValuesMillis = 0;
  unsigned long accumulatedSystemMillis = 0;
};

SystemState systemState;
TH localData;

bool bme_ok = false; // TODO superfluous

bool determineLocalTemperature = true; // TODO superfluous
bool startedWithPattern = false; // TODO superfluous
bool displayUpdated = false;
unsigned long systemStart = 0;

uint8_t cyclesWithoutUpdateThreshold = 5;

//ADC_MODE(ADC_VCC);

void setup()
{
  systemStart = millis();
  
  Serial.begin(115200);
  Serial.println(" Setup"); // Most importantly: move to a new line after boot message

  float volts = analogRead(A0) / 1023.0;
  float realVolts = volts * 4.975; // measured for 300k and 82k bridge; is somewhat tweaked...

  // TODO something to be done in the client; not trying to connect?
  
  if (realVolts < 3.1) {
    Serial.print("Do not start with too low battery: ");
    Serial.println(realVolts);

    ESP.deepSleep(45*60 * 1000000L); // sleep 45 minutes
  } else {
    Serial.print("Voltage: ");
    Serial.println(realVolts);
  }
  
  DisplayStateWrapper displayState;
  bool displayStateValid = displayState.readFromRtc(1, sizeof(DisplayStateWrapper));
    
  SystemState systemStateFromRtc;
  bool systemStateValid = systemStateFromRtc.readFromRtc(0, sizeof(SystemState));
  if (!systemStateValid) {
    Serial.println("Did not get last system data ");
  } else {
    systemState = systemStateFromRtc;
  }

  if (!bme.begin()) {
    Serial.println("No BME!");
  } else {
    bme_ok = true;
    delay(100); // Allow things to settle after begin()

    if (determineLocalTemperature) {
      determineLocalTemperature = false;
  
      localData.dataValid = false;
      
      if (bme_ok) {
        float t = bme.readTemperature() - 0.5;
        float h = bme.readHumidity();
  
        if (isnan(t) || isnan(h)) {
          Serial.println("First local temp read was invalid");
          delay(100);
          t = bme.readTemperature() - 0.5;
          h = bme.readHumidity();
        }
  
        bme.setSampling(Adafruit_BME280::MODE_FORCED); // power down
     
        if (!isnan(t) && !isnan(h) && t > -100 && t < 100) {
          localData.dataValid = true;
          localData.temperature = round(t * 5) / 5.0f;
          localData.humidity = round(h * 5) / 5.0f;
          localData.volts = realVolts;
  
          updateVaporPressure(&localData);
        }
      }
    }
  
    // TODO blank values if not up to date (> 5 minutes?)
  }
  
  //displayStateValid = false;

  // TODO / NOTE this will destroy communication with BME; thus temperature is alreay and finally read above
  
  display.initWave29(displayStateValid ? &displayState : NULL);
  display.setRotation(1);

  if (!displayStateValid) {
    Serial.println("Doing first display update with pattern");
    display.initFullMode();
    display.fillScreen(0x36); // a line pattern
    display.update();
  
    display.initPartialMode();

    display.fillScreen(0x1e); // a line pattern
    display.update();
    
    startedWithPattern = true;
    display.fillScreen(EPD_WHITE); // clear (all following operations only overwrite)

    if (startedWithPattern) {
      // Show something at the very first start
      TH invalidData;
      updateDisplay(&localData, &invalidData);
    }
  } else {
    display.initPartialMode();
    display.fillScreen(EPD_WHITE); // clear (all following operations only overwrite)
  }

  server.begin();

  /*
    TH local;
    local.dataValid = true;
    local.temperature = 23.0;
    local.humidity = 75.0;
    local.vaporPressure = 12.1;
    display.displayValues(&local, 1, 0);
    display.update();
    */
}

void loop()
{
  unsigned long loopStart = millis();

  unsigned long systemActive = millis() - systemStart;
  unsigned int secondsUntilOff = 0;
  if ((systemAwakeSeconds - 3)*1000 > systemActive) // -3 lie a little bit here; more time to connect
    secondsUntilOff = ((systemAwakeSeconds - 3)*1000 - systemActive) / 1000;

  TH externalData;
  bool dataReceived = server.receiveData(&externalData, secondsUntilOff);

  bool sleepNow = false;
  if (dataReceived && externalData.dataValid) {
    updateVaporPressure(&externalData);
    
    systemState.externalDataValid = true;
    systemState.lastExternalTemperature = externalData.temperature;
    systemState.lastExternalHumidity = externalData.humidity;
    systemState.lastExternalVoltage = externalData.volts;
    systemState.lastExternalValuesMillis = systemState.accumulatedSystemMillis + (millis() - systemStart);

    updateDisplay(&localData, &externalData);
    sleepNow = true;
  }
  
  bool shouldSleepNow = millis() - systemStart > systemAwakeSeconds * 1000;

  if (shouldSleepNow && !displayUpdated) {
    boolean internalIsToBeShown = true;
    /*
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
    }*/

    if (internalIsToBeShown) {
      if (systemState.externalDataValid) {
        externalData.dataValid = true;
        externalData.temperature = systemState.lastExternalTemperature;
        externalData.humidity = systemState.lastExternalHumidity;
        externalData.volts = systemState.lastExternalVoltage;
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

    systemState.accumulatedSystemMillis = systemState.accumulatedSystemMillis + systemActive + sleepMillis;
    Serial.print(systemState.accumulatedSystemMillis / (1000 * 60));
    
    Serial.print("m Sleeping ");
    if (shouldSleepNow && !sleepNow)
      Serial.print("in vain ");
    Serial.println(sleepMillis);

    DisplayStateWrapper *displayState = display.getState();
    displayState->writeToRtc(1, sizeof(DisplayStateWrapper));

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

