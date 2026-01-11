/*
 * Licensed under Apache 2.0
 * Text version: https://www.apache.org/licenses/LICENSE-2.0.txt
 * SPDX short identifier: Apache-2.0
 * OSI Approved License: https://opensource.org/licenses/Apache-2.0
 * Author: Robert Wiesner
 * 
 * Handle the Blow Client functions, send the data to the Blowpipe server
 * 
 */
#ifndef CLIENT_BLOW_H
#define CLIENT_BLOW_H 1

#define CLIENT_MBAR  apTxtIntItem[0]
#define CLIENT_MVOLT apTxtIntItem[1]

extern WiFiUDP Udp;

void
handleBlow(int pressure, int temp, int adc)
{
  static unsigned long lastTime;
  uint16_t aPackage[4];
  unsigned long thisTime = millis();
  int mV = ADC2MV(adc);

  if (250 < (thisTime - lastTime)) {
    // Send UDP package
    uint16_t aPackage[4];
    lastTime = thisTime;
    aPackage[0] = VAL_TIME  | ((lastTime >> 8) & 0xfff);
    aPackage[1] = VAL_MVOLT | (adc & 0xfff);
    aPackage[2] = VAL_MBAR  | pressure;
    aPackage[3] = VAL_TEMP  | temp;
    pCurDispItems->CLIENT_MBAR->setValue(pressure); // mBar
    pCurDispItems->CLIENT_MVOLT->setValue(mV); // mV
    Udp.beginPacket(serverAddr, 1805);
    Udp.write((const uint8_t *) aPackage, 4*sizeof(aPackage[0]));
    Udp.endPacket();
    
    toggleDisplay(thisTime, 0, mV);
    display.refresh(displayIdx);
  }
}

cDisplayItem *
initBlow()
{   
    pCurDispItems = aDispItems + DISP_CLIENT;
    cDisplayItem *pRet = genDefaultItems(pCurDispItems, "CLIENT");
    cDisplayItem *pPrev = pRet;

    pPrev = pCurDispItems->CLIENT_MBAR  = new cTextIntItem(0, 3*8, "loc: %4d mBar", pPrev);
    pPrev = pCurDispItems->CLIENT_MVOLT = new cTextIntItem(0, 4*8, " mV: %4d mV", pPrev);

    return pRet;
}

void setupBlow()
{
    displayIdx = DISP_CLIENT;
    pCurDispItems = aDispItems + displayIdx;
    SET(STATE_BLOW);
    BLINKEST(500, 3, 100);
}


void setupClientBlow(AsyncWebServer &server, const char *pHost, const char *pPassword)
{
    static const char* serverResetAndReboot = 
        "<!DOCTYPE HTML>"
        "<html>"
        "<head>"
        "<title>Clear device and reboot</title>"
        "</head>"
        "<body>"
        "<h1>Reset and Reboot</h1>"
        "</body>"
        "</html>";
    static const char* serverIndex = 
    "<!DOCTYPE HTML>"
    "<html>"
    "<head>"
      "<title>Blow Configuration</title>"
      "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "</head>"
    "<body>"
      "<h1>Blow Client</h1>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
        "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
    "<form method='GET' action='/reset' enctype='multipart/form-data'>"
    "<input type='submit' value='Reset'>"
    "</body>"
    "</html>";

    MDNS.begin(pHost);
    server.on("/",
        HTTP_GET,
        [](AsyncWebServerRequest *pReq) { pReq->send(200, "text/html", serverIndex);}
    );

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
