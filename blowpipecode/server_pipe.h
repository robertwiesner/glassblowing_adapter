/*
 * Licensed under Apache 2.0
 * Text version: https://www.apache.org/licenses/LICENSE-2.0.txt
 * SPDX short identifier: Apache-2.0
 * OSI Approved License: https://opensource.org/licenses/Apache-2.0
 * Author: Robert Wiesner
 * 
 * Handle the Blowpipe Server functions, receive the data and control the motor/vent
 */
#ifndef SERVER_PIPE_H
#define SERVER_PIPE_H 

extern WiFiUDP Udp;

#define SERVER_UDPSIZE apTxtIntItem[0]
#define SERVER_UDPCNT  apTxtIntItem[1]
#define SERVER_MBAR_L  apTxtIntItem[2]
#define SERVER_MBAR_R  apTxtIntItem[3]
#define SERVER_MVOLT_L  apTxtIntItem[4]
#define SERVER_MVOLT_R  apTxtIntItem[5]

void
handlePipe(int pressure, int temp, int adc)
{
  static int baselinePressure;
  static int nominalLocal  = 0;
  static int nominalRemote = 0;
  static int enableMotor = 0;
  static int packageCnt;
  static unsigned long lastTime;
  unsigned long thisTime = millis();
  // check for data receive
  int packetSize = Udp.parsePacket();
  int mV = ADC2MV(adc);

  if (0 < packetSize) {
    uint16_t aPackage[4];
    int n = Udp.read((uint8_t *) aPackage, sizeof(aPackage));

    packageCnt += n/2;
    
    for (int idx = 0; 2*idx < n; idx++) {
      switch (aPackage[idx] & 0xf000) {
      case VAL_TIME:
        UDPdata.time = aPackage[idx] & 0x0fff;
        break;
      case VAL_MVOLT:
        UDPdata.mvolt = ADC2MV(aPackage[idx] & 0x0fff);
        pCurDispItems->SERVER_MVOLT_R->setValue(UDPdata.mvolt);
        break;
      case VAL_MBAR:
        UDPdata.mbar = aPackage[idx] & 0x0fff;
        if (enableMotor < 16) {
          enableMotor = enableMotor + 1;
          baselinePressure += UDPdata.mbar;
        } else if (enableMotor == 16) {
          enableMotor = enableMotor + 1;
          baselinePressure /= 16;
          nominalRemote = baselinePressure + 5*(UDPdata.mbar - baselinePressure);
        } else {
          nominalRemote = baselinePressure + 5*(UDPdata.mbar - baselinePressure);
        }
        static char aMsg[16];
        sprintf(aMsg, "%d/%d", UDPdata.mbar, baselinePressure);
        pCurDispItems->pError->setValue(aMsg);
        pCurDispItems->SERVER_MBAR_R->setValue(nominalRemote /*UDPdata.mbar*/);
        break;
      case VAL_TEMP:
        UDPdata.temp = aPackage[idx] & 0x0fff;
        break;
      }
    }
    pCurDispItems->SERVER_UDPSIZE->setValue(n);
  } else {
    if ((lastTime + 500) <  thisTime) {
      pCurDispItems->SERVER_UDPSIZE->setValue(-1);
    }
  }

  toggleDisplay(thisTime, mV, UDPdata.mvolt);
  pCurDispItems->SERVER_UDPCNT->setValue(packageCnt);
  pCurDispItems->SERVER_MBAR_L->setValue(pressure); 
  pCurDispItems->SERVER_MVOLT_L->setValue(mV);
  display.refresh(displayIdx);
  
  if (16 <= enableMotor && nominalRemote < (pressure - 10)) { // Vent On
    pCurDispItems->pIconItem->setValue(aaIcon[2]);
    motor.run(0);
    motor.setAwake(false);
    vent.setAwake(true);
    vent.run(-255);
  } else if (16 <= enableMotor && (pressure+10) < nominalRemote) { // Motor On
    pCurDispItems->pIconItem->setValue(aaIcon[1]);
    vent.run(0);
    vent.setAwake(false);
    motor.setAwake(true);
    motor.run(255);
  } else {
    pCurDispItems->pIconItem->setValue(aaIcon[0]); // switch off both
    vent.run(0);
    vent.setAwake(false);
    motor.run(0);
    motor.setAwake(false);
  }
  
  lastTime = thisTime;
}

