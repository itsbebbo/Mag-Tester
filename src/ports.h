// Grove port pin lookup for the CoreS3. Each test owns one port:
//   Port A (G1/G2)   - Arduino magnetometer, UART
//   Port B (G8/G9)   - HGLRC 5883 magnetometer, I2C
//   Port C (G18/G17) - HGLRC M100 GPS, UART
#pragma once

#include <Arduino.h>

namespace ports {

struct Pins {
  int pin1; // Grove pin next to 5V (yellow lead on M5 cables)
  int pin2; // outer signal pin (white lead)
};

void init(); // call once after M5.begin()

Pins portA();
Pins portB();
Pins portC();

} // namespace ports
