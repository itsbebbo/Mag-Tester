#include "uart_scan.h"

#include "config.h"

// Messages needed within one dwell period before a baud/pin combination is accepted.
static constexpr uint32_t kLockCount = 2;

UartScanner::UartScanner(HardwareSerial& serial, const uint32_t* bauds, size_t baudCount,
                         uint32_t dwellMs, uint32_t rescanAfterMs)
    : serial_(serial), bauds_(bauds), baudCount_(baudCount), dwellMs_(dwellMs),
      rescanAfterMs_(rescanAfterMs) {}

void UartScanner::start(int rxPinA, int rxPinB) {
  pins_[0] = rxPinA;
  pins_[1] = rxPinB;
  pinCount_ = rxPinB < 0 ? 1 : 2;
  combo_ = 0;
  locked_ = false;
  everValid_ = false;
  running_ = true;
  apply();
}

void UartScanner::stop() {
  if (!running_) return;
  serial_.end();
  releasePins();
  running_ = false;
  locked_ = false;
}

// Combinations are ordered baud-major: (baud0, pinA), (baud0, pinB), (baud1, pinA), ...
uint32_t UartScanner::baud() const { return bauds_[combo_ / pinCount_]; }
int UartScanner::rxPin() const { return pins_[combo_ % pinCount_]; }

void UartScanner::releasePins() {
  for (size_t i = 0; i < pinCount_; i++) pinMode(pins_[i], INPUT);
}

void UartScanner::apply() {
  serial_.end();
  releasePins();
  serial_.setRxBufferSize(cfg::UART_RX_BUFFER);
  serial_.begin(baud(), SERIAL_8N1, rxPin(), -1); // RX only
  serial_.setRxFIFOFull(cfg::UART_RX_FIFO_FULL);
  serial_.setRxTimeout(1); // also flush the FIFO after one idle symbol
  comboStartMs_ = millis();
  validInCombo_ = 0;
}

void UartScanner::noteValid() {
  lastValidMs_ = millis();
  everValid_ = true;
  if (!locked_ && ++validInCombo_ >= kLockCount) locked_ = true;
}

uint32_t UartScanner::msSinceValid() const {
  return everValid_ ? millis() - lastValidMs_ : UINT32_MAX;
}

bool UartScanner::poll() {
  if (!running_) return false;
  const uint32_t now = millis();

  if (locked_) {
    if (now - lastValidMs_ < rescanAfterMs_) return false;
    // Lost the device (unplugged / reconfigured). Rescan, starting with the
    // combination that last worked.
    locked_ = false;
    apply();
    return true;
  }

  if (now - comboStartMs_ < dwellMs_) return false;
  combo_ = (combo_ + 1) % comboCount();
  apply();
  return true;
}
