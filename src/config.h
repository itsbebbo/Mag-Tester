// Tunable settings for the Mag / GPS tester.
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace cfg {

// ---- UART (GPS and Arduino magnetometer) -----------------------------------
// The tester only ever *listens* on Port A (the TX pin is never driven), so it
// is safe to probe both Grove signal pins to find which one carries data. This
// copes with either lead orientation.
constexpr size_t UART_RX_BUFFER = 4096;
// Bytes in the UART hardware FIFO before they are moved to the RX buffer. The
// framework default at >57600 baud is 120, which on a gapless stream delivers
// data in ~10 ms bursts; a small value keeps latency to about one packet.
constexpr uint8_t UART_RX_FIFO_FULL = 8;

// HGLRC M100: factory default is 115200 baud, but a unit that has been used
// with a flight controller may have been reconfigured, so try common rates.
constexpr uint32_t GPS_BAUDS[] = {115200, 9600, 38400, 57600, 230400};
constexpr uint32_t GPS_SCAN_DWELL_MS = 1500;   // time spent listening per baud/pin combination
constexpr uint32_t GPS_DATA_TIMEOUT_MS = 2500; // no valid message for this long -> NOT OKAY
constexpr uint32_t GPS_RESCAN_AFTER_MS = 6000; // no valid message for this long -> start scanning again
// false: OK means "module is talking valid NMEA/UBX" (useful indoors).
// true:  OK additionally requires a 2D/3D position fix.
constexpr bool GPS_OK_REQUIRES_FIX = false;

// Arduino magnetometer: 8-byte binary frames, see arduino_mag_test.h.
// The Arduino sketch runs at 115200 (tried first); other rates are fallbacks.
constexpr uint32_t ARD_BAUDS[] = {115200, 9600, 57600, 38400};
constexpr uint32_t ARD_SCAN_DWELL_MS = 1500;
constexpr uint32_t ARD_DATA_TIMEOUT_MS = 2000;
constexpr uint32_t ARD_RESCAN_AFTER_MS = 5000;
// Start Test: give up with "No connection" if no data within one full baud sweep.
constexpr uint32_t ARD_CONNECT_TIMEOUT_MS = 6500;
// Start Test: values unchanged for this long after the first frame -> Magnetometer fault.
constexpr uint32_t ARD_STATIC_CHECK_MS = 3000;

// ---- I2C (HGLRC 5883 magnetometer) -------------------------------------------
constexpr uint32_t I2C_FREQ = 100000;      // conservative for long Grove leads
constexpr uint32_t MAG_POLL_MS = 50;       // 20 Hz
constexpr uint32_t MAG_REDETECT_MS = 1000; // retry detection this often when no chip found
constexpr uint8_t MAG_MAX_READ_FAILS = 3;  // consecutive failures before declaring the chip lost

// ---- Magnetometer checks (shared by both mag tests) ---------------------------
constexpr uint32_t MAG_STUCK_MS = 3000; // identical raw readings for this long -> "data stuck"

// ---- UI ----------------------------------------------------------------------
constexpr uint32_t UI_REFRESH_MS = 200;

} // namespace cfg
