
/*
 * Licensed under Apache 2.0
 * Text version: https://www.apache.org/licenses/LICENSE-2.0.txt
 * SPDX short identifier: Apache-2.0
 * OSI Approved License: https://opensource.org/licenses/Apache-2.0
 * Author: Robert Wiesner
 * 
 * Handle the Unprogrammed or reset devices
 * 
 */
#ifndef SERVER_UNSET_H
#define SERVER_UNSET_H 


#define UNSET_MBAR  apTxtIntItem[0]
#define UNSET_MVOLT apTxtIntItem[1]
#define UNSET_TOUCH0 apTxtIntItem[2]
#define UNSET_TOUCH1 apTxtIntItem[3]
#define UNSET_TOUCH2 apTxtIntItem[4]

extern WiFiUDP Udp;
void handleUnset(int pressure, int temp, int adc)
{
  
  int mV = ADC2MV(adc);
  pCurDispItems->UNSET_MBAR->setValue(pressure); // mBar
  pCurDispItems->UNSET_MVOLT->setValue(mV); // mV

  pCurDispItems->UNSET_TOUCH1->setValue(touchRead(TOUCH1) / 1024);
  pCurDispItems->UNSET_TOUCH2->setValue(touchRead(TOUCH2) / 1024);
  pCurDispItems->UNSET_TOUCH0->setValue(touchRead(TOUCH0) / 1024);
  display.refresh(displayIdx);
}

cDisplayItem *
initUndef()
{
    pCurDispItems = aDispItems + DISP_UNDEF;
    cDisplayItem *pRet = genDefaultItems(pCurDispItems, "BOOTING");
    cDisplayItem *pPrev = pRet;

    pPrev = pCurDispItems->UNSET_MVOLT = new cTextIntItem(0, 3*8, "Bat: %1d.%03d V", 3, pPrev);
    pPrev = pCurDispItems->UNSET_MBAR = new cTextIntItem(0, 4*8, "Pa: %6d Pa", pPrev);
    
    pPrev = pCurDispItems->UNSET_TOUCH0 = new cTextIntItem( 0, 5*8, "T0: %3d", pPrev);
    pPrev = pCurDispItems->UNSET_TOUCH1 = new cTextIntItem(64, 5*8, "T1: %3d", pPrev);
    pPrev = pCurDispItems->UNSET_TOUCH2 = new cTextIntItem( 0, 6*8, "T2: %3d", pPrev);
    return pRet;
}

void setupUnset()
{
    displayIdx = DISP_UNDEF;
    pCurDispItems = aDispItems + displayIdx;
    strcpy(aSSID, "PB_UNSET");
    strcpy(aPassword, "0123456789");
    SET(STATE_UNSET);
    BLINKEST(500, 6, 100);
}


void handleUnsetRootRequest (AsyncWebServerRequest *pReq)
{
    static char aBuffer[4096];
    static char serverIndexHead[] = 
    "<!DOCTYPE HTML>"
    "<html>"
    "<head>"
      "<title>Blow/Pipe Configuration</title>"
      "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "</head>"
    "<body>"
      "<h1>Blow/Pipe Server</h1>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
          "<input type='file' name='update'>"
          "<input type='submit' value='Update'>"
      "</form>"
      "<hr>"
      "<form method='GET' action='/apply' enctype='multipart/form-data'>"
          "<label for='lname'>DeviceName:</label><input type='text' id='lname' name='name' value=''>"
          "<br>"
          "<label for='lssid'>SSID:</label>      <input type='text' id='lssid' name='ssid' value=''>"
          "<br>"
          "<label for='lpass'>Password:</label>  <input type='text' id='lpass' name='password' value=''>"
          "<br>"
          "<input type='submit' value='Apply'>"
          "<input type='reset' value='Reset EEPROM'>"
      "</form>"
      "Device Name, SSID, and Password settings:<p>"
      "Device name starts with:"
      "<ul>"
        "<li>P or p : set as a pipe server (requires DeviceName and Password)</li>"
        "<li>B or b : set as a blow client (requires DeviceName, SSID (corresponding pipe name), and Password)</li>"
        "<li>R, s, R, or S : reset device (clears DeviceName, SSID, and Password)</li>"
      "</ul>"
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
        aSmall[idx & 0xf] = val < 32  || val > 127? '.' : val;
        sprintf(pPtr, " 0x%02X", val);
        pPtr += strlen(pPtr);
    }

    strcpy(pPtr, aSmall);
    pPtr += strlen(pPtr);

    strcpy(pPtr, serverIndexTail);

    pReq->send_P(200, "text/html", aBuffer);
}


