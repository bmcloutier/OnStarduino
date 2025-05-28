#include <EEPROM.h>
#include <ESP32Servo.h>
#include <TimeLib.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

//
// PIN CONSTANTS
const int keypadPin = 17;       // A0 INPUT
const int dialPin = 22;         // A5 INPUT
const int smsPin = 21;          // A4 INPUT PULLUP
const int voltagePin = 13;      // D13 PWM OUTPUT
const int counterServoPin = 3;  // D3 PWM OUTPUT
const int counterMotorPin = 2;  // D2 OUTPUT
const int hallSensorPin = 23;   // A6 INPUT
const int stepperPin1 = 9;      // D9 OUTPUT
const int stepperPin2 = 10;     // D10 OUTPUT
const int stepperPin3 = 11;     // D11 OUTPUT
const int stepperPin4 = 12;     // D12 OUTPUT
const int stepperStopPin = 18;  // A1 INPUT PULLUP
const int tapeMotorPin = 8;     // D8 OUTPUT
const int speakerPin = 19;      // A2 PWM OUTPUT
const int batteryLEDPin = 6;    // D6 PWM OUTPUT
const int statusLEDPin = 24;    // A7 PWM OUTPUT

//
// SECRET VARIABLES
char mobileSSID[] = SECRET_MOBILE_SSID;
char wifiSSID[] = SECRET_WIFI_SSID;
char wifiPass[] = SECRET_WIFI_PASS;
char smsUser[] = SECRET_SMS_USER;
char smsPass[] = SECRET_SMS_PASS;
char smsDID[] = SECRET_SMS_DID;
char smsDST[] = SECRET_SMS_DST;
char vin[] = SECRET_VIN;
char clientRequestID[] = SECRET_CLIENT_REQUEST_ID;
char clientID[] = SECRET_CLIENT_ID;
char deviceID[] = SECRET_DEVICE_ID;

//
// GENERAL VARIABLES
unsigned long currentMillis = 0;
int failState = 0;

//
// COMMUNICATIONS VARIABLES
WiFiClientSecure client;
char smsServer[] = "voip.ms";
char refreshServer[] = "custlogin.gm.com";
char apiServer[] = "na-mobile-api.gm.com";
int port = 443;
int refreshTokenLength = 0;
int subjectTokenLength = 0;
int refreshTokenAddress = 10;
char refreshToken[1660] = SECRET_REFRESH_TOKEN;
char subjectToken[1725] = {0};
char accessToken[1600] = {0};
long refreshTokenTimestamp = 0;
long subjectTokenTimestamp = 0;
long accessTokenTimestamp = 0;
int collectIndex = 0;
char collect = false;
char timeCollection[59];
char subjectCollection[6000] = {0};
char accessCollection[2000] = {0};
char commandCollection[300] = {0};
char responseCollection[1500] = {0};
char commandID[50] = {0};
int queuedRequest = 0;
int queuedFetch = 0;
unsigned long commandURLStamp = 0;
unsigned long diagnosticStamp = 0;
unsigned long dailyStamp = 0;
unsigned long weeklyStamp = 0;
unsigned long alertStamp = 0;
unsigned long sendStamp = 0;

// KEYPAD VARIABLES
int keypadValue = 0;
unsigned long keyStamp = 0;
int key1count = 0;
int key2count = 0;
int key3count = 0;
int key4count = 0;
int key5count = 0;
int key6count = 0;
int keySampleCooldown = 5;
int queuedCommand = 0;

//
// EV VARIABLES
int evBatteryLevel = 0;
int evPlugVoltage = 0;
bool isEVCharging = false;
bool isEVPlugged = false;

//
// VOLTMETER VARIABLES
bool setVoltage = false;

