/*
 * Licensed under Apache 2.0
 * Text version: https://www.apache.org/licenses/LICENSE-2.0.txt
 * SPDX short identifier: Apache-2.0
 * OSI Approved License: https://opensource.org/licenses/Apache-2.0
 * Author: Robert Wiesner
 * 
 * PICO-W and ESP32-S3 source code to handle 3 versions of the
 * Blowpipe(pressue generation), Blow(pressure sensor), Unset(new board)
 * 
 * Supprted HW:
 * Display: SH1106G over I2C @ I2C0, address 0x3C
 * M24C02: EEProm(2Kbit/256 bytes) address 0x50 on I2C1
 * MS5607: pressure and temp sensor on I2C1 Address 0x76 or 0x77 (checks)
 * DRV8837: two motor controller to handle the motor and vent using 6 GPIO
 * ADC1: Monitor the battery voltage (1/11 * BatVolt)
 * Supports 7.5 or 11.1V battery
 *
 * 4 Operation modes (based on EEPROM content):
 * * PB_UNSET_NOPROM: unable to access the EEPROM - HW issue should not happen
 * * PB_UNSET: Default after inital flash, enabels WEB server to set operation mode and program the EEPROM, setting up AP 
 * * P*: Pipe portion setting up the AP point, Setting up UDP and web server
 * * B*: Blow portion, conntection to the Pipe AP and start sending pressure data over USP port
*/

#define MAJOR 0
#define MINOR 4
#define PATCH 1
#define STR(A) #A
#define VERSION(A, B, C) STR(A) "." STR(B) "." STR(C)
#define RP2040W  0
#define ESP32_S3 1

#include <LittleFS.h>
#include "m24c02.h"
#include "setting.h"
#include "server_unset.h"
#include "client_blow.h"
#include "server_pipe.h"

struct sUDPData UDPdata;
DRV8837 motor(MOTO_S, MOTO_1, MOTO_2);
DRV8837 vent(VENT_S, VENT_1, VENT_2);
MS5xxx sensor(&PRESSURE_I2C);

char aIPaddress[17] = "xxx.xxx.xxx.xxx";

cMenueInfo mainMenu(aTouchSensor, aMainMenu);
cDisplay display(DISP_I2C);

AsyncWebServer server(80);
WiFiUDP Udp;
cM24C02 eeprom(Wire1);

#ifndef WL_NO_MODULE
#define WL_NO_MODULE WL_NO_SHIELD
#endif

#include "bp_server.h"

float
getAltitude(float press, float temp)
{
  const float sea_press = 1013.25;
  return ((pow((sea_press / press), 1/5.257) - 1.0) * (temp + 273.15)) / 0.0065;
}

void
initWire(TwoWire *pW, int scl, int sda, int speed)
{
#if RP2040W
  pW->setSCL(scl);
  pW->setSDA(sda);
#elif ESP32_S3
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  pW->setPins(sda, scl);
#else
#error Add new code here
#endif

  pW->setClock(speed);
  pW->begin();
}

int
checkI2C(TwoWire *pW, int addr)
{
  pW->beginTransmission(addr);
  return pW->endTransmission();
}

char aDevName[17];
char aSSID[17];
char aPassword[17];

int state = 0;
int displayIdx;
IPAddress serverAddr;