cDisplayItem *
initPipe()
{
    pCurDispItems = aDispItems + DISP_SERVER;
    cDisplayItem *pRet = genDefaultItems(pCurDispItems, "SERVER");
    cDisplayItem *pPrev = pRet;

    pPrev = pCurDispItems->SERVER_UDPSIZE = new cTextIntItem(0, 3*8, "UDP: %d", pPrev);
    pPrev = pCurDispItems->SERVER_UDPCNT  = new cTextIntItem(8*6, 3*8, "(%d)", pPrev);
    pPrev = pCurDispItems->SERVER_MBAR_L  = new cTextIntItem(      0, 5*8, "mBa: %4d->", pPrev);
    pPrev = pCurDispItems->SERVER_MBAR_R  = new cTextIntItem((7+4)*6, 5*8, "%4d", pPrev);
    pPrev = pCurDispItems->SERVER_MVOLT_L = new cTextIntItem(      0, 6*8, "mV: %5d/", pPrev);
    pPrev = pCurDispItems->SERVER_MVOLT_R = new cTextIntItem((5+5)*6, 6*8, "%5d", pPrev);
    pPrev = pCurDispItems->pIconItem = new cIconItem(112, 5*8-1, 8, 11, pPrev);
    pCurDispItems->SERVER_MBAR_R->setInverted(true);
    pCurDispItems->SERVER_MVOLT_R->setInverted(true);

    return pRet;
}

void setupPipe()
{
    displayIdx = DISP_SERVER;
    pCurDispItems = aDispItems + displayIdx;
    pCurDispItems->SERVER_MBAR_R->setValue(0);
    SET(STATE_PIPE);
    BLINKEST(500, 4, 100);
}

void handleServerRootRequest (AsyncWebServerRequest *pReq)
{
    static char aBuffer[4096];
    static char serverIndexHead[] = 
    "<!DOCTYPE HTML>"
    "<html>"
    "<head>"
      "<title>Pipe Configuration</title>"
      "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "</head>"
    "<body>"
      "<h1>Pipe Server</h1>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
    "<form method='GET' action='/reset' enctype='multipart/form-data'>"
    "<input type='submit' value='Reset'>"
      "<br>Compiled: " __DATE__ ", " __TIME__
      "<br><hr>EEPROM: <p style=\"font-family:'Courier New'\">";
    static char serverIndexTail[] = 
    "</p></body>"
    "</html>";

    strcpy(aBuffer, serverIndexHead);
    char *pPtr = aBuffer + strlen(serverIndexHead);
    char aSmall[20] = ": 0123456789abcdef";
    for (int idx = 0; idx < 256; idx++) {
        if ((idx & 0xf) == 0) {
            if (idx) {
                strcpy(pPtr, aSmall);
                pPtr += strlen(pPtr);
            }
            sprintf(pPtr, "<br>0x%02X: ", idx);
            pPtr += strlen(pPtr);
        }
        int val = eeprom.getByte(idx);
        aSmall[2+(idx & 0xf)] = val < 32  || val > 127? '.' : val;
        sprintf(pPtr, " 0x%02X", val);
        pPtr += strlen(pPtr);
    }

    strcpy(pPtr, aSmall);
    pPtr += strlen(pPtr);

    strcpy(pPtr, serverIndexTail);

    pReq->send_P(200, "text/html", aBuffer);
}

void setupServerPipe(AsyncWebServer &server, const char *pHost, const char *pPassword, char *pIPaddress)
{
    static const char* serverResetAndReboot = 
    "</body>"
    "</html>";

    setupAP(pHost, pPassword, pIPaddress);

    MDNS.begin(pHost);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *pReq) { handleServerRootRequest(pReq);} );

    server.on("/reset",
        HTTP_GET,
        [](AsyncWebServerRequest *pReq) { 
            uint8_t aEmpty[1] = {0};
            eeprom.setBuffer(EEPROM_DEV_NAME, aEmpty, 1);
            eeprom.setBuffer(EEPROM_PASSWORD, aEmpty, 1);
            eeprom.setBuffer(EEPROM_SSID_NAME, aEmpty, 1);
            pReq->send(200, "text/plain", "OK, restarting...");
            delay(1000);
            DORESTART;
        }
    );
    server.on(
        "/update",
        HTTP_POST, 
        handleUploadRestart,
        handleUploadFile
    );
    server.on(
        "/reset",
        HTTP_GET, 
        [](AsyncWebServerRequest *pReq) {
            uint8_t aEmpty[2] = {0xff, 0xff};
            pReq->send(200, "text/html", serverResetAndReboot);
            delay(100);
            eeprom.setBuffer(EEPROM_DEV_NAME, aEmpty, 2);
            DORESTART;
        }
    );

    server.begin();
    MDNS.addService("http", "tcp", 80);
}

#endif
