# OnStarduino

#### Designed to appear as a retro-styled radio, this Arduino-powered dashboard displays data about my Chevy Bolt EV and lets me remotely start the car, unlock doors, and more!

![Retro-styled portable cassette player](/images/OnStarduinoDemo.gif "OnStarduino")

- The glowing tuner needle shows the battery level, and pulses if the car is charging.
- VU meter displays the current charging voltage of 120, 240, or 400 volts.
- Counter resets when data is refreshed and increments each hour thereafter.
- Buttons for play/rewind/etc send commands to remote start the car, unlock doors, flash lights, etc.
- Tape spins when a command is in progress, and the activity LED will blink SOS if the command fails.
- Toggle button enables an SMS reminder if the car isn't plugged in at 9PM.
- Slider controls the speaker volume, the knob adjusts LED brightness.
- Antenna is for aesthetics only!

<br>

## Project Highlights

- Communicates with the OnStar API to send commands and receive diagnostics. A separate VoIP API is used to send SMS notifications.
- Pseudo-multitasking allows concurrent tasks without a Real-Time OS (RTOS) on limited Arduino hardware.
- Several electro-mechanical features for inputs (keys and dials), outputs (LEDs and motors), and sensing (hall-effect rotary encoding).
- Designed in Fusion 360, 3D printed at home, assembled using mostly off-the-shelf electronics and one custom laser-cut part.