void setup(void) {

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(HEARTBEAT, OUTPUT);
  pinMode(ALARMLED, OUTPUT);
  
  BLINKEST(0, 2, 500);

  digitalWrite(LED_BUILTIN, 0);

  pinMode(MOTO_1, OUTPUT);
  pinMode(MOTO_2, OUTPUT);

  digitalWrite(MOTO_1, 1);
  digitalWrite(MOTO_2, 0);

  pinMode(VENT_1, OUTPUT);
  pinMode(VENT_2, OUTPUT);
  
  digitalWrite(VENT_1, 0);
  digitalWrite(VENT_2, 1);

  analogReadResolution(12);
  
  initWire(&Wire, I2C0_SCL, I2C0_SDA, 100000);
  // Wire.i2c_set_pullup_en(true);
  initWire(&Wire1, I2C1_SCL, I2C1_SDA, 100000);

  // check if display is connected
  if (checkI2C(display.getI2C(), 0x3c)) {
    // No Display
    CLEAR(WITH_DISPLAY);
  }

  // check if EEPROM is connected
  if (checkI2C(&Wire1, 0x50)) {
    // No EEProm
    CLEAR(WITH_EEPROM);
  }

  // check if pressure sensor is connected
  if (checkI2C(&Wire1, 0x76)) {
    if (checkI2C(&Wire1, 0x77)) {
      sensorAddr = 0;
      CLEAR(WITH_PRESSURE);
    } else {
      sensorAddr = 0x77;
    }
  } else {
    sensorAddr = 0x76;
  }

  if (CHECK(WITH_EEPROM)) {
    eeprom.readAll();
    if (MAJOR != eeprom.getByte(EEPROM_MAJOR) || MINOR != eeprom.getByte(EEPROM_MINOR) || PATCH != eeprom.getShort(EEPROM_PATCH)) {
      eeprom.setByte(EEPROM_MAJOR, MAJOR);
      eeprom.setByte(EEPROM_MINOR, MINOR);
      eeprom.setShort(EEPROM_PATCH, PATCH);
    }
  }

  if (CHECK(WITH_PRESSURE)) {
    sensor.setI2Caddr(sensorAddr);
    sensor.ReadProm();
  }

  if (CHECK(WITH_DISPLAY)) {
    display.init();
    display.addItem(DISP_UNDEF, initUndef());
    display.addItem(DISP_SERVER, initPipe());
    display.addItem(DISP_CLIENT, initBlow());
    pCurDispItems = aDispItems + DISP_UNDEF;
    display.addMenue(&mainMenu);
  } else {
    // We have an issue with the display, use LED to indicate it
    while (true) {
      BLINKEST(500, 6, 100);
    }
  }

  pCurDispItems = aDispItems + DISP_UNDEF;

  if (CHECK(WITH_EEPROM)) {
    eeprom.getBuffer(EEPROM_DEV_NAME,  aDevName,  sizeof(aDevName) - 1);
    eeprom.getBuffer(EEPROM_PASSWORD,  aPassword, sizeof(aPassword) - 1);
    eeprom.getBuffer(EEPROM_SSID_NAME, aSSID,     sizeof(aSSID) - 1);

    aDevName[sizeof(aDevName) - 1] = 0;
    aPassword[sizeof(aPassword) - 1] = 0;
    aSSID[sizeof(aSSID) - 1] = 0;

    switch (aDevName[0]) {
      case 'b':
      case 'B': setupBlow(); break;
      case 'p':
      case 'P': setupPipe(); break;
      default:  setupUnset(); break;
    }
    pCurDispItems = aDispItems + displayIdx;
    pCurDispItems->pDevTitle->updateText(aDevName);
    display.refresh(displayIdx);
    delay(1000);
  } else {
        displayIdx = DISP_UNDEF;
        pCurDispItems = aDispItems + displayIdx;
        strcpy(aSSID, "PB_UNSET_NOPROM");
        strcpy(aPassword, "0123456789");
        SET(STATE_UNSET);
  }

  int wifi_status = WL_NO_MODULE;
  if (CHECK(STATE_PIPE)) {
    setupServerPipe(server, aDevName, aPassword, aIPaddress);
    Udp.begin(1805); // Start UDP communication; wait for packages
  } else if (CHECK(STATE_BLOW)) {
    char aWaitStr[24];
    int connectCount = 1;
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    sprintf(aIPaddress, "AP: %s", aSSID);
    pCurDispItems->pDevIP->setValue(aIPaddress);
    do {
      sprintf(aWaitStr, "WAIT %d", connectCount);
      pCurDispItems->pError->setValue(aWaitStr);
      
      display.refresh(displayIdx);

      WiFi.begin(aSSID, aPassword);
      wifi_status = WiFi.waitForConnectResult(); // timeout 15 sec
      if (wifi_status == WL_NO_SSID_AVAIL) {
      }
    } while (wifi_status == WL_NO_SSID_AVAIL || wifi_status == WL_DISCONNECTED);

    if (wifi_status == WL_CONNECTED) {
      setupClientBlow(server, aDevName, aPassword);
      serverAddr = WiFi.localIP();
      strcpy(aIPaddress, serverAddr.toString().c_str());
      serverAddr[3] = 1;
    } else {
      sprintf(aIPaddress, "<NOT SET %d>", wifi_status);
    }
    pCurDispItems->pDevIP->setValue(aIPaddress);
    pCurDispItems->pError->setValue("Connected");
    Udp.begin(1805); // Start UDP communication, send packages
  } else {
    pCurDispItems->pDevTitle->updateText(aDevName);
    display.refresh(displayIdx);
    BLINKEST(500, 10, 50);
    int status = setupServerUndef(server, aSSID, aPassword, aDevName, aIPaddress);
    
    displayIdx = DISP_UNDEF;
    pCurDispItems = aDispItems + displayIdx;
    pCurDispItems->pError->setValue(status < 0 ? "ERROR" : "OK");
  }
  pCurDispItems->pDevIP->setValue(aIPaddress);
  pCurDispItems->pDevTitle->updateText(aDevName);
  display.refresh(displayIdx);
  
}

