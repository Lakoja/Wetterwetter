# Wetterwetter
A weather station based on two ESP8266 and an Waveshare e-paper display

Overview
--------
The server component including display driver can be found in *Wetteranzeige* and the client in *Kleineswetter*.

Dependencies
------------
The libraries Adafruit GFX, Adafruit BME280 (+ Adafruit Unified Sensor) and CRC32 are used.

Disclaimer
----------
Some drafts of the display driver and spi communication classes were initially loosely based on:
* The display library of ZinggJM: https://github.com/ZinggJM/GxEPD
* The demo code of Waveshare: http://www.waveshare.com/wiki/2.9inch_e-Paper_Module#Demo_code
