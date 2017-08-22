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

#include "Rect.h"
#include "TH.h"
#include "SpiLine.h"

#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

#define min(a,b) ((a)<(b)?(a):(b))

template <typename T> static inline void
swap(T& a, T& b)
{
  T t = a;
  a = b;
  b = t;
}

// mirror a pixel around a center line
static inline uint16_t mirror(uint16_t value, uint16_t maxi)
{
  // TODO check which is smaller?
  return maxi - value - 1;
}

#define D1 5
#define D2 4
#define D4 2

#define EPD_BLACK 0x0000
#define EPD_WHITE 0xFFFF

#define GDEH029A1_WIDTH 128
#define GDEH029A1_HEIGHT 296

#define GDEH029A1_BUFFER_SIZE (uint32_t(GDEH029A1_WIDTH) * uint32_t(GDEH029A1_HEIGHT) / 8)

#define GDEH029A1_LAST_X (GDEH029A1_WIDTH-1)
#define GDEH029A1_LAST_Y (GDEH029A1_HEIGHT-1)

#define CMD_DISPLAY_ACTIVATION 0x20
#define CMD_DISPLAY_UPDATE 0x22
#define CMD_PIXEL_DATA 0x24
#define CMD_WRITE_LUT 0x32
#define CMD_SET_RAM_X 0x44
#define CMD_SET_RAM_Y 0x45
#define CMD_SET_RAM_X_COUNTER 0x4e
#define CMD_SET_RAM_Y_COUNTER 0x4f
#define CMD_NOP_TERMINATE_WRITE 0xff

#define DATA_CLK_CP_OFF 0x03
#define DATA_CLK_CP_ON 0xc0
#define DATA_CLK_CP_ON_OFF 0xc3

const uint8_t LUTDefault_full[] =
{
  CMD_WRITE_LUT,
  0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 0x66, 0x69, 0x69, 0x59, 0x58, 0x99, 0x99,
  0x88, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xB4, 0x13, 0x51, 0x35, 0x51, 0x51, 0x19, 0x01, 0x00
};

