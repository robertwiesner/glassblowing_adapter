/*
 * Licensed under Apache 2.0
 * Text version: https://www.apache.org/licenses/LICENSE-2.0.txt
 * SPDX short identifier: Apache-2.0
 * OSI Approved License: https://opensource.org/licenses/Apache-2.0
 * Author: Robert Wiesner
 *
 * Access function for EEPROM code
 * cM24C02(TwoWire, I2Caddr): initialize the EEPROM function
 * readAll(): read the entrier (256 bytes) eeprom
 * get*(): return the cached bytes for the EEPROM
 * set*(): update the chached and EEPROM functions
 */
#include <Wire.h>
#include <stdlib.h>
#ifndef M24C02_H

#define M24C02_H

class cM24C02 {
    TwoWire &wire;
    uint8_t deviceAddress;
    unsigned char aData[256];
    public:
    cM24C02(TwoWire &w, uint8_t da = 0x50) : wire(w), deviceAddress(da) {
    }

    void readAll() {
        // read the entire EEPROM into aData array
        for (int off = 0; off < sizeof(aData); ) {
            wire.beginTransmission(deviceAddress);
            wire.write((uint8_t)off); // start at address 0x00
            wire.endTransmission(false);
            int byteCount = wire.requestFrom(deviceAddress, ((off + 8) < sizeof(aData)) ? 8 : (sizeof(aData) - off));
            wire.readBytes(aData + off, byteCount);
            off += byteCount;
        }
    }
    ~cM24C02() {}
    int getByte(int addr) {
        if (addr < 0 || addr >= sizeof(aData)) {
            return -1; // out of bounds
        }
        return aData[addr];
    }
    int getShort(int addr) {
        if (addr < 0 || addr + 1 >= sizeof(aData)) {
            return -1; // out of bounds
        }
        return (aData[addr] << 8) | aData[addr + 1];
    }
    int getInt(int addr) {
        if (addr < 0 || addr + 3 >= sizeof(aData)) {
            return -1; // out of bounds
        }
        return (aData[addr] << 24) | (aData[addr + 1] << 16) | (aData[addr + 2] << 8) | aData[addr + 3];
    }
    char *getBuffer(int addr, char* buf, int len) {
        if (addr < 0 || addr + len > sizeof(aData)) {
            return nullptr; // out of bounds
        }
        for (int i = 0; i < len; i++) {
            buf[i] = aData[addr + i];
        }
        buf[len] = 0;
        return buf;
    }

    void setByte(int addr, unsigned char val) {
        if (addr < 0 || addr >= sizeof(aData)) {
            return; // out of bounds
        }
        aData[addr] = val;
        wire.beginTransmission(deviceAddress);
        wire.write((uint8_t)addr);
        delay(1);
        wire.write(val);
        delay(1);
        wire.endTransmission();

        delay(10); // EEPROM write delay
    }

    void setShort(int addr, unsigned short val) {
        if (addr < 0 || addr >= sizeof(aData)) {
            return; // out of bounds
        }
        if (14 < (addr & 0xf)) { // does not fit into 16 bytes block
            setByte(addr, val);
            setByte(addr+1, val >> 8);
            return; // address must be even for short
        }
        aData[addr] = val;
        aData[addr + 1] = val >> 8;

        wire.beginTransmission(deviceAddress);
        wire.write((uint8_t)addr);
        wire.write((uint8_t)aData[addr]);
        wire.write((uint8_t)aData[addr + 1]);
        wire.endTransmission();
        delay(10); // EEPROM write delay
    }
    void setInt(int addr, unsigned int val) {
        if (addr < 0 || addr >= sizeof(aData)) {
            return; // out of bounds
        }
        
        if (12 < (addr & 0xf)) { // does not fir into 16 bytes block
            setByte(addr, val);
            setByte(addr+1, val >> 8);  
            setByte(addr+2, val >> 16);
            setByte(addr+3, val >> 24);  
            return;
        }

        // This does fit into 16 bytes block
        aData[addr] = val;
        aData[addr + 1] = val >> 8;
        aData[addr + 2] = val >> 16;
        aData[addr + 3] = val >> 24;

        wire.beginTransmission(deviceAddress);
        wire.write((uint8_t)addr);
        wire.write((uint8_t)aData[addr]);
        wire.write((uint8_t)aData[addr + 1]);
        wire.write((uint8_t)aData[addr + 2]);
        wire.write((uint8_t)aData[addr + 3]);
        wire.endTransmission();
        delay(10); // EEPROM write delay
    }
    void setBuffer(int addr, const unsigned char* buf, int len) {
        if (addr < 0 || addr + len > sizeof(aData)) {
            return; // out of bounds
        }

        while (0 < len--) {
            setByte(addr++, *buf++);
        }
#if 0
        memcpy(aData + addr, buf, len);

        // For larger writes, do it in chunks of up to 16 bytes
        while (len > 0) {
            int chunkSize = 16 - (addr & 0xf);
            chunkSize = (len < chunkSize) ? len : chunkSize;
            wire.beginTransmission(deviceAddress);
            wire.write((uint8_t)(addr));
            for (int i = 0; i < chunkSize; i++) {
                wire.write((uint8_t) aData[addr + i]);
            }
            wire.endTransmission();
            delay(10); // EEPROM write delay
            len  -= chunkSize;
            addr += chunkSize;
        }
#endif
        return;
    }
};

#endif // M24C02_H
