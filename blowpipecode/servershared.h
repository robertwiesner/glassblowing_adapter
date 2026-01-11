/*
 * Licensed under Apache 2.0
 * Text version: https://www.apache.org/licenses/LICENSE-2.0.txt
 * SPDX short identifier: Apache-2.0
 * OSI Approved License: https://opensource.org/licenses/Apache-2.0
 * Author: Robert Wiesner
 * 
 * Shared functions: 
 * setupAP: Setting up the Access Point for the Undefined and Pipe mode
 * handleUploadRestart: Handle the restart after firmware update
 * handleUploadFile: Handle the file upload for firmware update
 */

void
setupAP(const char *pSSID, const char *pPassword, char *pIPaddress)
{
#if RP2040W
    WiFi.mode(WIFI_AP);
    WiFi.softAP(pSSID, pPassword);
    strcpy(pIPaddress, WiFi.localIP().toString().c_str());
#elif ESP32_S3
  if (WiFi.softAP(pSSID, pPassword)) {
    strcpy(pIPaddress, WiFi.softAPIP().toString().c_str());
  } else {
    strcpy(pIPaddress, "WIFI-AP bad");
  }
#else
#error Add Support
#endif 
}

void
handleUploadRestart(AsyncWebServerRequest *pReq) 
{
    pReq->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(1000);
    DORESTART;
}

void
handleUploadFile(AsyncWebServerRequest *pReq, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    static File uploadFile;

    if (index == 0) {
        Serial.printf("Upload start: %s\n", filename.c_str());
        // Open file for writing in LittleFS
        uploadFile = LittleFS.open("/" + filename, "w");
        if (!uploadFile) {
            Serial.println("Failed to open file for writing");
            return ;
        }
    }

    // Write received chunk
    if (uploadFile) {
        uploadFile.write(data, len);
    }

    if (final) {
        Serial.printf("Upload complete: %s, size: %u bytes\n", filename.c_str(), (unsigned int)(index + len));
        if (uploadFile) {
            uploadFile.close();
        }
    }
    return;
}

void
getRequest(AsyncWebServerRequest *pReq, const char *pName, char *pBuffer)
{
    const AsyncWebParameter* pParam = pReq->getParam(pName);

    if (pParam) {
        String txt = pParam->value();
        Serial.printf("Received %s: %s\n", pName, txt.c_str());
        if (txt.length() > 0 && txt.length() < 17) {
            txt.toCharArray(pBuffer, 16);
            pBuffer[16] = '\0';
        } 
    } else {
        pBuffer[0] = 0;
    }
}