//
// LED VARIABLES
bool isTransmitting = false;
unsigned long batteryLEDStamp = 0;
float batteryLEDBrightness = 10.0;
int dialValue = 0;
int rollingDialValues[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int dialElement = 0;
int dialAverage = 0;
int stickyDialAverage = 0;
unsigned long dialStamp = 0;
float brightnessMultiplier = 0.0;
bool increaseBrightness = true;
unsigned long statusLEDStamp = 0;
int statusLEDIncrement = 2;
byte statusLEDBrightness = 0;

//
// STEPPER VARIABLES
unsigned long stepperStamp = 0;
const int stepsTo100 = 5600;
int stepperCooldown = 2;
int stepperTarget = 0;
int stepperValue = 0;
int stepperStep = 0;

//
// COUNTER VARIABLES
Servo counterservo;
unsigned long servoStamp = 0;
unsigned long counterStamp = 0;
unsigned long hourStamp = 0;
bool isResetRequired = true;
bool isServoRaised = false;
bool isNewHour = false;
bool wasLastSampleHigh = false;
int servoCooldown = 500;
int counterPosition = 0;
int hoursPassed = 0;

//
// DEMO VARIABLES
bool demoMode = false;
unsigned long demoKeyStamp = 0;
bool demoSpinTape = false;

//
// SETUP
void setup() {
  Serial.begin(115200);

  // KEYPAD SETUP
  pinMode(keypadPin, INPUT);

  // COUNTER SERVO/MOTOR/SENSOR SETUP
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  counterservo.setPeriodHertz(50);
  counterservo.attach(counterServoPin);
  pinMode(counterMotorPin, OUTPUT);
  pinMode(hallSensorPin, INPUT);
  while (digitalRead(hallSensorPin) == HIGH) {
    digitalWrite(counterMotorPin, HIGH);
  }
  digitalWrite(counterMotorPin, LOW);

  // BATTERY/STATUS LED SETUP
  pinMode(dialPin, INPUT);
  pinMode(batteryLEDPin, OUTPUT);
  pinMode(statusLEDPin, OUTPUT);
  analogWrite(batteryLEDPin, batteryLEDBrightness);

  // STEPPER/STOP SWITCH SETUP
  pinMode(stepperPin1, OUTPUT);
  pinMode(stepperPin2, OUTPUT);
  pinMode(stepperPin3, OUTPUT);
  pinMode(stepperPin4, OUTPUT);
  pinMode(stepperStopPin, INPUT_PULLUP);

  while (digitalRead(stepperStopPin) == LOW) {
    stepOnce(-1);
    delay(stepperCooldown);
  }
  for (int f = 0; f < 220; f++) {
    stepOnce(1);
    delay(stepperCooldown);
  }
  digitalWrite(stepperPin1, 0);
  digitalWrite(stepperPin2, 0);
  digitalWrite(stepperPin3, 0);
  digitalWrite(stepperPin4, 0);

  // SMS TOGGLE SETUP
  pinMode(smsPin, INPUT);

  // TAPE MOTOR SETUP
  pinMode(tapeMotorPin, OUTPUT);

  // SPEAKER SETUP
  pinMode(speakerPin, OUTPUT);
  setToneChannel(6);

  // VOLTMETER SETUP
  pinMode(voltagePin, OUTPUT);

  // WIFI SETUP
  keypadValue = (analogRead(keypadPin));
  if (keypadValue > 2010 && keypadValue < 2040) {
    demoMode = true;
    WiFi.begin(wifiSSID, wifiPass);
    playZelda();
  } else if (keypadValue > 1320 && keypadValue < 1340) {
    demoMode = true;
    WiFi.begin(mobileSSID, wifiPass);
    playZelda();
  } else if (keypadValue > 980 && keypadValue < 1000) {
    WiFi.begin(mobileSSID, wifiPass);
    playZelda();
  } else {
    WiFi.begin(wifiSSID, wifiPass);
  }
  keypadValue = 0;
  isTransmitting = true;
  while (WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    setStatusLED();
  }
  isTransmitting = false;
  setStatusLED();

  // TIME UPDATE
  client.setInsecure();
  isTransmitting = true;
  client.connect(smsServer, port);
  while (!client.connected()) {
    delay(10);
  }
  queuedRequest = 11;
  sendRequest();
  while (client.available() < 200) {
    delay(10);
  }
  fetchResponse();
  updateTime();
  Serial.println(now());

  if (timeStatus() == timeNotSet) {
    blinkSOS();
  }

  // EEPROM SETUP
  EEPROM.begin(1670);
  EEPROM.get(0, refreshTokenTimestamp);
  if (refreshTokenTimestamp > 0) {
    EEPROM.get(10, refreshToken);
  }
  for (int l = 0; l < sizeof(refreshToken); l++) {
    if (refreshToken[l] == NULL) {
      refreshTokenLength = l;
      break;
    }
  }
}

//
// LOOP
void loop() {
  currentMillis = millis();
  handleErrors();
  queueDiagnosticCommand();
  sendAlert();
  sampleKeypad();
  queueKeypadCommand();
  handleCommand();
  sendRequest();
  fetchResponse();
  quanitzeDial();
  setBatteryLED();
  setStatusLED();
  setVoltmeter();
  setStepper();
  resetCounter();
  incrementCounter();
}

//
// FUNCTIONS

void handleErrors() {
  if (failState > 0) {
    if (WiFi.isConnected()) {
      Serial.println("Sending SMS with error message");
      client.connect(smsServer, port);
      client.print("GET /api/v1/rest.php?api_username=");
      client.print(smsUser);
      client.print("&api_password=");
      client.print(smsPass);
      client.print("&method=sendSMS&did=");
      client.print(smsDID);
      client.print("&dst=");
      client.print(smsDST);
      client.print("&message=ERROR: ");
    }
    Serial.print("ERROR: ");
    switch (failState) {
      case 1: {
        client.print("Initial connection failed for request ");
        client.print(queuedRequest);
        Serial.print("Initial connection failed for request ");
        Serial.println(queuedRequest);
        break;
      }
      case 2: {
        client.print("Response took too long to arrive for case ");
        client.print(queuedFetch);
        Serial.print("Response took too long to arrive for case ");
        Serial.println(queuedFetch);
        break;
      }
      case 3: {
        client.print("inProgress status not received for command ");
        client.print(queuedFetch);
        Serial.print("inProgress status not received for command ");
        Serial.println(queuedFetch);
        Serial.println(commandCollection);
        break;
      }
      case 4: {
        client.print("Unexpected command result");
        Serial.println("Unexpected command result");
        Serial.println(responseCollection);
        break;
      }
      case 5: {
        client.print("Failed refresh or subject token update");
        Serial.println("Failed refresh or subject token update");
        Serial.println(subjectCollection);
        break;
      }
      case 6: {
        client.print("Failed access token update");
        Serial.println("Failed access token update");
        Serial.println(accessCollection);
        break;
      }
      case 7: {
        client.print("Failed to get date");
        Serial.println("Failed to get date");
        Serial.println(timeCollection);
        break;
      }
      case 8: {
        client.print("Invalid battery level");
        Serial.println("Invalid battery level");
        Serial.println(responseCollection);
        break;
      }
      case 9: {
        client.print("Invalid plug voltage");
        Serial.println("Invalid plug voltage");
        Serial.println(responseCollection);
        break;
      }
      case 10: {
        client.print("Invalid plug state");
        Serial.println("Invalid plug state");
        Serial.println(responseCollection);
        break;
      }
      case 11: {
        client.print("Invalid charge state");
        Serial.println("Invalid charge state");
        Serial.println(responseCollection);
        break;
      }
    }
    client.println(" HTTP/1.1");
    client.println("Host: voip.ms");
    client.println();
    client.flush();
    client.stop();
  }
  failState = 0;
}

void queueDiagnosticCommand() {
  if (queuedCommand == 0) {
    if (isEVCharging && currentMillis - diagnosticStamp >= 3600000) {
      queuedCommand = 6;
    } else if (digitalRead(smsPin) == HIGH && hour() == 2 && (currentMillis - dailyStamp > 3600000 || dailyStamp == 0)) {
      // 2:00GMT/21:00CDT
      dailyStamp = currentMillis;
      queuedCommand = 6;
    } else if (weekday() == 7 && hour() == 13 && (currentMillis - weeklyStamp > 3600000 || weeklyStamp == 0)) {
      // Saturday at 13:00GMT/8:00CDT
      weeklyStamp = currentMillis;
      queuedCommand = 6;
    }
  }
}

void sendAlert() {
  if (queuedCommand == 0 && digitalRead(smsPin) == HIGH && !isEVPlugged && evBatteryLevel < 80 && dailyStamp > alertStamp) {
    alertStamp = currentMillis;
    client.connect(smsServer, port);
    queuedRequest = 10;
    isTransmitting = true;
  }
}

void sampleKeypad() {
  if (!isTransmitting && queuedRequest == 0 && currentMillis - keyStamp >= keySampleCooldown) {
    keyStamp = currentMillis;
    keypadValue = analogRead(keypadPin);
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
  }
}

void queueKeypadCommand() {
  if (demoMode && currentMillis - demoKeyStamp >= 1000) {
    demoKeyStamp = currentMillis;
    if (key1count > 2) {
      if (stepperTarget != stepsTo100 * 0.3) {
        stepperTarget = stepsTo100 * 0.3;
      }
      setTime(0, 0, 0, 1, 1, 2000);
      hourStamp = -60000;
      evPlugVoltage = 0;
      setVoltage = true;
      resetKeyCount();
    } else if (key2count > 2) {
      if (evPlugVoltage == 0) {
        isResetRequired = true;
        evPlugVoltage = 240;
        setVoltage = true;
      }
      if (stepperTarget < stepsTo100 * 0.8) {
        stepperTarget += stepsTo100 * 0.1;
      }
      resetKeyCount();
    } else if (key3count > 2) {
      if (!demoSpinTape) {
        demoSpinTape = true;
        digitalWrite(tapeMotorPin, HIGH);
        for (int i = 0; i < 2; i++) {
          while (statusLEDBrightness < 180) {
            statusLEDBrightness += statusLEDIncrement;
            analogWrite(statusLEDPin, statusLEDBrightness);
            delay(2);
          }
          statusLEDBrightness = 0;
        }
        analogWrite(statusLEDPin, 0);
      } else {
        demoSpinTape = false;
        for (int i = 0; i < 4; i++) {
          while (statusLEDBrightness < 180) {
            statusLEDBrightness += statusLEDIncrement;
            analogWrite(statusLEDPin, statusLEDBrightness);
            delay(2);
          }
          statusLEDBrightness = 0;
        }
        digitalWrite(tapeMotorPin, LOW);
        analogWrite(statusLEDPin, 0);
        statusLEDBrightness = 0;
      }
      resetKeyCount();
    } else if (key4count > 2) {
      if (digitalRead(smsPin) == HIGH) {
        client.connect(smsServer, port);
        queuedRequest = 10;
      }
      resetKeyCount();
    } else if (key5count > 2) {
      resetKeyCount();
    } else if (key6count > 2) {
      resetKeyCount();
    }
  } else if (!demoMode && queuedCommand == 0) {
    if (key1count > 2) {
      queuedCommand = 1;
      resetKeyCount();
    } else if (key2count > 2) {
      queuedCommand = 2;
      resetKeyCount();
    } else if (key3count > 2) {
      queuedCommand = 3;
      resetKeyCount();
    } else if (key4count > 2) {
      queuedCommand = 4;
      resetKeyCount();
    } else if (key5count > 2) {
      queuedCommand = 5;
      resetKeyCount();
    } else if (key6count > 2) {
      queuedCommand = 6;
      resetKeyCount();
    }
    if (queuedCommand > 0) {
      digitalWrite(tapeMotorPin, HIGH);
    }
  }
}

void handleCommand() {
  if (queuedCommand > 0 && !isTransmitting) {
    if (now() - subjectTokenTimestamp >= 3600 - 60) {
      client.connect(refreshServer, port);
      queuedRequest = 8;
    } else if (now() - accessTokenTimestamp >= 1800 - 60) {
      client.connect(apiServer, port);
      queuedRequest = 9;
    } else if (queuedCommand != 7) {
      client.connect(apiServer, port);
      queuedRequest = queuedCommand;
    } else if (queuedCommand == 7 && currentMillis - commandURLStamp > 5000) {
      commandURLStamp = currentMillis;
      client.connect(apiServer, port);
      queuedRequest = 7;
    }
    if (queuedRequest > 0) {
      isTransmitting = true;
    }
    if (queuedRequest > 0 && client.connected() == false) {
      failState = 1;
    }
  }
}

void sendRequest() {
  if (client.connected() && queuedRequest > 0) {
    switch (queuedRequest) {
      // START
      case 1: {
        client.print("POST /api/v1/account/vehicles/");
        client.print(vin);
        client.println("/commands/start HTTP/1.1");
        client.println("Accept: application/json");
        client.println("Accept-Encoding: br, gzip, deflate");
        client.println("Accept-Language: en-US");
        client.print("Authorization: Bearer ");
        client.println(accessToken);
        client.println("Connection: keep-alive");
        client.println("Content-Type: application/json; charset=UTF-8");
        client.println("Host: na-mobile-api.gm.com");
        client.println("User-Agent: myChevrolet/118 CFNetwork/1408.0.4 Darwin/22.5.0");
        client.println("Content-Length: 0");
        client.println();
        break;
      }

      // STOP
      case 2: {
        client.print("POST /api/v1/account/vehicles/");
        client.print(vin);
        client.println("/commands/cancelStart HTTP/1.1");
        client.println("Accept: application/json");
        client.println("Accept-Encoding: br, gzip, deflate");
        client.println("Accept-Language: en-US");
        client.print("Authorization: Bearer ");
        client.println(accessToken);
        client.println("Connection: keep-alive");
        client.println("Content-Type: application/json; charset=UTF-8");
        client.println("Host: na-mobile-api.gm.com");
        client.println("User-Agent: myChevrolet/118 CFNetwork/1408.0.4 Darwin/22.5.0");
        client.println("Content-Length: 0");
        client.println();
        break;
      }

      // LOCK
      case 3: {
        client.print("POST /api/v1/account/vehicles/");
        client.print(vin);
        client.println("/commands/lockDoor HTTP/1.1");
        client.println("Accept: application/json");
        client.println("Accept-Encoding: br, gzip, deflate");
        client.println("Accept-Language: en-US");
        client.print("Authorization: Bearer ");
        client.println(accessToken);
        client.println("Connection: keep-alive");
        client.println("Content-Type: application/json; charset=UTF-8");
        client.println("Host: na-mobile-api.gm.com");
        client.println("User-Agent: myChevrolet/118 CFNetwork/1408.0.4 Darwin/22.5.0");
        client.println("Content-Length: 34");
        client.println();
        client.println("{\"lockDoorRequest\": { \"delay\": 0}}");
        break;
      }

      // UNLOCK
      case 4: {
        client.print("POST /api/v1/account/vehicles/");
        client.print(vin);
        client.println("/commands/unlockDoor HTTP/1.1");
        client.println("Accept: application/json");
        client.println("Accept-Encoding: br, gzip, deflate");
        client.println("Accept-Language: en-US");
        client.print("Authorization: Bearer ");
        client.println(accessToken);
        client.println("Connection: keep-alive");
        client.println("Content-Type: application/json; charset=UTF-8");
        client.println("Host: na-mobile-api.gm.com");
        client.println("User-Agent: myChevrolet/118 CFNetwork/1408.0.4 Darwin/22.5.0");
        client.println("Content-Length: 36");
        client.println();
        client.println("{\"unlockDoorRequest\": { \"delay\": 0}}");
        break;
      }

      // FLASH
      case 5: {
        client.print("POST /api/v1/account/vehicles/");
        client.print(vin);
        client.println("/commands/alert HTTP/1.1");
        client.println("Accept: application/json");
        client.println("Accept-Encoding: br, gzip, deflate");
        client.println("Accept-Language: en-US");
        client.print("Authorization: Bearer ");
        client.println(accessToken);
        client.println("Connection: keep-alive");
        client.println("Content-Type: application/json; charset=UTF-8");
        client.println("Host: na-mobile-api.gm.com");
        client.println("User-Agent: myChevrolet/118 CFNetwork/1408.0.4 Darwin/22.5.0");
        client.println("Content-Length: 105");
        client.println();
        client.println("{\"alertRequest\":{ \"action\": [\"Flash\"], \"delay\": 0, \"duration\": 1, \"override\": [\"DoorOpen\",\"IgnitionOn\"]}}");
        break;
      }

      // DIAGNOSTICS
      case 6: {
        client.print("POST /api/v1/account/vehicles/");
        client.print(vin);
        client.println("/commands/diagnostics HTTP/1.1");
        client.println(" HTTP/1.1");
        client.println("Accept: application/json");
        client.println("Accept-Encoding: br, gzip, deflate");
        client.println("Accept-Language: en-US");
        client.print("Authorization: Bearer ");
        client.println(accessToken);
        client.println("Connection: keep-alive");
        client.println("Content-Type: application/json; charset=UTF-8");
        client.println("Host: na-mobile-api.gm.com");
        client.println("User-Agent: myChevrolet/118 CFNetwork/1408.0.4 Darwin/22.5.0");
        client.println("Content-Length: 117");
        client.println();
        client.println("{\"diagnosticsRequest\": { \"diagnosticItem\": [\"EV BATTERY LEVEL\",\"EV PLUG VOLTAGE\",\"EV PLUG STATE\",\"EV CHARGE STATE\"]}}");
        break;
      }

      // COMMAND RESULT
      case 7: {
        client.print("GET /api/v1/account/vehicles/");
        client.print(vin);
        client.print("/requests/");
        client.print(commandID);
        client.println(" HTTP/1.1");
        client.println("Accept: application/json");
        client.println("Accept-Encoding: br, gzip, deflate");
        client.println("Accept-Language: en-US");
        client.print("Authorization: Bearer ");
        client.println(accessToken);
        client.println("Connection: keep-alive");
        client.println("Content-Type: application/json; charset=UTF-8");
        client.println("Host: na-mobile-api.gm.com");
        client.println("User-Agent: myChevrolet/118 CFNetwork/1408.0.4 Darwin/22.5.0");
        client.println();
        break;
      }

      // REFRESH/SUBJECT TOKEN
      case 8: {
        client.println("POST /gmb2cprod.onmicrosoft.com/b2c_1a_seamless_mobile_signuporsignin/oauth2/v2.0/token HTTP/1.1");
        client.println("Accept: application/json");
        client.print("Client-Request-Id: ");
        client.println(clientRequestID);
        client.print("Content-Length: ");
        client.println(refreshTokenLength + 279);
        client.println("Content-Type: application/x-www-form-urlencoded");
        client.println("Host: custlogin.gm.com");
        client.println("Return-Client-Request-Id: true");
        client.println("User-Agent: myChevrolet/118 CFNetwork/1408.0.4 Darwin/22.5.0");
        client.println();
        client.print("client_id=");
        client.print(clientID);
        client.print("&scope=https%3A%2F%2Fgmb2cprod.onmicrosoft.com%2F3ff30506-d242-4bed-835b-422bf992622e%2FTest.Read+openid+profile+offline_access&refresh_token=");
        client.print(refreshToken);
        client.println("&redirect_uri=msauth.com.gm.myChevrolet%3A%2F%2Fauth&client_info=1&grant_type=refresh_token");
        break;
      }

      // ACCESS TOKEN
      case 9: {
        client.println("POST /sec/authz/v3/oauth/token HTTP/1.1");
        client.println("Accept-Language: en-US");
        client.println("Appversion: OMB_CVY_IOS_7.1.0");
        client.print("Content-Length: ");
        client.println(subjectTokenLength + 262);
        client.println("Content-Type: application/x-www-form-urlencoded");
        client.println("Host: na-mobile-api.gm.com");
        client.println("User-Agent: myChevrolet/118 CFNetwork/1408.0.4 Darwin/22.5.0");
        client.println();
        client.print("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Atoken-exchange&subject_token=");
        client.print(subjectToken);
        client.print("&subject_token_type=urn%3Aietf%3Aparams%3Aoauth%3Atoken-type%3Aaccess_token&scope=msso+role_owner+priv+onstar+gmoc+user+user_trailer&device_id=");
        client.println(deviceID);
        break;
      }

      // SMS
      case 10: {
        client.print("GET /api/v1/rest.php?api_username=");
        client.print(smsUser);
        client.print("&api_password=");
        client.print(smsPass);
        client.print("&method=sendSMS&did=");
        client.print(smsDID);
        client.print("&dst=");
        client.print(smsDST);
        client.println("&message=Vehicle is unplugged HTTP/1.1");
        client.println("Host: voip.ms");
        client.println();
        break;
      }

      // TIME
      case 11: {
        client.println("GET / HTTP/1.1");
        client.print("Host: ");
        client.println(smsServer);
        client.println("Connection: close");
        client.println();
        break;
      }
    }
    queuedFetch = queuedRequest;
    queuedRequest = 0;
    sendStamp = millis();
  }
}

void fetchResponse() {
  if ((client.available() > 50) && queuedFetch > 0) {
    collectIndex = 0;
    collect = false;
    switch ((queuedFetch <= 6) ? 1 : queuedFetch) {
      // COMMAND ID
      case 1: {
        while (client.available()) {
          char c = client.read();
          if (c == 123) {
            collect = true;
          }
          if (collect) {
            commandCollection[collectIndex++] = c;
          }
        }
        commandCollection[collectIndex] = '\0';
        client.flush();
        if (strstr(commandCollection, "inProgress")) {
          char *commandIDStart = strstr(commandCollection, "requests") + 9;
          char *commandIDEnd = strstr(commandIDStart, "\"");
          strncpy(commandID, commandIDStart, commandIDEnd - commandIDStart);
          commandID[commandIDEnd - commandIDStart] = '\0';
          queuedCommand = 7;
        } else {
          digitalWrite(tapeMotorPin, LOW);
          failState = 3;
        }
        break;
      }

      // COMMAND RESULT
      case 7: {
        while (client.available()) {
          char c = client.read();
          if (c == 123) {
            collect = true;
          }
          if (collect) {
            responseCollection[collectIndex++] = c;
          }
        }
        responseCollection[collectIndex] = '\0';
        client.flush();
        if (strstr(responseCollection, "inProgress")) {
          Serial.println("Command inProgress");
        } else if (strstr(responseCollection, "success")) {
          queuedCommand = 0;
          digitalWrite(tapeMotorPin, LOW);
          if (strstr(responseCollection, "diagnostics")) {
            diagnosticStamp = currentMillis;
            isResetRequired = true;
            updateDiagnostics();
          }
        } else {
          digitalWrite(tapeMotorPin, LOW);
          failState = 4;
        }
        break;
      }

      // REFRESH/SUBJECT TOKEN
      case 8: {
        delay(100);
        while (client.available()) {
          char c = client.read();
          if (c == 123) {
            collect = true;
          }
          if (collect) {
            subjectCollection[collectIndex++] = c;
          }
        }

        subjectCollection[collectIndex] = '\0';

        if (strstr(subjectCollection, "refresh_token_expires_in") != NULL) {
          if (now() - refreshTokenTimestamp >= 604800 - 60) {
            Serial.println("Time to write new refresh token to EEPROM");
            char *refreshTokenStart = strstr(subjectCollection, "refresh_token") + 16;
            char *refreshTokenEnd = strstr(refreshTokenStart, "\"");
            refreshTokenLength = refreshTokenEnd - refreshTokenStart;
            strncpy(refreshToken, refreshTokenStart, refreshTokenLength);
            refreshToken[refreshTokenLength] = '\0';
            Serial.println(refreshToken);
            Serial.println();
            refreshTokenTimestamp = now();
            Serial.println(refreshTokenTimestamp);
            EEPROM.put(0, refreshTokenTimestamp);
            EEPROM.put(10, refreshToken);
            EEPROM.commit();
          }

          char *subjectTokenStart = strstr(subjectCollection, "access_token") + 15;
          char *subjectTokenEnd = strstr(subjectTokenStart, "\"");
          subjectTokenLength = subjectTokenEnd - subjectTokenStart;
          strncpy(subjectToken, subjectTokenStart, subjectTokenLength);
          subjectToken[subjectTokenLength] = '\0';
          subjectTokenTimestamp = now();
        } else {
          failState = 5;
        }

        client.flush();
        break;
      }

      // ACCESS TOKEN
      case 9: {
        while (client.available()) {
          char c = client.read();
          if (c == 123) {
            collect = true;
          }
          if (collect) {
            accessCollection[collectIndex++] = c;
          }
        }
        accessCollection[collectIndex] = '\0';

        if (strstr(accessCollection, "access_token") != NULL) {
          char *accessTokenStart = strstr(accessCollection, "access_token") + 15;
          char *accessTokenEnd = strstr(accessTokenStart, "\"");
          strncpy(accessToken, accessTokenStart, accessTokenEnd - accessTokenStart);
          accessToken[accessTokenEnd - accessTokenStart] = '\0';
          accessTokenTimestamp = now();
        } else {
          failState = 6;
        }
        client.flush();
        break;
      }

      // SMS
      case 10: {
        client.flush();
        break;
      }

      // TIME
      case 11: {
        while (client.available() && collectIndex < 59) {
          timeCollection[collectIndex++] = client.read();
        }
        timeCollection[59] = '\0';
        client.flush();
        break;
      }
    }
    queuedFetch = 0;
    isTransmitting = false;
    client.stop();
  } else if (queuedFetch > 0 && currentMillis > sendStamp && currentMillis - sendStamp >= 5000) {
    failState = 2;
    isTransmitting = false;
    client.stop();
    digitalWrite(tapeMotorPin, LOW);
  }
}

void quanitzeDial() {
  if (currentMillis - dialStamp > 10) {
    dialStamp = currentMillis;
    dialValue = analogRead(dialPin);
    rollingDialValues[dialElement] = dialValue;
    dialAverage = (rollingDialValues[0] + rollingDialValues[1] + rollingDialValues[2] + rollingDialValues[3] + rollingDialValues[4] + rollingDialValues[5] + rollingDialValues[6] + rollingDialValues[7] + rollingDialValues[8] + rollingDialValues[9]) / 100;
    dialElement++;
    if (dialElement == 10) {
      dialElement = 0;
    }
    if (dialAverage - stickyDialAverage > 1 || dialAverage - stickyDialAverage < -1) {
      stickyDialAverage = dialAverage;
      if (stickyDialAverage <= 295) {
        brightnessMultiplier = 0.0;
      } else if (stickyDialAverage > 294 && stickyDialAverage <= 300) {
        brightnessMultiplier = 0.1;
      } else if (stickyDialAverage > 300 && stickyDialAverage <= 305) {
        brightnessMultiplier = 0.2;
      } else if (stickyDialAverage > 305 && stickyDialAverage <= 310) {
        brightnessMultiplier = 0.5;
      } else if (stickyDialAverage > 310 && stickyDialAverage <= 320) {
        brightnessMultiplier = 0.8;
      } else {
        brightnessMultiplier = 1.0;
      }
      Serial.println(brightnessMultiplier);
    }
  }
}

void setBatteryLED() {
  if (currentMillis - batteryLEDStamp > 10) {
    batteryLEDStamp = currentMillis;
    if (evPlugVoltage == 0 && brightnessMultiplier == 0.0) {
      batteryLEDBrightness = 0;
    } else if (evPlugVoltage == 0 && batteryLEDBrightness < 40 * brightnessMultiplier) {
      batteryLEDBrightness += brightnessMultiplier;
    } else if (evPlugVoltage == 0 && batteryLEDBrightness > 40 * brightnessMultiplier) {
      batteryLEDBrightness -= brightnessMultiplier;
    } else {
      if (batteryLEDBrightness < 40 * brightnessMultiplier) {
        increaseBrightness = true;
        batteryLEDBrightness = 40 * brightnessMultiplier;
      } else if (batteryLEDBrightness > 200 * brightnessMultiplier) {
        increaseBrightness = false;
        batteryLEDBrightness = 200 * brightnessMultiplier;
      } else if (increaseBrightness) {
        batteryLEDBrightness += brightnessMultiplier;
      } else {
        batteryLEDBrightness -= brightnessMultiplier;
      }
    }
    analogWrite(batteryLEDPin, batteryLEDBrightness);
  }
}

void setStatusLED() {
  if (isTransmitting) {
    if (currentMillis - statusLEDStamp > 1) {
      statusLEDStamp = currentMillis;
      statusLEDBrightness += statusLEDIncrement;
      analogWrite(statusLEDPin, statusLEDBrightness);
      if (statusLEDBrightness >= 180) {
        statusLEDBrightness = 0;
      }
    }
  } else if (!isTransmitting && statusLEDBrightness > 0) {
    statusLEDBrightness = 0;
    analogWrite(statusLEDPin, 0);
  }
}

void setVoltmeter() {
  if (evPlugVoltage == 0 && setVoltage) {
    analogWrite(voltagePin, 0);
    setVoltage = false;
  } else if (evPlugVoltage == 120 && setVoltage) {
    analogWrite(voltagePin, 70);
    setVoltage = false;
  } else if (evPlugVoltage == 240 && setVoltage) {
    analogWrite(voltagePin, 165);
    setVoltage = false;
  } else if (evPlugVoltage == 400 && setVoltage) {
    analogWrite(voltagePin, 255);
    setVoltage = false;
  }
}

void setStepper() {
  if (stepperValue != stepperTarget && stepperTarget >= 0 && stepperTarget <= stepsTo100 && currentMillis - stepperStamp >= stepperCooldown) {
    stepperStamp = currentMillis;
    if (stepperValue < stepperTarget) {
      stepOnce(1);
      stepperValue++;
    } else {
      stepOnce(-1);
      stepperValue--;
    }
  } else if (currentMillis - stepperStamp >= stepperCooldown) {
    digitalWrite(stepperPin1, 0);
    digitalWrite(stepperPin2, 0);
    digitalWrite(stepperPin3, 0);
    digitalWrite(stepperPin4, 0);
  }
}

void resetCounter() {
  if (isResetRequired && currentMillis - servoStamp >= servoCooldown && !isNewHour) {
    servoStamp = currentMillis;
    if (!isServoRaised) {
      counterservo.write(95);
      isServoRaised = true;
    } else {
      counterservo.write(110);
      isServoRaised = false;
      isResetRequired = false;
      hoursPassed = 0;
    }
  }
}

void incrementCounter() {
  if (!isEVCharging && minute() == 0 && currentMillis - hourStamp > 60000) {
    hourStamp = currentMillis;
    isNewHour = true;
    digitalWrite(counterMotorPin, HIGH);
  }
  if (isNewHour && currentMillis - counterStamp >= 5) {
    counterStamp = currentMillis;
    if (digitalRead(hallSensorPin) == HIGH && !wasLastSampleHigh) {
      wasLastSampleHigh = true;
    }
    if (digitalRead(hallSensorPin) == LOW && wasLastSampleHigh) {
      wasLastSampleHigh = false;
      counterPosition++;
    }
  }
  if ((hoursPassed % 4 != 0 && counterPosition == 8) || counterPosition == 9) {  // A little more every 4
    digitalWrite(counterMotorPin, LOW);
    hoursPassed++;
    counterPosition = 0;
    isNewHour = false;
  }
}

void stepOnce(int direction) {
  if (direction == 1 || direction == -1) {
    stepperStep += direction;
    if (stepperStep == -1) {
      stepperStep = 3;
    } else if (stepperStep == 4) {
      stepperStep = 0;
    }
    switch (stepperStep) {
      case 0:
        digitalWrite(stepperPin1, 1);
        digitalWrite(stepperPin2, 0);
        digitalWrite(stepperPin3, 0);
        digitalWrite(stepperPin4, 1);
        break;
      case 1:
        digitalWrite(stepperPin1, 0);
        digitalWrite(stepperPin2, 0);
        digitalWrite(stepperPin3, 1);
        digitalWrite(stepperPin4, 1);
        break;
      case 2:
        digitalWrite(stepperPin1, 0);
        digitalWrite(stepperPin2, 1);
        digitalWrite(stepperPin3, 1);
        digitalWrite(stepperPin4, 0);
        break;
      case 3:
        digitalWrite(stepperPin1, 1);
        digitalWrite(stepperPin2, 1);
        digitalWrite(stepperPin3, 0);
        digitalWrite(stepperPin4, 0);
        break;
    }
  }
}

void resetKeyCount() {
  key1count = 0;
  key2count = 0;
  key3count = 0;
  key4count = 0;
  key5count = 0;
  key6count = 0;
}

void updateTime() {
  if (strstr(timeCollection, "Date") == NULL) {
    failState = 7;
    return;
  }

  char dateFromServer[26] = {0};
  char *dateStart = strstr(timeCollection, "Date") + 6;
  char *dateEnd = strstr(timeCollection, "GMT") - 1;
  strncpy(dateFromServer, dateStart, dateEnd - dateStart);

  int monthNumber = 0;
  if (strstr(dateFromServer, "Jan") != NULL) {
    monthNumber = 1;
  }
  if (strstr(dateFromServer, "Feb") != NULL) {
    monthNumber = 2;
  }
  if (strstr(dateFromServer, "Mar") != NULL) {
    monthNumber = 3;
  }
  if (strstr(dateFromServer, "Apr") != NULL) {
    monthNumber = 4;
  }
  if (strstr(dateFromServer, "May") != NULL) {
    monthNumber = 5;
  }
  if (strstr(dateFromServer, "Jun") != NULL) {
    monthNumber = 6;
  }
  if (strstr(dateFromServer, "Jul") != NULL) {
    monthNumber = 7;
  }
  if (strstr(dateFromServer, "Aug") != NULL) {
    monthNumber = 8;
  }
  if (strstr(dateFromServer, "Sep") != NULL) {
    monthNumber = 9;
  }
  if (strstr(dateFromServer, "Oct") != NULL) {
    monthNumber = 10;
  }
  if (strstr(dateFromServer, "Nov") != NULL) {
    monthNumber = 11;
  }
  if (strstr(dateFromServer, "Dec") != NULL) {
    monthNumber = 12;
  }

  setTime(
      (dateFromServer[17] - '0') * 10 + (dateFromServer[18] - '0'),
      (dateFromServer[20] - '0') * 10 + (dateFromServer[21] - '0'),
      (dateFromServer[23] - '0') * 10 + (dateFromServer[24] - '0'),
      (dateFromServer[5] - '0') * 10 + (dateFromServer[6] - '0'),
      monthNumber,
      (dateFromServer[12] - '0') * 1000 + (dateFromServer[13] - '0') * 100 + (dateFromServer[14] - '0') * 10 + (dateFromServer[15] - '0'));
}

void playZelda() {
  tone(speakerPin, 392, 150);
  delay(150);
  tone(speakerPin, 369.99, 150);
  delay(150);
  tone(speakerPin, 311.13, 150);
  delay(150);
  tone(speakerPin, 220, 150);
  delay(150);
  tone(speakerPin, 207.65, 150);
  delay(150);
  tone(speakerPin, 329.63, 150);
  delay(150);
  tone(speakerPin, 415.3, 150);
  delay(150);
  tone(speakerPin, 523.25, 300);
  delay(300);
}

void blinkSOS() {
  while (true) {
    analogWrite(statusLEDPin, 180);
    delay(100);
    analogWrite(statusLEDPin, 0);
    delay(100);
    analogWrite(statusLEDPin, 180);
    delay(100);
    analogWrite(statusLEDPin, 0);
    delay(100);
    analogWrite(statusLEDPin, 180);
    delay(100);
    analogWrite(statusLEDPin, 0);

    delay(400);

    analogWrite(statusLEDPin, 180);
    delay(300);
    analogWrite(statusLEDPin, 0);
    delay(100);
    analogWrite(statusLEDPin, 180);
    delay(300);
    analogWrite(statusLEDPin, 0);
    delay(100);
    analogWrite(statusLEDPin, 180);
    delay(300);
    analogWrite(statusLEDPin, 0);

    delay(300);

    analogWrite(statusLEDPin, 180);
    delay(100);
    analogWrite(statusLEDPin, 0);
    delay(100);
    analogWrite(statusLEDPin, 180);
    delay(100);
    analogWrite(statusLEDPin, 0);
    delay(100);
    analogWrite(statusLEDPin, 180);
    delay(100);
    analogWrite(statusLEDPin, 0);

    delay(1200);
  }
}

void updateDiagnostics() {
  char *batteryLevelTag = strstr(responseCollection, "EV BATTERY LEVEL");
  char *batteryLevelValue = strstr(batteryLevelTag, "value") + 8;
  if (batteryLevelValue[3] == 46) {
    evBatteryLevel = 100;
  } else if (batteryLevelValue[2] == 46) {
    evBatteryLevel = (batteryLevelValue[0] - '0') * 10 + (batteryLevelValue[1] - '0');
  } else if (batteryLevelValue[1] == 46) {
    evBatteryLevel = (batteryLevelValue[0] - '0');
  } else {
    failState = 8;
  }
  stepperTarget = stepsTo100 * evBatteryLevel / 100;

  char *plugVoltageTag = strstr(responseCollection, "EV PLUG VOLTAGE");
  char *plugVoltageValue = strstr(plugVoltageTag, "value") + 8;
  if (isDigit(plugVoltageValue[2])) {
    evPlugVoltage = (plugVoltageValue[0] - '0') * 100 + (plugVoltageValue[1] - '0') * 10 + (plugVoltageValue[2] - '0');
  } else if (isDigit(plugVoltageValue[0])) {
    evPlugVoltage = 0;
  } else {
    failState = 9;
  }
  setVoltage = true;

  char *plugStateTag = strstr(responseCollection, "EV PLUG STATE");
  char *plugStateValue = strstr(plugStateTag, "value") + 8;
  if (plugStateValue[0] == 112) {
    isEVPlugged = true;
  } else if (plugStateValue[0] == 117) {
    isEVPlugged = false;
  } else {
    failState = 10;
  }

  char *chargeStateTag = strstr(responseCollection, "EV CHARGE STATE");
  char *chargeStateValue = strstr(chargeStateTag, "value") + 8;
  if (chargeStateValue[0] == 99) {
    isEVCharging = true;
  } else if (chargeStateValue[0] == 68) {
    isEVCharging = false;
  } else {
    failState = 11;
  }
}