void
handleUnsetApplyRequest (AsyncWebServerRequest *pReq, char *pS, char *pP, char *pD)
{   
    bool reboot = false;

    getRequest(pReq, "ssid", pS);
    getRequest(pReq, "password", pP);
    getRequest(pReq, "name", pD);

    if (strlen(pP) < 8) { pP[0] = '\0'; }

    switch (pD[0]) {
    case 'b':
    case 'B':
        if (pS[0] && pP[0] && pD[0]) {
            eeprom.setBuffer(EEPROM_DEV_NAME, (uint8_t *)pD, 16);
            eeprom.setBuffer(EEPROM_PASSWORD, (uint8_t *)pP, 16);
            eeprom.setBuffer(EEPROM_SSID_NAME, (uint8_t *)pS, 16);
            reboot = true;
            pCurDispItems->pDevTitle->updateText("Set Blow");
            display.refresh(DISP_UNDEF);
            delay(1000);
        } else {
            pD[0] = pP[0] = pS[0] = 0;
            eeprom.setBuffer(EEPROM_DEV_NAME, (uint8_t *)pD, 1);
            eeprom.setBuffer(EEPROM_PASSWORD, (uint8_t *)pP, 1);
            eeprom.setBuffer(EEPROM_SSID_NAME, (uint8_t *)pS, 1);
            reboot = true;
            pCurDispItems->pDevTitle->updateText("Clear Blow");
            display.refresh(DISP_UNDEF);
            delay(1000);
        }
        break;
    case 'p':
    case 'P':
        if (pD[0] && pP[0]) {
            eeprom.setBuffer(EEPROM_DEV_NAME, (uint8_t *)pD, 16);
            eeprom.setBuffer(EEPROM_PASSWORD, (uint8_t *)pP, 16);
            pS[0] = 0;
            eeprom.setBuffer(EEPROM_SSID_NAME, (uint8_t *)pS, 1);
            reboot = true;
            delay(1000);
        } else {
            eeprom.setBuffer(EEPROM_DEV_NAME, (uint8_t *)pD, 16);
            eeprom.setBuffer(EEPROM_PASSWORD, (uint8_t *)pP, 16);
            eeprom.setBuffer(EEPROM_SSID_NAME, (uint8_t *)pS, 16);
            reboot = true;
            pCurDispItems->pDevTitle->updateText("Clear Pipe");
            display.refresh(DISP_UNDEF);
            delay(1000);
        }
        break;
    case 'r':
    case 'R':
    case 's':
    case 'S':
        pD[0] = pP[0] = pS[0] = 0;
        eeprom.setBuffer(EEPROM_DEV_NAME, (uint8_t *)pD, 1);
        eeprom.setBuffer(EEPROM_PASSWORD, (uint8_t *)pP, 1);
        eeprom.setBuffer(EEPROM_SSID_NAME, (uint8_t *)pS, 1);
        reboot = true;
        break;
    }

    if (reboot) {
        pReq->send(200, "text/plain", "OK, restarting...");
        pCurDispItems->pDevTitle->updateText("RESTART");
        display.refresh(DISP_UNDEF);
        delay(1000);
        DORESTART;
    } else {
        pReq->send(200, "text/plain", "FAIL, invalid settings - keep going...");
    }
}

void
handleUnsetResetRequest (AsyncWebServerRequest *pReq)
{
    for (int idx = 0; idx < 256; idx++) {
        eeprom.setByte(idx, 0xff);
    }
    delay(1000);
    DORESTART;
}

int setupServerUndef(AsyncWebServer &server, char *pS, char *pP, char *pD, char *pI)
{
    static char *pSSID = pS;
    static char *pPassword = pP;
    static char *pName = pD;
    static char *pIPaddress = pI;

    setupAP(pS, pP, pI);

    MDNS.begin(pSSID);
    // handle "/" request
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *pReq) { handleUnsetRootRequest(pReq); } );

    // handle "/apply" request
    server.on("/apply", HTTP_GET, [](AsyncWebServerRequest *pReq) {handleUnsetApplyRequest(pReq, pSSID, pPassword, pName); } );

    // handle "/update" request
    server.on("/update", HTTP_POST, handleUploadRestart, handleUploadFile );

    // handle "/update" request
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *pReq) { handleUnsetResetRequest(pReq); } );
    server.begin();
    MDNS.addService("http", "tcp", 80);
    return 0;
}

#endif
