// Receive-only UART with automatic baud rate and RX-pin detection.
//
// The TX pin is never driven, so it is safe to probe both Grove signal pins:
// whichever orientation the lead is wired in, one of them will carry data.
#pragma once

#include <Arduino.h>

class UartScanner {
 public:
  UartScanner(HardwareSerial& serial, const uint32_t* bauds, size_t baudCount,
              uint32_t dwellMs, uint32_t rescanAfterMs);

  void start(int rxPinA, int rxPinB = -1); // rxPinA is tried first; -1 = single pin
  void stop();

  // Call every loop. Returns true when the port was reconfigured, so the
  // caller should reset any partially parsed packet state.
  bool poll();

  // Parser calls this for every complete, checksum-valid message.
  void noteValid();

  HardwareSerial& serial() { return serial_; }
  bool running() const { return running_; }
  bool locked() const { return locked_; }
  uint32_t baud() const;
  int rxPin() const;
  size_t comboIndex() const { return combo_; }
  size_t comboCount() const { return baudCount_ * pinCount_; }
  uint32_t msSinceValid() const; // UINT32_MAX if never

 private:
  void apply();
  void releasePins();

  HardwareSerial& serial_;
  const uint32_t* bauds_;
  size_t baudCount_;
  uint32_t dwellMs_;
  uint32_t rescanAfterMs_;
  int pins_[2] = {-1, -1};
  size_t pinCount_ = 2;
  size_t combo_ = 0;
  bool running_ = false;
  bool locked_ = false;
  uint32_t comboStartMs_ = 0;
  uint32_t validInCombo_ = 0;
  uint32_t lastValidMs_ = 0;
  bool everValid_ = false;
};