const uint8_t LUTDefault_part[] =
{
  CMD_WRITE_LUT,
  0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x44, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t GDOControl[] = {0x01, GDEH029A1_LAST_Y % 256, GDEH029A1_LAST_Y / 256, 0x00}; //for 2.9inch
const uint8_t softstart[] = {0x0c, 0xd7, 0xd6, 0x9d};
const uint8_t VCOMVol[] = {0x2c, 0xa8};  // VCOM 7c
const uint8_t DummyLine[] = {0x3a, 0x1a}; // 4 dummy line per gate
const uint8_t Gatetime[] = {0x3b, 0x08};  // 2us per line
const uint8_t RamDataEntryMode[] = {0x11, 0x01};  // Ram data entry mode


SpiLine io(SPI, SS, D1, D4);

class DisplayState {
public:
  uint32_t crc32 = 0;
  uint32_t partialUpdateCount = 0;
  bool isInitialized = false;
  bool isFullMode = true;
  bool isPowerOff = true;
};

class Wave29Display : public Adafruit_GFX 
{
private:
  SpiLine& spiOutput;
  uint8_t busyPin;
  uint8_t pixelBuffer[GDEH029A1_BUFFER_SIZE];
  bool isSyncOperation = true;
  Rect changeRect;

  DisplayState state;

  // TODO value paint code must be moved to own (extension) class?
  uint16_t blockHeight = 124;
  uint8_t partialUpdateThreshold = 10;

public:
  Wave29Display(bool operateAsync = false) : 
    Adafruit_GFX(GDEH029A1_WIDTH, GDEH029A1_HEIGHT), spiOutput(io)
  {
    busyPin = D2;
    isSyncOperation = !operateAsync;
  }

  virtual void init(DisplayState *storedState)
  {
    if (storedState != NULL) {
      state = *storedState;
    }
    
    spiOutput.init(4000000);

    pinMode(busyPin, INPUT);

    if (!state.isInitialized) {
      initializeRegisters();
    }
  }

  void initFullMode()
  {
    if (state.isFullMode && state.isInitialized)
      return; // TODO PowerOn?

    state.isInitialized = true;
    state.isFullMode = true;

    //Serial.println("init full");

    // Having it here has some slight advantages (in update cleanliness)
    _SetAddresses(0x00, GDEH029A1_LAST_X, GDEH029A1_LAST_Y, 0x00); 

    // NOTE also works with LUTDefault_part here (only no full update then)
    writeCommandData(LUTDefault_full, sizeof(LUTDefault_full));
    _PowerOn();
  }

  void initPartialMode()
  {
    if (!state.isFullMode && state.isInitialized)
      return;

    state.isInitialized = true;
    state.isFullMode = false;

    //Serial.println("init part");
    
    changeRect.reset();

    writeCommandData(LUTDefault_part, sizeof(LUTDefault_part));
    _PowerOn();
  }

  DisplayState* getState()
  {
    return &state;
  }

  virtual void drawPixel(int16_t x, int16_t y, uint16_t color)
  {
    if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) 
      return;

    // TODO maybe a direction can be specified? in ram area
    switch (getRotation()) {
      case 1:
        swap(x, y);
        break;
      case 2:
        // this is the most natural for the display; esp. x-order is correct
        y = mirror(y, GDEH029A1_HEIGHT);
        break;
      default:
        Serial.println("!!! Rotation value not supported.");
    }

    changeRect.addPixel(x, y);

    uint16_t lineWidth = GDEH029A1_WIDTH / 8;
    uint16_t idx = x / 8 + y * lineWidth;
    byte value = color == EPD_BLACK ? 0 : 1;
    byte bitInByte = x % 8;

    byte currentData = pixelBuffer[idx];
    if (value != 0)
      currentData = currentData | (1 << (7 - bitInByte));
    else
      currentData = currentData & (0xFF ^ (1 << (7 - bitInByte)));

    pixelBuffer[idx] = currentData;
  }

  void displayValues(TH *th, uint8_t whichBlock, unsigned long millisOn)
  {
    int16_t x = 0, y = 0, w = GDEH029A1_WIDTH, h = blockHeight;
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
      
      setCursor(x + 16, y + 28);
  
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
      print(th->humidity, 1);
      println('%');
    }

    setFont(&FreeMonoBold9pt7b);
    setCursor(x + 30, y + 122);
    print('t');
    static char formatted[12];
    dtostrf(millisOn / 1000.0f, 7, 1, formatted);
    println(formatted);

/*
      Serial.print("Showing time for ");
      Serial.print(whichBlock);
      Serial.print(" ");
      Serial.println(formatted);
      */
    
    if (state.partialUpdateCount >= partialUpdateThreshold) {
      state.partialUpdateCount = 0;
      
      update();
      
      initFullMode();
      waitWhileBusy();
      
      update();
/*
      Serial.print("Full update for block ");
      Serial.println(whichBlock);
*/
      initPartialMode();
    } else {
      state.partialUpdateCount++;
      update();
    }
  }
  
  virtual void fillScreen(uint16_t color)
  {
    memset(pixelBuffer, (uint8_t)color, sizeof(pixelBuffer));
    changeRect.addPixel(0, 0);
    changeRect.addPixel(GDEH029A1_LAST_X, GDEH029A1_LAST_Y);
  }

  virtual void update()
  {
    updateWithRegion();
  }
  
  Rect* updateWithRegion()
  {
    if (!isSyncOperation && isBusy())
      return NULL;

    bool wasPartial = false;

    // TODO mixing reset/init and partial update (set ram pointers) toegether is very problematic
    if (true || state.isFullMode)
      showBuffer((uint8_t *)pixelBuffer, false);
    else if (changeRect.hasData()) {

      /*
      Serial.print(" Width and height ");
      Serial.print(changeRect.getWidth());
      Serial.print(" ");
      Serial.println(changeRect.getHeight());*/

      int16_t Xstart = changeRect.Xstart / 8;
      int16_t Xend = changeRect.Xend / 8;
      int16_t Ystart = changeRect.Ystart;
      int16_t Yend = changeRect.Yend;
      int16_t lineBytes = GDEH029A1_WIDTH / 8;

      int16_t bufferSize = (Xend - Xstart + 1) * (Yend - Ystart + 1);

      if (bufferSize > GDEH029A1_BUFFER_SIZE * 0.8) {
        showBuffer((uint8_t *)pixelBuffer, false);
      } else {
        wasPartial = true;
        
        uint8_t *partpixelBuffer = (uint8_t *)malloc(bufferSize);

        /*
        Serial.print(bufferSize);
        Serial.print(" Doing partial auto update ");
        Serial.print(Xstart);
        Serial.print(" ");
        Serial.print(Xend);
        Serial.print(" ");
        Serial.print(Ystart);
        Serial.print(" ");
        Serial.print(Yend);
        Serial.println();
        */

        int16_t idx2 = 0;
        for (int16_t y = Ystart; y <= Yend; y++) {
          for (int16_t x = Xstart; x <= Xend; x++) {
            int16_t idx1 = y * lineBytes + x;

            partpixelBuffer[idx2++] = pixelBuffer[idx1];
          }
        }

        // TODO
        int16_t y1 = mirror(changeRect.Yend, GDEH029A1_HEIGHT);
        int16_t y2 = mirror(changeRect.Ystart, GDEH029A1_HEIGHT);

        showBuffer(changeRect.Xstart, changeRect.Xend, y1, y2, partpixelBuffer, false);

        free(partpixelBuffer);
      }
    }

    changeRect.reset();
  }
  
  void onlyPowerOff()
  {
    _PowerOff();
  }

  bool isBusy()
  {
    return digitalRead(busyPin) == HIGH;
  }