unsigned long lastTime = 0;
void
doBlinkOnboardLED(unsigned long time)
{
  if (250 < (time - lastTime)) {
    static int obBoardLED = LOW;
    digitalWrite(LED_BUILTIN, obBoardLED);
    digitalWrite(HEARTBEAT, obBoardLED);
    obBoardLED = obBoardLED == LOW ? HIGH : LOW;
    lastTime = time;
  }
}

int
handleTouchSensor(sTouchSensor *pTS, bool pressed)
{
  static unsigned long wasPressed;
  static char aName[120/6 - 6];

  if (pressed) { wasPressed |= (1 << pTS->pin); }
  else { wasPressed &= ~(1 << pTS->pin); }

  sprintf(aName, "0:%c 1:%c 2:%c", wasPressed & (1 << TOUCH0) ? 'P' : 'r', wasPressed & (1 << TOUCH1) ? 'P' : 'r', wasPressed & (1 << TOUCH2) ? 'P' : 'r');
  if (pCurDispItems && pCurDispItems->pDevTitle) {
    pCurDispItems->pDevTitle->updateText(aName);
  }
  return 0;
}

void 
toggleDisplay(unsigned long thisTime, int mv1, int mv2)
{
  static int toggleDisplayTime;
  static bool state;
  
  int toggle1 = mv1 < 3300 ? 0 : (mv1 < 9000 ? 200 : (mv1 < 10000 ? 500 : 0));
  int toggle2 = mv2 < 3300 ? 0 : (mv2 < 5000 ? 200 : (mv2 <  6000 ? 500 : 0));
  
  if (toggle1 || toggle2 || state) {
    int freq = toggle1 < toggle2 ? toggle1 : toggle2;
    if ((toggleDisplayTime + freq) < thisTime) {
      state = !state;
      display.invertDisplay(state);
      toggleDisplayTime = thisTime;
      digitalWrite(ALARMLED, state);
    }
  }
}

void loop(void)
{
  if (!CHECK(WITH_DISPLAY)) {
    unsigned long thisTime = millis();
    doBlinkOnboardLED(thisTime);
    return;
  }

  // read local data
  sensor.Readout();
  int pressure = sensor.GetPres() / 100;
  int temp = 10*sensor.GetTemp();
  int adc  = analogRead(ADC1);

  switch (settingsFlags & (STATE_PIPE|STATE_BLOW|STATE_UNSET)) {
  case STATE_PIPE: handlePipe(pressure, temp, adc); break;
  case STATE_BLOW: handleBlow(pressure, temp, adc); break;
  default:
  case STATE_UNSET: handleUnset(pressure, temp, adc); break;
  }
}
