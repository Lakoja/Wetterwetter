#include <Arduino.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>

Adafruit_BME280 bme;
bool bme_ok = false;

// TODO center in server awake window
// TODO save last temp and only connect when necessary

void setup() {
  Serial.begin(115200);

  Serial.println("Beginning");


  if (!bme.begin()) {
    Serial.println("Could not start BME280.");
  } else {
    bme_ok = true;
  }
  
  randomSeed(analogRead(A0));
  float t = 29.7 + random(7);
  float h = 76.5 - random(5);

  if (bme_ok) {
    delay(200);
    t = bme.readTemperature() - 1;
    h = bme.readHumidity();

    if (isnan(t) || isnan(h)) {
      delay(10);
      t = bme.readTemperature() - 1;
      h = bme.readHumidity();

      if (isnan(t) || isnan(h)) {
        Serial.println("Ooop Cannot get meaningful values..Sleeping");
        ESP.deepSleep(3 * 1000000L);
      }
    }
    
    Serial.print("T ");
    Serial.print(t);
    Serial.print(" H ");
    Serial.println(h);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin("Wettergunde", "3ApCo_rtz_ppopp");

  unsigned long connectStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - connectStart > 10000) {
      Serial.println("Cannot connect to WiFi .. Sleeping");
      ESP.deepSleep(6e6, RF_NO_CAL); // NOTE this must be taken plus the wait time above

      // TODO differentiate between successful and unsuccessful sleep time
    }
    
    delay(300);
    Serial.print(".");
  }
  Serial.print("Wifi Connected @");
  Serial.println(WiFi.localIP());


  // TODO use sleeping for these error cases?

  WiFiClient client;
  if (!client.connect(IPAddress(192,168,122,1), 80)) {
    Serial.println("Server connect failed.. Sleeping");
    ESP.deepSleep(14e6, RF_NO_CAL);
  }

  unsigned long connectionTime = millis();
  Serial.println("Server connected");

  client.setNoDelay(true);
  unsigned long waitForServerGreetingStart = millis();
  while(client.connected() && !client.available()) {
    if (millis() - waitForServerGreetingStart > 2000) {
      Serial.println("No greeting from server.. Sleeping");
      ESP.deepSleep(17e6, RF_NO_CAL);
    }
  }

  // TODO consider timeout on read?

  String greeting = client.readStringUntil('\n');

  short serverSleepSeconds = 60;
  short serverSecondsUntilOff = 5;
  
  if (!greeting.startsWith("WS")) {
    Serial.print("Greeting from server bogus ");
    Serial.print(greeting);
    Serial.println(" Stopping");
    return;
  } else {
    int idx = greeting.indexOf('-');
    if (idx < 0) {
      serverSleepSeconds = greeting.substring(2).toInt();
    } else {
      serverSleepSeconds = greeting.substring(2, idx).toInt();
      serverSecondsUntilOff = greeting.substring(idx+1).toInt();
    }

    if (serverSleepSeconds < 0 || serverSleepSeconds > 600) {
      Serial.print("Bogus sleep seconds from server ");
      Serial.println(serverSleepSeconds);

      serverSleepSeconds = 60;
    }
  }

  

  static char numBuffer1[10];
  static char numBuffer2[10];
  dtostrf(t, 4, 2, numBuffer1);
  dtostrf(h, 4, 2, numBuffer2);
  static char outBuffer[22];
  sprintf(outBuffer, "%s %s", numBuffer1, numBuffer2);

  client.println(outBuffer);
  Serial.print("Sent ");
  Serial.println(outBuffer);

/*
  unsigned long st = millis();
  while (client.available() == 0) {
    if (millis() - st > 8000) {
      Serial.println("No reply from server.");
      client.stop();
      return;
    }
    delay(200);
    Serial.print('.');
  }
  byte data;// buffer[1];
  client.read(&data, 1);
  Serial.print(data, DEC);
  Serial.println();

  //*/

  unsigned long sleepMillis = 1; 
  if (serverSleepSeconds*1000 > millis() - connectionTime) {
    sleepMillis = serverSleepSeconds * 1000 - (millis() - connectionTime);
    sleepMillis += serverSecondsUntilOff - 5; // shift wake up time in the middle
    // TODO consider passed time until here for this
  }
  Serial.print("Sleeping ");
  Serial.println(sleepMillis);
  ESP.deepSleep(sleepMillis * 1000, RF_NO_CAL);
}

void loop() {

}
