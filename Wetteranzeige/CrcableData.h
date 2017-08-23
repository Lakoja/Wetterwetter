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

#ifndef _CrcableData_H_
#define _CrcableData_H_
 
#include <CRC32.h>

class CrcableData {
private:
  uint32_t crc32;

public:
  bool readFromRtc(uint16_t slot)
  {
    uint32_t offset = slot * 64;
    uint32_t *data = (uint32_t*)this;
    uint16_t structSize = sizeof(this);
    
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
    
    return crc32 == crcOfRead;
  }
  
  void writeToRtc(uint16_t slot)
  {
    uint32_t offset = slot * 64;
    uint32_t *data = (uint32_t*)this;
    uint16_t structSize = sizeof(this);
    
    uint32_t crcBeforeWrite = CRC32::calculate(((uint8_t *)data) + 4, structSize - 4);
  
    crc32 = crcBeforeWrite;
  
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
    
    bool writeOk = ESP.rtcUserMemoryWrite(offset, data, structSize);
  
    if (!writeOk)
      Serial.println("!! Writing RTC failed");
  }
};

#endif