Special thanks to samrum for their work on the [OnStarJS](https://github.com/samrum/OnStarJS) library. It was instrumental in helping me understand the mechanics of the OnStar API.

[Icon artwork by Noah Jacobus](https://www.svgrepo.com/collection/chunk-16px-thick-interface-icons/)

<br>

## Minimum Install Requirements

:warning: **WARNING: This is an unofficial implementation of the OnStar API, use at your own risk.**

- An active [OnStar Connect](https://www.onstar.com/plans/connect) subscription and associated vehicle.
- An active [VoIP.ms](https://voip.ms/) account with a Direct Inward Dial (DID) number that support SMS ($0.99/month + $0.0075/SMS)
- [Arduino Nano ESP32](https://store.arduino.cc/products/nano-esp32) or similar Single Board Computer (SBC) with the following features:
  - WiFi
  - EEPROM with 2kB
  - 16 GPIO pins (4 must support PWM)
- Installed libraries:
  - [EEPROM](https://docs.arduino.cc/learn/built-in-libraries/eeprom/)
  - [ESP32Servo](https://madhephaestus.github.io/ESP32Servo/classServo.html)
  - [TimeLib](https://playground.arduino.cc/Code/Time/)
  - [WiFiClientSecure](https://github.com/pamelacece/WiFiClientSecure)
- Included `secrets.h`:

```cpp
#define SECRET_WIFI_SSID "aaa" // WiFi network name
#define SECRET_WIFI_PASS "bbb" // WiFi network password
#define SECRET_SMS_USER "someone@example.com" // VoIP.ms username
#define SECRET_SMS_PASS "password1" // VoIP.ms API password (not account password)
#define SECRET_SMS_DID "1234567890" // VoIP.ms DID number
#define SECRET_SMS_DST "9876543210" // Phone number to receive SMS notifications
#define SECRET_VIN "ccc" // Vehicle Identification Number (VIN) of your vehicle
#define SECRET_VID "hhh" // Vehicle Id from GM
#define SECRET_DEVICE_ID "dddddddd-dddd-dddd-dddd-dddddddddddd" // Random Version 4 UUID from https://www.uuidgenerator.net/version4
#define SECRET_CLIENT_REQUEST_ID "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee" // Found in header of https request to custlogin.gm.com
#define SECRET_CLIENT_ID "ffffffff-ffff-ffff-ffff-ffffffffffff" // Found in body of https request to custlogin.gm.com
#define SECRET_REFRESH_TOKEN "ggg\0" // ~1650 character token found in https response from custlogin.gm.com
```

The last three secrets can be found by monitoring network traffic between the official Chevrolet app and custlogin.gm.com using a tool such as [Burp Suite](https://portswigger.net/). The critical request to custlogin.gm.com appears to occur only once an hour when the `subject_token` expires. Similarly, you can obtain your vehicle ID by monitoring traffic to eve-vcn.ext.gm.com.![Screenshot of Burp Suite interface](/images/BurpSuite-OnStar.jpg)

<br>

## Component Details

### Tuner/Battery Level

The tuner display is the easiest to read, even from across the room, so it shows the most important data: battery level. To move the tuner needle, a stepper motor rotates a pulley gear [1] attached to a belt, which moves the rail carriage [2] and the needle. The stop switch [3] is used to home the carriage at startup.![Render of tuner mechanism](/images/TunerRender.png)

Moving an actual, physical object takes time, even if it's just turning a gear a fraction of a degree. Since the CPU will always outrun the motor, it's important to program delays to allow the motor to catch up. The following code moves the needle to the 0% battery position during setup, and includes a `stepperCooldown` variable that can be tweaked as needed. It turns out 2ms is the lowest whole millisecond that works; 1ms just makes the motor vibrate in a disconcerting way.

```cpp
for (int f = 0; f < 220; f++) {
  stepOnce(1);
  delay(stepperCooldown);
}
```

<br>

### Cassette Tape/Command in Progress

The radio wouldn't be complete without a tape player. I wanted its movement related to the command keys in some way, and eventually decided the tape should spin to indicate that a command is being processed. When it stops, you're clear to send another command.

A mini motor in the back [1] spins the right wheel [2]. A thin belt [3] connects the right wheel to the left. The wheels are different sizes, making them spin at slightly different rates for a realistic effect. The clear plastic is harvested from an actual tape since it's nearly impossible to 3D print crystal-clear objects.![Render of tape mechanism](/images/TapeRender.png)

<br>

### Tape Counter/Refresh Counter

Next to the tape player you'll often find a tape counter. On this radio it represents how many hours have elapsed since the last data refresh. Technically it increments on the hour like a cuckoo clock because the motor is a bit loud and having it run at random times was unacceptable.

The mini motor [1] spins a cuff [2] attached to a shaft. Embedded magnets pass over a hall effect sensor [3] as the shaft turns the gears of the counter mechanism [4]. The servo arm [5] lifts to reset the counter.![Render of counter mechanism](/images/CounterRender.png)

To roll the counter forward one digit the shaft must spin about 500 degrees, which means 8 passes of a magnet is not quite enough (480), but 9 is too much (540). The following code stops the motor from spinning after 8 passes, but only if the `hoursPassed` _isn't_ divisible by 4. That way, every fourth hour 9 magnets are allowed to pass, catching up the counter.

```cpp
if ((hoursPassed % 4 != 0 && counterPosition == 8) || counterPosition == 9) {
  digitalWrite(counterMotorPin, LOW);
  hoursPassed++;
  counterPosition = 0;
  isNewHour = false;
}
```

<br>

### Playback Controls/Command Keypad

The command keypad mimics the appearance of tape playback buttons, and uses mechanical keyboard switches for a nice look and feel. I wanted six buttons but didn't want to consume six GPIO pins, so I used a technique [posted by IoT_hobbyist](https://forum.arduino.cc/t/analog-keypad-or-multiple-buttons-on-a-single-arduino-pins-code-example/931236) that allows a single pin to read multiple button inputs.![Render of keypad](/images/KeypadRender.png)

The challenge with using the single-pin technique is that it relies on analog readings of the keypad, which is often noisy. For example, pressing a key might typically output a value of about 990. But sometimes that same key might output a value of 650. To mitigate this I sample the keypad every 5 milliseconds, counting how many times the value falls within a range. Only when a count exceeds a threshold will an event trigger. That way, one or two erroneous readings won't accidentally start my car!

```cpp
if (keypadValue > 2010 && keypadValue < 2040) {
  key1count++;
} else if (keypadValue > 1320 && keypadValue < 1340) {
  key2count++;
} else if (keypadValue > 980 && keypadValue < 1000) {
  key3count++;
} else if (keypadValue > 770 && keypadValue < 790) {
  key4count++;
} else if (keypadValue > 640 && keypadValue < 660) {
  key5count++;
} else if (keypadValue > 550 && keypadValue < 570) {
  key6count++;
}
```

<br>

## Assembly Resources

Although it's unlikely anyone would want to make one of these for themselves, below is a list of parts, 3D models, a wiring diagram, and an SVG for the display faces.

### Parts List

- Battery Indicator
  - [150mm MGN9 linear rail and carriage](https://www.amazon.com/dp/B0D54M2NR8)
  - [Stepper motor with driver board](<https://www.microcenter.com/product/639726/KS0327_Keyestudio_Stepper_Motor_Drive_Board__5V_Stepper_Motor_Kit_(3PCS)>)
  - [Micro switch](https://www.adafruit.com/product/818)
  - [6mm timing belt](<https://www.microcenter.com/product/661279/3D_Printer_Timing_Belt_(6mm)>)
  - [5x6mm 20T pully wheel](https://www.amazon.com/dp/B077GNZK3J)
  - [5x6mm idler wheel](https://www.amazon.com/dp/B07CCMQ8T7)
  - [25mm 3v blue filament LED](https://www.adafruit.com/product/6143)
  - [300mm 3v white filament LED](https://www.adafruit.com/product/6146)
  - [5k potentiometer](https://www.amazon.com/dp/B00GYVH0PW)
  - [Machined 20mm knob](https://www.adafruit.com/product/5527)
    <br>
    <br>
- Charge Voltage Display
  - [91C4 3V volt meter](https://www.amazon.com/dp/B08RNGGSWM)
    <br>
    <br>
- Refresh Counter
  - [3V mini DC motor](https://www.adafruit.com/product/3871)
  - [5V 9G servo](https://www.microcenter.com/product/613880/inland-blue-9g-servo-3-pack)
  - [3x1mm magnets](https://www.amazon.com/dp/B09MQ3JXLV)
  - [49E hall sensor](https://www.amazon.com/dp/B0CZ6RL4B2)
  - [Counter from Pioneer CT-760 tape deck](https://www.facebook.com/share/1CSfxKRXxj/)
    <br>
    <br>
- Cassette Tape
  - [3V mini DC motor](https://www.adafruit.com/product/3871)
  - [7mm x 14mm x 5mm bearings](https://www.amazon.com/dp/B07FPNWBL6)
  - [1mm rubber belt](https://www.amazon.com/dp/B077FNY4H1)
  - [Maxell cassette tape](https://www.amazon.com/dp/B000001OKK)
  - [1mm felt sheets](https://www.amazon.com/dp/B01GCLS32M)
    <br>
    <br>
- Command Keypad
  - [Linear keyboard switches](https://www.microcenter.com/product/674879/MX_Heavy_Black_RGB__Linear_Key_Switches)
  - [Low profile keycaps](https://mechanicalkeyboards.com/products/tai-hao-black-tht-blank-18-key-abs-low-profile)
    <br>
    <br>
- Speaker
  - [1w 8 ohm speaker](https://www.adafruit.com/product/5986)
  - [5k slide potentiometer](https://www.amazon.com/dp/B07W3WRZH2)
    <br>
    <br>
- Electronics
  - [Arduino Nano ESP32](https://store.arduino.cc/collections/nano-family/products/nano-esp32)
  - [Mini breadboard](https://www.microcenter.com/product/504160/Perma-Proto_Mint_Tin_Size_Breadboard_PCB)
  - [TIP120 transistors](https://www.amazon.com/dp/B08BFYYK7D)
  - [0.1mF capacitor](https://www.microcenter.com/product/603754/Pi_Kit_Deluxe_Parts_Pack)
  - [3v LED](https://www.microcenter.com/product/632687/5mm_5_Colors_LED_Light_Emitting_Diode_Assortment_Kit_-_500_Pcs)
  - [Assorted resistors](https://www.amazon.com/dp/B0792M83JH)
  - [Toggle switch](https://www.amazon.com/dp/B07PK6C6LB)
  - [USB-C extension](https://www.adafruit.com/product/6069)
    <br>
    <br>
- Housing
  - [0.5mm clear PET sheet](https://www.amazon.com/dp/B0CT9W4QFD)
  - [Ocean Blue PLA](https://www.amazon.com/dp/B01DXWROBO)
  - [Carbon Fiber PLA](<https://www.microcenter.com/product/632387/175mm_PLA_Pro_3D_Printer_Filament_10_kg_(22_lbs)_Spool_-_Carbon_Fiber;_Tough,_High_Rigidity>)
  - [Gray PLA](<https://www.microcenter.com/product/606620/175mm_PLA_3D_Printer_Filament_1kg_(22_lbs)_Cardboard_Spool_-_Gray;_Dimensional_Accuracy_--_003mm,_Fits_Most_FDM-FFF_Printers,_Odor_Free,_Clog_Free>)
  - [Brown PLA](<https://www.microcenter.com/product/611536/175mm_Brown_PLA_3D_Printer_Filament_-_1kg_Spool_(22_lbs)>)
  - [White PLA](<https://www.microcenter.com/product/611544/inland-175mm-pla-3d-printer-filament-10-kg-(22-lbs)-spool-white>)
  - [M2 & M3 heat-set inserts](https://www.amazon.com/dp/B08BCRZZS3)
  - [M2 & M3 screws](https://www.amazon.com/dp/B0BNJMTQVX)
    <br>
    <br>
- Aesthetics
  - [Chevrolet EV enamel pin](https://chevycollection.com/products/chevy-ev-lapel-pin?variant=43225981714630)
  - [Chevrolet bowtie enamel pin](https://chevycollection.com/products/copy-of-chevy-gold-bowtie-lapel-pin-1?variant=43225999507654)
  - [130mm telescoping antenna](https://www.amazon.com/dp/B00W8WL980)
  - 1.5mm laser-cut brushed aluminum front [STEP file](/resources/AluminumFront.step) from [pcbway.com](https://www.pcbway.com)

### Files

- [3MFs for 3D printing](/resources/models)
- [SVG for 2D printing](/resources/DisplayFaces.svg)
- [Fritzing wiring diagram](/resources/WiringDiagram.fzz)![Full wiring diagram](/images/Wiring.png)

<br>

## Future Improvements

- Allow wifi details to be set through serial interface and/or bluetooth
- Implement one-time password login as seen in [OnStarJS2](https://github.com/BigThunderSR/OnStarJS) so network sniffing isn't required
- Switch to OBD-II option such as [mictrack](https://shop.mictrack.com/product/4g-obd-gps-tracker-mp91/) for direct vehicle connection through the network provider of my choice (no OnStar subscription, possibly no VoIP subscription either)
