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
    uint16_t blockSize = min(WIDTH, HEIGHT);
    int16_t x = 0, y = 0, w = blockSize, h = blockSize;
    if (2 == whichBlock) {
      // blocks on top of each other
      y = blockSize;
    }
    
    if (1 == getRotation() || 3 == getRotation()) {
      // blocks next to each other
      swap(x, y);
    }

    drawRect(x+2, y+2, w-4, h-4, EPD_BLACK);

    if (th->dataValid) {
      // NOTE / TODO the fonts are modified for the . and : and space to use less width
      
      setFont(&FreeMonoBold18pt7b);
      setTextColor(EPD_BLACK);

      setCursor(x, y + 30);

      /*
      if (th->temperature < 0) {
        // the minus sign
        fillRect(x + 5, y + 20, 10, 4, EPD_BLACK);
      }
      printFloat(abs(th->temperature));
      */
      
      static char formatted[10];
      dtostrf(th->temperature, 5, 1, formatted);

      print(formatted);
      println('c');

      setCursor(x + 20, y + 64);
      printFloat(th->vaporPressure);
      println("p");
      
      setFont(&FreeMonoBold12pt7b);
      setCursor(x + 56, y + 96);
      printFloat(th->humidity);
      println('%');
    }

    setFont(&FreeMonoBold9pt7b);
    setCursor(x + 7, y + 120);
    
    print("v ");
    printFloat(th->volts + 0.05, 1); // + 0.05 = round
    print(" ");
    
    printTime(millisDiff);
  }

  void printFloat(float f, uint8_t decimals = 2)
  {
    static char formatter[6];
    sprintf(formatter, "%%%dd", decimals);
    static char formatted[12];
    
    sprintf(formatted, formatter, (int)f);
    print(formatted);
    print('.');
    sprintf(formatted, "%1d", (int)(10*f) % 10);
    print(formatted);
  }

  void printTime(unsigned long milli)
  {
    static char timeFormatted[10];

    long seconds = milli / 1000;
    int h = seconds / 3600;
    int m = (seconds / 60) % 60;
    int s = seconds % 60;

    if (h > 9)
      sprintf(timeFormatted, "h %4d", h);
    else if (h != 0)
      sprintf(timeFormatted, "h %1d:%02d", h, m);
    else
      sprintf(timeFormatted, "m%2d:%02d", m, s);
    print(timeFormatted);
  }
};

