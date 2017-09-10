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

  void displayValues(TH *th, uint8_t whichBlock, unsigned long millisDiff)
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
      // NOTE / TODO the fonts are modified for the . and : to use less width
      
      setFont(&FreeMonoBold18pt7b);
      setTextColor(EPD_BLACK);
      
      setCursor(x + 18, y + 30);

      //dtostrf(th->temperature, 4, 1, formatted);

      printFloat(th->temperature);
      println('c');
      
      printFloat(th->vaporPressure);
      println("p");
      
      setFont(&FreeMonoBold12pt7b);
      setCursor(x + 54, y + 96);
      printFloat(th->humidity);
      println('%');
    }

    setFont(&FreeMonoBold9pt7b);
    setCursor(x + 23, y + 122);
    printTime(millisDiff);
  }

  void printFloat(float f)
  {
      static char formatted[12];
      
      sprintf(formatted, "%2d", (int)f);
      print(formatted);
      print('.');
      sprintf(formatted, "%1d", (int)(10*f) % 10);
      print(formatted);
  }

  void printTime(unsigned long milli)
  {
    static char timeFormatted[12];

    long seconds = milli / 1000;
    int h = seconds / 3600;
    int m = (seconds / 60) % 60;
    int s = seconds % 60;

    if (h != 0)
      sprintf(timeFormatted, "h %4d:%02d", h, m);
    else
      sprintf(timeFormatted, "m %4d:%02d", m, s);
    print(timeFormatted);
  }
};

