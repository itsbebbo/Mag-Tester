// Arduino magnetometer test on Port A (UART, receive only, RX on G2).
//
// The Arduino sends 8-byte binary frames (HMC5883L raw axis counts):
//   [0]    'M' (0x4D) sync byte
//   [1..2] X  int16 little-endian
//   [3..4] Y  int16 little-endian
//   [5..6] Z  int16 little-endian
//   [7]    XOR of bytes 0..6
//
// Flow: Prompt (connect cable, Start Test) -> Checking -> one of
//   NoConnection  no valid frames from the Arduino
//   MagFault      frames arrive but the values never change
//   Live          values vary: full readings screen, monitored continuously
#pragma once

#include "mag_panel.h"
#include "tester.h"
#include "uart_scan.h"

class ArduinoMagTest : public Tester {
 public:
  ArduinoMagTest();

  const char* menuLabel() const override { return "Test Arduino & Mag"; }
  const char* menuIcon() const override { return LV_SYMBOL_USB; }
  const char* portLabel() const override { return "Port A  UART"; }

  void start() override;
  void stop() override;
  void poll() override;
  void buildBody(lv_obj_t* body) override;
  void updateBody() override;
  bool ok() const override { return ok_; }
  Status status() const override;
  const char* statusText() const override;
  const char* detail() const override { return detail_; }

 private:
  enum class Phase : uint8_t { Prompt, Checking, NoConnection, MagFault, Live };

  static constexpr size_t kFrameLen = 8;
  static constexpr uint8_t kSync = 'M';

  static void onButton(lv_event_t* e);
  void beginCheck();
  void feed(uint8_t b);
  void acceptFrame();
  void evaluate();
  void buildPhase();
  void buildMessage(const char* text, const char* button);

  Phase phase_ = Phase::Prompt;
  // What the body shows. Same as phase_, except a Live fault shows the
  // NoConnection / MagFault message while monitoring continues underneath.
  Phase view_ = Phase::Prompt;
  Phase builtView_ = Phase::Prompt;
  bool buttonPressed_ = false;
  uint32_t checkStartMs_ = 0;
  uint32_t firstDataMs_ = 0;

  UartScanner scan_;
  uint8_t frame_[kFrameLen];
  size_t frameLen_ = 0;

  float xyz_[3] = {};
  bool haveReading_ = false;
  bool changed_ = false;      // values have varied since the check started
  uint32_t lastChangeMs_ = 0; // last time any axis changed
  RateMeter rate_;

  bool ok_ = false;
  char detail_[64] = "";

  lv_obj_t* body_ = nullptr;
  lv_obj_t* message_ = nullptr;
  MagPanel panel_;
};
