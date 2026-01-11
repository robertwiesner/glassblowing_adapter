/*
 * Licensed under Apache 2.0
 * Text version: https://www.apache.org/licenses/LICENSE-2.0.txt
 * SPDX short identifier: Apache-2.0
 * OSI Approved License: https://opensource.org/licenses/Apache-2.0
 * Author: Robert Wiesner
 *
 * General Setting (GPIO) and shared data for header and base functions
 */

#ifndef SETTING_H
#define SETTING_H

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#if RP2040W
  #include <AsyncWebServer_RP2040W.h>
  #include <LEAmDNS.h>
#elif ESP32_S3
  #include <AsyncTCP.h>
  #include <ESPAsyncWebServer.h>
  #include <ESPmDNS.h>
#else
  #error Select either RP2040W or ESP32_S3
#endif

#include <MS5xxx.h>
#include <DRV8837.h>
#include "Display.h"

#define PROM_I2C Wire
#define PRESSURE_I2C Wire1
#if RP2040W

#define DORESTART rp2040.restart()

#define DISP_I2C false /* false == Wire, ture == Wire1 */
#define MOTO_1 9
#define MOTO_2 12
#define MOTO_S 8
#define VENT_1 3
#define VENT_2 4
#define VENT_S 2

#define BATT_VOLT ADC1
#define I2C0_SDA 20
#define I2C0_SCL 21
#define I2C1_SDA 11
#define I2C1_SCL 10
#elif ESP32_S3

#include <Update.h>
#define DORESTART  ESP.restart()

#define MOTO_1 10
#define MOTO_2 11
#define MOTO_S 9

#define VENT_1 48
#define VENT_2 45
#define VENT_S 47
#define BATT_VOLT ADC1

#define DISP_I2C false /* false == Wire, ture == Wire1 */
#define I2C0_SDA 46
#define I2C0_SCL 3
#define I2C1_SDA 39
#define I2C1_SCL 38

#define HEARTBEAT 1
#define ALARMLED  2

#define TOUCH0 12
#define TOUCH1 13
#define TOUCH2 14

#define ADC0 5
#define ADC1 6
#define ADC2 7

#define ADC2MV(a) ((3230 * (a) * 11) / 4096)
extern int handleTouchSensor(sTouchSensor *pTS, bool);

sTouchSensor aTouchSensor[] = {
    {TOUCH0, 16, 1000, 500, handleTouchSensor},
    {TOUCH1, 16, 1000, 500, handleTouchSensor},
    {TOUCH2, 16, 1000, 500, handleTouchSensor},
    {-1}
};

sMenueInfo aMainMenu[] = {
    {0x1000, "Main"},
    {0x2000, "Settings"},
    {0x3000, "Exit"},
    {0}
};

#else
#error Add here
#endif

#define BLINKEST(a,b,c) delay(a); for (int idx = 0; idx < b; idx++) { digitalWrite(HEARTBEAT, 1); delay(c);  digitalWrite(HEARTBEAT, 0); delay(c); }

#define WITH_DISPLAY  (1 << 0)
#define WITH_EEPROM   (1 << 1)
#define WITH_PRESSURE (1 << 2)
#define STATE_UNSET   (1 << 3)
#define STATE_PIPE    (1 << 4)
#define STATE_BLOW    (1 << 5)
#define DISP_UNDEF 0
#define DISP_SERVER 1  
#define DISP_CLIENT 2

int settingsFlags = WITH_DISPLAY | WITH_EEPROM | WITH_PRESSURE;
#define CHECK(a) ((settingsFlags & (a)) != 0)
#define CLEAR(a) (settingsFlags &= ~(a))
#define SET(a)   (settingsFlags |= (a))

#include "icon.h"

struct sUDPData {
  uint16_t time;
  uint16_t mvolt;
  uint16_t mbar;
  uint16_t temp;
} ;

int sensorAddr;


struct sDispItem {
    cIconItem *pIconItem;
    cTextItem *pDevTitle;
    cTextStrItem *pDevIP, *pError;
    cTextIntItem *apTxtIntItem[6];
} aDispItems[3], *pCurDispItems = nullptr;


cDisplayItem *genDefaultItems(struct sDispItem *pCDI, const char *pTitle)
{
    static const char aVersion[] = VERSION(MAJOR, MINOR, PATCH);
    cDisplayItem *pRet = new cBoxItem(0, 0, 127, 13);
    cDisplayItem *pPrev = pRet;

    pPrev = new cTextItem(128 - 6*strlen(aVersion) - 3,   3, aVersion,   pPrev);
    pPrev = pCDI->pDevTitle = new cTextItem(4,   3, pTitle,   pPrev);
    pPrev = pCDI->pDevIP = new cTextStrItem(0, 2*8, "IP: %s",  pPrev);
    pPrev = pCDI->pError = new cTextStrItem(0, 7*8, "Stat: %s", pPrev);
    
    pCurDispItems->pError->setValue("n/a");

    memset(pCDI->apTxtIntItem, 0, sizeof(pCDI->apTxtIntItem));
    return pRet;
}

extern cDisplay display;
extern struct sUDPData UDPdata;

extern char aDevName[17];
extern char aSSID[17];
extern char aPassword[17];
extern char aIPaddress[17];
extern int state;
extern int displayIdx;
extern IPAddress serverAddr;
extern DRV8837 motor;
extern DRV8837 vent;
extern MS5xxx sensor;
extern cM24C02 eeprom;

extern void toggleDisplay(unsigned long thisTime, int mv1, int mv2);
extern void handleUploadRestart(AsyncWebServerRequest *pReq);
extern void handleUploadFile(AsyncWebServerRequest *pReq, String filename, size_t index, uint8_t *data, size_t len, bool final);
extern void setupAP(const char *pSSID, const char *pPassword, char *pIPaddress);
extern void getRequest(AsyncWebServerRequest *pReq, const char *pName, char *pBuffer);
// EEPROM offsets
#define EEPROM_MAJOR 0
#define EEPROM_MINOR 1
#define EEPROM_PATCH 2
#define EEPROM_DEV_NAME  16
#define EEPROM_PASSWORD  (EEPROM_DEV_NAME + 16)
#define EEPROM_SSID_NAME (EEPROM_PASSWORD + 16)

#define VAL_TIME  0x4000
#define VAL_MVOLT 0x5000
#define VAL_MBAR  0x6000
#define VAL_TEMP  0x7000

#endif
