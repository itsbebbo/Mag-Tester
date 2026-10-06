# Mag / GPS Tester (M5Stack CoreS3)

Touchscreen tester for HGLRC M100 GPS/compass modules and an Arduino-based magnetometer.
Built with PlatformIO, M5Unified/M5GFX and LVGL 8.3.

## Ports

| Test                      | Port   | Pins                 | Bus            |
|---------------------------|--------|----------------------|----------------|
| M100 GPS                  | Port C | G18 = RX, G17 = TX   | UART (RX only) |
| 5883 Magnetometer (M100)  | Port B | G8 = SCL, G9 = SDA   | I2C            |
| Arduino Magnetometer      | Port A | G1 / G2              | UART (RX only) |

The ports have 5V enabled at startup. The tester never drives a UART TX line.
It listens on both signal pins and tries each baud rate in turn, so a crossed
lead or a reconfigured baud rate is found automatically. On the I2C port it
also tries SDA/SCL swapped.

## Status logic

Each test screen shows a green **OK** or red **NOT OKAY** banner with a one-line
reason, and a **Home** button.

- **GPS**: OK when checksum-valid NMEA or UBX messages are arriving. It does not need
  a fix by default, so it can be tested indoors. Set `GPS_OK_REQUIRES_FIX` in
  `src/config.h` to require one. It decodes NMEA and also UBX NAV-PVT, for modules
  that a flight controller has left in UBX mode.
- **5883**: detects HMC5883L (0x1E), QMC5883L (0x0D) or QMC5883P (0x2C). OK when
  reads succeed, the sensor isn't saturated, the readings keep changing, and
  |B| is between 10 and 200 uT.
- **Arduino**: OK when valid packets are arriving. **The packet parser is a
  placeholder** (`ArduinoMagTest::parsePacket` in `src/arduino_mag_test.cpp`).
  It accepts any text line with three numbers as X, Y, Z until the real format is defined.

Timeouts, baud lists and thresholds are all in `src/config.h`.

## Building

This project needs PlatformIO Core 6.1.19 or later (pioarduino platform 55.03.311).
Build from PowerShell or the VS Code PlatformIO UI, not Git Bash, because the
ESP-IDF tool installer rejects MSys/MinGW shells.

```
pio run -t upload
```
