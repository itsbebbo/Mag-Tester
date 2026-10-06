// HGLRC 5883 magnetometer test on Port B (I2C).
#pragma once

#include "mag5883.h"
#include "mag_panel.h"
#include "tester.h"

class Mag5883Test : public Tester {
 public:
  const char* menuLabel() const override { return "Test 5883 Mag"; }
  const char* menuIcon() const override { return LV_SYMBOL_REFRESH; }
  const char* portLabel() const override { return "Port B  I2C"; }

  void start() override;
  void stop() override;
  void poll() override;
  void buildBody(lv_obj_t* body) override;
  void updateBody() override;
  bool ok() const override { return ok_; }
  const char* detail() const override { return detail_; }

 private:
  bool tryDetect();
  void beginBus(bool swapped);
  void evaluate();

  m5::I2C_Class bus_;
  Mag5883 mag_{bus_};
  bool busUp_ = false;
  bool swapped_ = false; // SDA/SCL found on the opposite pins
  int sda_ = -1, scl_ = -1;

  uint32_t lastPollMs_ = 0, lastDetectMs_ = 0;
  uint8_t failStreak_ = 0;
  uint32_t readsOk_ = 0, readsErr_ = 0;
  bool haveReading_ = false;
  MagReading last_{};
  uint32_t lastChangeMs_ = 0;
  RateMeter rate_;
  char otherDevices_[40] = "";

  bool ok_ = false;
  char detail_[64] = "";
  char message_[160] = ""; // shown instead of the readings when there is no data; "" = readings
  lv_obj_t *data_ = nullptr, *messageLbl_ = nullptr;
  MagPanel panel_;
};
