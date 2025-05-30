# OnStarduino

#### Designed to appear as a retro-styled radio, this is actually an Arduino-powered dashboard to display data about my Chevy Bolt EV. It can also send commands to start the car, unlock doors, and more!

![Retro-styled portable cassette player](/images/OnStarduino.jpg "OnStarduino")

- The tuner needle shows the battery level, and pulses if the car is charging.
- The VU meter displays the current charging voltage of 120, 240, or 400 volts.
- The counter resets when data is refreshed and increments each hour thereafter.
- The buttons for play/rewind/etc send commands to remote start the car, unlock doors, flash lights, etc.
- The tape spins when a command is in progress, and the activity LED will blink SOS if the command fails.
- The toggle button enables an SMS reminder if the car isn't plugged in at 9PM.
- The slider controls the speaker volume, the knob adjusts LED brightness.
- The antenna is for aesthetics only!

## Project Highlights

- Communicates with the OnStar API to send commands and receive diagnostics. A separate VoIP API is used to send SMS notifications.
- Pseudo-multitasking to allow concurrent tasks without a Real-Time OS (RTOS) on limited Arduino hardware.
- Several electro-mechanical features for inputs (keys and dials), outputs (LEDs and motors), and sensing (hall-effect rotary encoding).
- Designed in Fusion 360, 3D printed at home, assembled using mostly off-the-shelf electronics and one custom laser-cut aluminum part.

Special thanks to samrum for their work on the [OnStarJS](https://github.com/samrum/OnStarJS) library. It was instrumental in helping me understand the mechanics of the OnStar API.

## Minimal Install

:warning: **WARNING: This is an unoffical implementation of the OnStar API, use at your own risk.**

### Requirements

- An active [OnStar](https://www.onstar.com/) account (may require subscription) and associated vehicle.
- An active [VoIP.ms](https://voip.ms/) account with a Direct Inward Dial (DID) number that support SMS ($0.99/month + $0.0075/SMS)
- [Arduino Nano ESP32](https://store.arduino.cc/products/nano-esp32) or similar Single Board Computer (SBC) with the following features:
  - WiFi
  - EEPROM with 2kb
  - 16 GPIO pins (4 must support PWM)
- Installed libraries
  - [EEPROM](https://docs.arduino.cc/learn/built-in-libraries/eeprom/)
  - [ESP32Servo](https://madhephaestus.github.io/ESP32Servo/classServo.html)
  - [TimeLib](https://playground.arduino.cc/Code/Time/)
  - [WiFiClientSecure](https://github.com/pamelacece/WiFiClientSecure)
- Included `secrets.h` as described below

### secrets.h

This code relies on several pieces of sensitive data, and requires a `secrets.h` file:

```cpp
#define SECRET_WIFI_SSID "aaa" // WiFi network name
#define SECRET_WIFI_PASS "bbb" // WiFi network password
#define SECRET_SMS_USER "someone@example.com" // VoIP.ms username
#define SECRET_SMS_PASS "password1" // VoIP.ms API password (not account password)
#define SECRET_SMS_DID "1234567890" // VoIP.ms DID number
#define SECRET_SMS_DST "9876543210" // Phone number to receive SMS notifications
#define SECRET_VIN "ccc" // Vehicle Identification Number (VIN) of your vehicle
#define SECRET_DEVICE_ID "dddddddd-dddd-dddd-dddd-dddddddddddd" // Random Version 4 UUID from https://www.uuidgenerator.net/version4
#define SECRET_CLIENT_REQUEST_ID "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee" // Found in header of https request to custlogin.gm.com
#define SECRET_CLIENT_ID "ffffffff-ffff-ffff-ffff-ffffffffffff" // Found in body of https request to custlogin.gm.com
#define SECRET_REFRESH_TOKEN "ggg\0" // ~1650 character token found in https response from custlogin.gm.com
```

A tool such as [Burp Suite](https://portswigger.net/) can monitor network traffic between the official Chevrolet app and custlogin.gm.com in order to obtain the last three secrets. I believe the https request only occurs when a specific token expires after one hour, so you may need to wait for it to appear.![Screenshot of Burp Suite interface](/images/BurpSuite-OnStar.jpg)

### Testing the Code

The `setup()` function homes the tuner needle by moving the stepper motor until the stop switch is pressed. Therefore, the `setup()` function will never complete if the switch is not present. To test the code without components attached, remove the `while` loop from the STEPPER/STOP SWITCH SETUP section.

<br>
<br>

# More to come...

## Component Details

- Battery level
- Refresh counter
- Command keypad

## Assembly

- Parts list
- STLs for printing
- Wiring diagram
