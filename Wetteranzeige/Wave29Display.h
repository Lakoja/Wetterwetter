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

#include "TH.h"
#include "EpdDisplay.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

#include "CrcableData.h"

class DisplayStateWrapper : public CrcableData {
public:
  EpdDisplayState displayState;
};

class Wave29Display : public EpdDisplay 
{
private:
  DisplayStateWrapper state;
  
  uint16_t blockHeight = 124;

public:
  Wave29Display(bool operateAsync = false) :  
    EpdDisplay(EPD_2x9_DISPLAY_WIDTH, EPD_2x9_DISPLAY_HEIGHT, operateAsync)
  {
  }

  DisplayStateWrapper* getState()
  {
    state.displayState = *EpdDisplay::getState();
    return &state;
  }

  void initWave29(DisplayStateWrapper *state)
  {
    EpdDisplay::init(NULL != state ? &state->displayState : NULL);
  }

  void displayValues(TH *th, uint8_t whichBlock, unsigned long millisOn)
  {
    int16_t x = 0, y = 0, w = width(), h = blockHeight;
    if (2 == whichBlock) {
      y = blockHeight;
    }
    
    if (1 == getRotation()) {
      swap(x, y);
      swap(w, h);
    }

    fillRect(x, y, w, h, EPD_WHITE);
    drawRect(x+2, y+2, w-4, h-4, EPD_BLACK);

    if (th->dataValid) {
      setFont(&FreeMonoBold18pt7b);
      setTextColor(EPD_BLACK);
      
      setCursor(x + 16, y + 30);
  
      /* TODO use / consider single digits and negative
      static char formatted[8];
      dtostrf(th->temperature, 4, 1, formatted);
      println(formatted);
      dtostrf(th->humidity, 4, 1, formatted);
      println(formatted);
      */
      print(th->temperature, 1);
      println('c');
      print(th->vaporPressure, 1);
      println("p");
      setFont(&FreeMonoBold12pt7b);
      setCursor(x + 52, y + 96);
      print(th->humidity, 1);
      println('%');
    }

    setFont(&FreeMonoBold9pt7b);
    setCursor(x + 30, y + 122);
    print('t');
    static char formatted[12];
    dtostrf(millisOn / 1000.0f, 7, 0, formatted);
    println(formatted);

/*
      Serial.print("Showing time for ");
      Serial.print(whichBlock);
      Serial.print(" ");
      Serial.println(formatted);
      */
  }
};

