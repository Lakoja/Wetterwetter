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

#include <ESP8266WiFi.h>
#include "TH.h"

class WeatherServer : public WiFiServer
{
private:
  unsigned int sleepCycleSeconds;
  char welcomeBuffer[14];
  unsigned long waitForClientDataTimeout = 2000;

public:
  WeatherServer(unsigned int cycleSeconds) : WiFiServer(80)
  {
    sleepCycleSeconds = cycleSeconds;
  }

  void begin()
  {
    // TODO check two return values?
    
    WiFi.softAPConfig(IPAddress(192,168,122,1), IPAddress(192,168,122,254), IPAddress(255,255,255,0));
    WiFi.softAP("Wettergunde", "3ApCo_rtz_ppopp");
    delay(100);
    WiFiServer::begin();
    setNoDelay(true);

    /*
    Serial.print("Local server @ ");
    Serial.print(WiFi.localIP());
    Serial.print("/");
    Serial.println(WiFi.softAPIP());
  
    Serial.println("AP and server started");
    */
  }

  bool receiveData(TH *th, unsigned int secondsUntilOff)
  {
    // Check if a client has connected
    //Serial.print("R");
    WiFiClient client = available();
  
    if (client) {
      //Serial.print("+");
      // Wait until the client sends some data
      //Serial.println("New client");

      sprintf(welcomeBuffer, "WS%d-%d", sleepCycleSeconds, secondsUntilOff);
      client.println(welcomeBuffer);
      //Serial.print("o");
      
      unsigned long clientWaitStart = millis();
      while(client.connected() && !client.available()) {
        //Serial.print("c");
        if (millis() - clientWaitStart > waitForClientDataTimeout) {
          Serial.println("No data from client.");
          return false;
        }
        
        delay(5);
      }
      //Serial.print("k");
     
      // Read the request
      String data = client.readStringUntil('\n');

      //Serial.print("u");
      Serial.print("Got data ");
      Serial.print(data);
      Serial.println('_');


      // TODO this takes timeout long (5s)
      //client.print("OK");
      //delay(1);

      // client.stop();
  
      if (data.length() > 0) {
        int idx = data.indexOf(' ');
  
        if (idx > 0) {
          float t = data.substring(0, idx).toFloat();
          float h = data.substring(idx+1).toFloat();

          if (isnan(t) || isnan(h)) {
            Serial.println("Discarding bogus data");
            return false;
          }

          t = round(t * 5) / 5.0f;
          h = round(h * 5) / 5.0f;

          th->temperature = t;
          th->humidity = h;

          th->dataValid = true;
          return true;
        }
      }
    }

    return false;
  }
};

