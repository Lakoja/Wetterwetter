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

#ifndef _SpiLine_H_
#define _SpiLine_H_

#define SPI_DEFAULT_FREQUENCY 16000000

#include <SPI.h>

class SpiLine
{
private:
  SPIClass& spiSink;
  int8_t selectPin, dataCommandPin, resetPin;
  
public:
  SpiLine(SPIClass& spi, int8_t cs, int8_t dc, int8_t rst) : spiSink(spi)
  {
    // TODO selectPin cannot be undefined (<=0)
    selectPin = cs;
    dataCommandPin = dc;
    resetPin = rst;
  }
  
  void reset()
  {
    if (resetPin >= 0)
    {
      digitalWrite(resetPin, LOW);
      delay(40);
      digitalWrite(resetPin, HIGH);
      delay(40);
    }
  }
  
  void init(uint32_t freq = SPI_DEFAULT_FREQUENCY)
  {
    if (selectPin >= 0)
    {
      digitalWrite(selectPin, HIGH);
      pinMode(selectPin, OUTPUT);
    }
    if (dataCommandPin >= 0)
    {
      digitalWrite(dataCommandPin, HIGH);
      pinMode(dataCommandPin, OUTPUT);
    }
    if (resetPin >= 0)
    {
      digitalWrite(resetPin, HIGH);
      pinMode(resetPin, OUTPUT);
    }
    reset();
    
    spiSink.begin();
    spiSink.setDataMode(SPI_MODE0);
    spiSink.setBitOrder(MSBFIRST);
    spiSink.setFrequency(freq);
  }
  void writeCommandTransaction(uint8_t c)
  {
    if (dataCommandPin >= 0) digitalWrite(dataCommandPin, LOW);
    if (selectPin >= 0) digitalWrite(selectPin, LOW);
    spiSink.transfer(c);
    if (selectPin >= 0) digitalWrite(selectPin, HIGH);
    if (dataCommandPin >= 0) digitalWrite(dataCommandPin, HIGH);
  }
  
  void writeDataTransaction(uint8_t d)
  {
    if (selectPin >= 0) digitalWrite(selectPin, LOW);
    spiSink.transfer(d);
    if (selectPin >= 0) digitalWrite(selectPin, HIGH);
  }
  
  void writeCommand(uint8_t c)
  {
    if (dataCommandPin >= 0) digitalWrite(dataCommandPin, LOW);
    spiSink.transfer(c);
    if (dataCommandPin >= 0) digitalWrite(dataCommandPin, HIGH);
  }
  
  void writeData(uint8_t d)
  {
    spiSink.transfer(d);
  }
  
  void startTransaction()
  {
    if (selectPin >= 0) digitalWrite(selectPin, LOW);
  }
  
  void endTransaction()
  {
    if (selectPin >= 0) digitalWrite(selectPin, HIGH);
  }
};

#endif

