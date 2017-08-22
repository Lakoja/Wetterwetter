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
 
#include <CRC32.h>

#include "Wave29Display.h"
Wave29Display display;

unsigned long systemSleepSeconds = 60; // including the awaked time
unsigned long systemAwakeSeconds = 10;

#include "TH.h"
#include "WeatherServer.h"
WeatherServer server(systemSleepSeconds);

#include <Adafruit_BME280.h>
Adafruit_BME280 bme(0);
bool bme_ok = false;

unsigned long lastLocalUpdate = 0;
unsigned long lastRemoteUpdate = 0;
unsigned long systemStart = 0;

unsigned long tempUpdateThreshold = 6000L;

class SystemState {
public:
  uint32_t crc32;
  bool externalDataValid = false;
  float lastExternalTemperature = 0;
  float lastExternalHumidity = 0;
  unsigned long lastExternalValuesMillis = 0;
  unsigned long accumulatedSystemMillis = 0;
};

SystemState systemState;

void setup()
{
  systemStart = millis();
  
  Serial.begin(115200);
  Serial.println("Setup");
  
  DisplayState displayState;
  bool displayStateValid = readFromRtc(1, (uint32_t*)&displayState, sizeof(DisplayState));
  if (displayStateValid)
    displayState.isInitialized = false; //TODO ?
    
  SystemState systemStateFromRtc;
  bool systemStateValid = readFromRtc(0, (uint32_t*)&systemStateFromRtc, sizeof(SystemState));
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

    Serial.print("Server and BME setup ");
    Serial.print(m1-m0);
    Serial.print(" + ");
    Serial.println(m2-m1);
}

void loop()
{
  /*
  while (Serial.available()) {
    static byte buffer[3];
    Serial.readBytes(buffer, 2);
    Serial.println(buffer[0], DEC);
  }
  //*/

  unsigned long nowLoopStart = millis();
  if (lastLocalUpdate == 0 || nowLoopStart - lastLocalUpdate >= tempUpdateThreshold) {
    uint32_t m0 = millis();

    // TODO temperature outside of if - shouldn't be too often though...
    TH thOne;
    thOne.dataValid = false;
    thOne.temperature = 54.0;
    thOne.humidity = 231.2;
    
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
        thOne.dataValid = true;
        thOne.temperature = round(t * 5) / 5.0f;
        thOne.humidity = round(h * 5) / 5.0f;

        updateVaporPressure(&thOne);
      } else {
        thOne.dataValid = false;
      }
    }

    uint32_t m1 = millis();
    display.displayValues(&thOne, 1, systemState.accumulatedSystemMillis + (millis() - systemStart));
    uint32_t m2 = millis();

    Serial.print("Block 1 read and update ");
    Serial.print(m1-m0);
    Serial.print(" + ");
    Serial.println(m2-m1);

    lastLocalUpdate = millis();  
  }

  // TODO blank values if not up to date (> 5 minutes?)

    uint32_t m1 = millis();

  unsigned long systemActive = millis() - systemStart;
  unsigned int secondsUntilOff = 0;
  if (10*1000 > systemActive)
    secondsUntilOff = (10*1000 - systemActive) / 1000;

  TH externalData;
  bool dataReceived = server.receiveData(&externalData, secondsUntilOff);

  if (dataReceived && externalData.dataValid) {
    updateVaporPressure(&externalData);

    Serial.print("Setting external data in system state ");
    Serial.print(externalData.humidity);
    
    systemState.externalDataValid = true;
    systemState.lastExternalTemperature = externalData.temperature;
    systemState.lastExternalHumidity = externalData.humidity;
    systemState.lastExternalValuesMillis = systemState.accumulatedSystemMillis + (millis() - systemStart);

    Serial.print(" ");
    Serial.println(systemState.lastExternalValuesMillis);
  } else if (systemState.externalDataValid) {
    // TODO consider time somehow
    
    externalData.dataValid = true;
    externalData.temperature = systemState.lastExternalTemperature;
    externalData.humidity = systemState.lastExternalHumidity;
    updateVaporPressure(&externalData);
  }

    uint32_t m2 = millis();

