/*
 * Licensed under Apache 2.0
 * Text version: https://www.apache.org/licenses/LICENSE-2.0.txt
 * SPDX short identifier: Apache-2.0
 * OSI Approved License: https://opensource.org/licenses/Apache-2.0
 * Author: Robert Wiesner
 * define Icons for the display functions
 */
#ifndef ICON_H
#define ICON_H

const uint8_t PROGMEM 
aaIcon[][11] = {
  { // 0
    0B11111111,
    0B10000001,
    0B10000001,
    0B10000001,
    0B11111111,
    0B11111111,
    0B11111111,
    0B10000001,
    0B10000001,
    0B10000001,
    0B11111111,
  },
  { // 1 Motor On
    0B00011000,
    0B00011000,
    0B00111100,
    0B00111100,
    0B01111110,
    0B11011011,
    0B11011011,
    0B00011000,
    0B00011000,
    0B00011000,
    0B11111111,
  },
  { // 2 Vent On
    0B11111111,
    0B00011000,
    0B00011000,
    0B00011000,
    0B11011011,
    0B11011011,
    0B01111110,
    0B01111110,
    0B00111100,
    0B00011000,
    0B00011000,
  },
  { // 3
    0B11100111,
    0B10000001,
    0B10000001,
    0B10000001,
    0B10000001,
    0B10000001,
    0B10000001,
    0B10000001,
    0B10000001,
    0B10000001,
    0B11100111,
  },
};

#endif
