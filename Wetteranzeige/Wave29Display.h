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
    // NOTE the +20 below assumes the 2.9 display with 128x296 pixels.
    
    int16_t startX = 0, startY = 0, w = blockSize, h = blockSize;
    if (2 == whichBlock) {
      // blocks on top of each other
      startY = blockSize + 20;
    }

    int8_t displayRotation = getRotation();
    setTextColor(EPD_BLACK);

    printText(startX, startY, 1 == whichBlock ? "Innen" : "Aussen");

    if (th->dataValid) {
      int16_t x = startX;
      int16_t y = startY;
      
      if (1 == displayRotation || 3 == displayRotation) {
        // blocks next to each other
        swap(x, y);
      } else {
        // battery to the right of the text
        x = WIDTH - 20;  
      }
      
      showBatteryLevel(x, y, th->volts);
    }

    int16_t x = startX;
    int16_t y = startY + 20;

    if (1 == displayRotation || 3 == displayRotation) {
      // blocks next to each other
      swap(x, y);
    }
    
    drawRect(x+2, y+2, w-4, h-4, EPD_BLACK);

    if (th->dataValid) {
      // NOTE / TODO the fonts are modified for the . and : and space to use less width

      setFont(&FreeMonoBold18pt7b);
      setCursor(x + 2, y + 32);

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

      setCursor(x + 22, y + 65);
      printFloat(th->vaporPressure);
      println("p");
      
      setFont(&FreeMonoBold12pt7b);
      setCursor(startX + 58, startY + 96);
      printFloat(th->humidity);
      println('%');
    }

    setFont(&FreeMonoBold9pt7b);
    
    /*
    setCursor(x + 7, y + 120);
    print("v ");
    printFloat(th->volts + 0.05, 1); // + 0.05 = round
    print(" ");
    */
    
    setCursor(x + 60, y + 120);
    printTime(millisDiff);
  }

  void printText(int16_t x, int16_t y, const char* text)
  {
    int8_t displayRotation = getRotation();
    
    setFont(&FreeMonoBold12pt7b);

    // The texts are always written in the smaller direction
    if (1 == displayRotation)
      setRotation(0);
    else if (3 == displayRotation)
      setRotation(2);
      
    setCursor(x + 4, y + 20 - 2);
    print(text);
    
    setRotation(displayRotation);
  }

  void showBatteryLevel(int16_t x, int16_t y, float volts)
  {
    int16_t batteryInnerWidth = 13;
    int16_t batteryInnerHeight = 5;
    
    // assume a linear range between 3.1 and 4.1 volts
    float batteryJuiceLeft = volts - 3.1;
    if (batteryJuiceLeft < 0)
      batteryJuiceLeft = 0;
    int16_t batteryFillWidth = (int16_t)((batteryJuiceLeft / 1.0) * batteryInnerWidth + 0.5);

    drawRect(x+2, y+4+1, 2, batteryInnerHeight, EPD_BLACK);
    drawRect(x+4, y+4, batteryInnerWidth+2, batteryInnerHeight+2, EPD_BLACK);
    fillRect(x+5+batteryInnerWidth-batteryFillWidth, y+4+1, batteryFillWidth, batteryInnerHeight, EPD_BLACK);
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