/*
    Serial.print("c");
    Serial.print(m2-m1);
    */
    
  unsigned long nowLoopMiddle = millis();

  // TODO consider age of shown data vs tempUpdateThreshold
  if (externalData.dataValid && (lastRemoteUpdate == 0 || dataReceived)) { //nowLoopMiddle - lastRemoteUpdate >= tempUpdateThreshold)) {
    Serial.print("Showing external data ");
    Serial.print(externalData.humidity);
    Serial.print(" ");
    Serial.println(systemState.lastExternalValuesMillis);

    display.displayValues(&externalData, 2, systemState.lastExternalValuesMillis);
    lastRemoteUpdate = millis();
    
    // TODO? should be there after reset
    //systemState.lastExternalData.dataValid = false; // Wait for new data
  } else if (dataReceived) {
    Serial.print("Do not show external data ");
    Serial.print(externalData.dataValid);
    Serial.print(" ");
    Serial.print(lastRemoteUpdate);
    Serial.print(" ");
    Serial.println(nowLoopMiddle);
  }

  unsigned long nowLoopEnd = millis();
  if (nowLoopEnd - systemStart > systemAwakeSeconds * 1000) {

/*
    systemState2.accumulatedSystemMillis++;
    writeToRtc(3, (uint32_t*) &systemState2, sizeof(SystemState));
    */
  
    unsigned long systemActive = nowLoopEnd - systemStart;
    
    unsigned long sleepMillis = 1;
    if (systemSleepSeconds * 1000 > systemActive)
      sleepMillis = systemSleepSeconds * 1000 - systemActive;
    Serial.print("Sleeping ");
    Serial.println(sleepMillis);

    DisplayState *displayState = display.getState();
    writeToRtc(1, (uint32_t*)displayState, sizeof(DisplayState));


    systemState.accumulatedSystemMillis = systemState.accumulatedSystemMillis + systemActive + sleepMillis;
  
    writeToRtc(0, (uint32_t*)&systemState, sizeof(SystemState));
    
    ESP.deepSleep(sleepMillis * 1000);
  }

  unsigned long loopTook = nowLoopEnd - nowLoopStart;
  if (loopTook < 100) {
    delay(100 - loopTook);
  } else {
    delay(1); // yield
  }
}

void updateVaporPressure(TH *th)
{
  // The Magnus formula (T >= 0)
  // TODO < 0 a = 7.6, b = 240.7 für T < 0 über Wasser (Taupunkt)
  
  float saturationVaporPressure = 6.1078f * pow(10, ((7.5f*th->temperature)/(237.3f+th->temperature)));

  float vaporPressure = th->humidity/100 * saturationVaporPressure;

  th->vaporPressure = vaporPressure;
}

bool readFromRtc(uint16_t slot, uint32_t *data, uint16_t structSize)
{
  uint32_t offset = slot * 64;
  
  bool readOk = ESP.rtcUserMemoryRead(offset, data, structSize);

  if (!readOk)
    Serial.println("!! Reading RTC failed");
  
  uint32_t crcOfRead = CRC32::calculate(((uint8_t *)data) + 4, structSize - 4);

/*
  if (slot > 1) {
    Serial.print("Reading state ");
  uint8_t *x = (uint8_t*)data;
  for (int i=0; i<structSize; i++) {
    Serial.print(x[i], HEX);
    Serial.print('_');
  }
  Serial.print(" Check value ");
  Serial.println(crcOfRead, HEX);
  }
  */
  
  return data[0] == crcOfRead;
}

void writeToRtc(uint16_t slot, uint32_t *data, uint16_t structSize)
{
  uint32_t crcBeforeWrite = CRC32::calculate(((uint8_t *)data) + 4, structSize - 4);

  data[0] = crcBeforeWrite;

/*
  if (slot > 1) {
    Serial.print("Writing state ");
  uint8_t *x = (uint8_t*)data;
  for (int i=0; i<structSize; i++) {
    Serial.print(x[i], HEX);
    Serial.print('_');
  }
  Serial.print(" Check value ");
  Serial.println(crcBeforeWrite, HEX);
  }
  */
  
  uint32_t offset = slot * 64;
  bool writeOk = ESP.rtcUserMemoryWrite(offset, data, structSize);

  if (!writeOk)
    Serial.println("!! Writing RTC failed");
}