private:

  bool first = true;

  void showBuffer(uint8_t *data, bool mono)
  {
    showBuffer(0, GDEH029A1_LAST_X, 0, GDEH029A1_LAST_Y, data, mono);
  }
  
  void showBuffer(uint8_t xStart, uint8_t xEnd,
      uint16_t yStart, uint16_t yEnd, uint8_t *data, bool mono)
  {
    if (state.isPowerOff) {
      _PowerOn();
    }

    if (!state.isFullMode)
      _SetAddresses(xStart, xEnd, yEnd, yStart);
    // else was set in init...

    uint32_t m0 = millis();
  
    writeDisplayData(xEnd-xStart+1, yEnd-yStart+1, data, mono);

    if (state.isFullMode)
      sendUpdateFullCommands();
    else
      sendUpdatePartCommands();

    if (isSyncOperation) {
      uint32_t m1 = millis();
      waitWhileBusy();
      uint32_t m2 = millis();

      if (true || first) {
        Serial.print(state.isFullMode ? "Full" : "Part");
        Serial.print(" update ");
        Serial.print(m1-m0);
        Serial.print(" + ");
        Serial.println(m2-m1);
  
        first = false;
      }
      
    }
    // TODO else
  
    if (!state.isFullMode) {
      // Disabling those enables a two screen flashing...
      //_SetAddresses(xStart, xEnd, yEnd, yStart);
      writeDisplayData(xEnd-xStart+1, yEnd-yStart+1, data, mono);
    }

    _PowerOff();
    waitWhileBusy();
  }

  void writeDisplayData(uint8_t XSize, uint16_t YSize, uint8_t *data, bool mono)
  {
    XSize = (XSize + 7)/8; // ceil
    
    waitWhileBusy(); // TODO important: Otherwise no clear before partial update
    
    writeCommand(CMD_PIXEL_DATA);
    
    for (uint16_t i=0; i<XSize; i++){
      for (uint16_t j=0; j<YSize; j++){
        writeData(*data);
  
        if (!mono)
          data++;
      }
    }
  }

  void _SetAddresses(uint16_t Xstart, uint16_t Xend, uint16_t Ystart, uint16_t Yend)
  {
    _SetRamArea(Xstart / 8, Xend / 8, Ystart, Yend);
    _SetRamPointer(Xstart / 8, Ystart);
  }
  
  void _SetRamArea(uint8_t Xstart, uint8_t Xend, uint16_t Ystart, uint16_t Yend)
  {
    writeCommand(CMD_SET_RAM_X);
    writeData(Xstart);
    writeData(Xend);
    writeCommand(CMD_SET_RAM_Y);
    writeData(Ystart % 256);
    writeData(Ystart / 256);
    writeData(Yend % 256);
    writeData(Yend / 256);
  }
  
  void _SetRamPointer(uint8_t addrX, uint16_t addrY)
  {
    writeCommand(CMD_SET_RAM_X_COUNTER);
    writeData(addrX);
    writeCommand(CMD_SET_RAM_Y_COUNTER);
    writeData(addrY % 256);
    writeData(addrY / 256);
  }
  
  void _PowerOn()
  {
    if (state.isPowerOff == false)
      Serial.println("!Wrong power on");
    
    writeCommand(CMD_DISPLAY_UPDATE);
    writeData(DATA_CLK_CP_ON);
    writeCommand(CMD_DISPLAY_ACTIVATION);

    state.isPowerOff = false;
  }
  
  void _PowerOff()
  {
    if (state.isPowerOff == true)
      Serial.println("!Wrong power off");
    
    writeCommand(CMD_DISPLAY_UPDATE);
    writeData(DATA_CLK_CP_OFF);
    writeCommand(CMD_DISPLAY_ACTIVATION);

    state.isPowerOff = true;
  }
  void initializeRegisters()
  {
    writeCommandData(GDOControl, sizeof(GDOControl));  // Pannel configuration, Gate selection
    writeCommandData(softstart, sizeof(softstart));  // X decrease, Y decrease
    writeCommandData(VCOMVol, sizeof(VCOMVol));    // VCOM setting
    writeCommandData(DummyLine, sizeof(DummyLine));  // dummy line per gate
    writeCommandData(Gatetime, sizeof(Gatetime));    // Gate time setting
    writeCommandData(RamDataEntryMode, sizeof(RamDataEntryMode));  // X increase, Y decrease
  }
  
  void sendUpdateFullCommands()
  {
    writeCommand(CMD_DISPLAY_UPDATE);
    writeData(0xc4); // TODO use c7?
    writeCommand(CMD_DISPLAY_ACTIVATION);
    writeCommand(CMD_NOP_TERMINATE_WRITE);
  }
  
  void sendUpdatePartCommands()
  {
    writeCommand(CMD_DISPLAY_UPDATE);
    writeData(0x04);
    writeCommand(CMD_DISPLAY_ACTIVATION);
    writeCommand(CMD_NOP_TERMINATE_WRITE);
  }
  
  void writeCommand(uint8_t command)
  {
    spiOutput.writeCommandTransaction(command);
  }
  
  void writeData(uint8_t data)
  {
    spiOutput.writeDataTransaction(data);
  }
  
  void writeCommandData(const uint8_t* pCommandData, uint8_t datalen)
  {
    spiOutput.startTransaction();
    spiOutput.writeCommand(*pCommandData++);
    for (uint8_t i = 0; i < datalen - 1; i++)
    {
      spiOutput.writeData(*pCommandData++);
    }
    spiOutput.endTransaction();
  }
  
  void waitWhileBusy()
  {
    for (uint16_t i=0; i<400; i++)
    {
      if (digitalRead(busyPin) == LOW) 
        break;
      delay(10);
    }
  }
  
};

