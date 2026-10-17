#include "mag5883_test.h"

#include "config.h"
#include "ports.h"
#include "ui.h"

// Port A is not used as I2C in this app (M5.Ex_I2C is released), so its
// controller is free for Port B. M5.In_I2C (internal bus) uses I2C_NUM_1.
static constexpr i2c_port_t kPort = I2C_NUM_0;

void Mag5883Test::start() {
  mag_.forget();
  busUp_ = false;
  swapped_ = false;
  failStreak_ = 0;
  readsOk_ = readsErr_ = 0;
  haveReading_ = false;
  otherDevices_[0] = '\0';
  rate_.reset();
  lastDetectMs_ = 0;
  tryDetect();
  evaluate();
}

void Mag5883Test::stop() {
  if (busUp_) bus_.release();
  busUp_ = false;
  const auto p = ports::portB();
  pinMode(p.pin1, INPUT);
  pinMode(p.pin2, INPUT);
}

void Mag5883Test::beginBus(bool swapped) {
  // Grove convention (matching Port A): pin 1 = SCL, pin 2 = SDA.
  const auto p = ports::portB();
  scl_ = swapped ? p.pin2 : p.pin1;
  sda_ = swapped ? p.pin1 : p.pin2;
  if (busUp_) bus_.release();
  busUp_ = bus_.begin(kPort, sda_, scl_);
  swapped_ = swapped;
}

bool Mag5883Test::tryDetect() {
  lastDetectMs_ = millis();
  char found[sizeof(otherDevices_)] = "";

  for (bool swapped : {false, true}) {
    beginBus(swapped);
    if (!busUp_) continue;
    if (mag_.detect(cfg::I2C_FREQ)) {
      failStreak_ = 0;
      haveReading_ = false;
      lastChangeMs_ = millis();
      otherDevices_[0] = '\0';
      return true;
    }
    // No 5883 - note anything else answering, to help diagnose wiring.
    bool present[128] = {};
    bus_.scanID(present, cfg::I2C_FREQ);
    for (int a = 0x08; a < 0x78; a++) {
      if (!present[a]) continue;
      const size_t len = strlen(found);
      if (len + 6 >= sizeof(found)) break;
      snprintf(found + len, sizeof(found) - len, "%s0x%02X", len ? " " : "", a);
    }
  }
  strlcpy(otherDevices_, found, sizeof(otherDevices_));
  beginBus(false);
  return false;
}

void Mag5883Test::poll() {
  const uint32_t now = millis();

  if (mag_.chip() == MagChip::None) {
    if (now - lastDetectMs_ >= cfg::MAG_REDETECT_MS) tryDetect();
    evaluate();
    return;
  }

  if (now - lastPollMs_ < cfg::MAG_POLL_MS) return;
  lastPollMs_ = now;

  MagReading r;
  if (!mag_.read(r)) {
    readsErr_++;
    if (++failStreak_ >= cfg::MAG_MAX_READ_FAILS) {
      mag_.forget(); // lost contact - fall back to detection
      haveReading_ = false;
    }
    evaluate();
    return;
  }

  failStreak_ = 0;
  readsOk_++;
  rate_.tick();
  if (!haveReading_ || memcmp(r.raw, last_.raw, sizeof(r.raw)) != 0) lastChangeMs_ = now;
  last_ = r;
  haveReading_ = true;
  evaluate();
}

void Mag5883Test::evaluate() {
  ok_ = false;
  message_[0] = '\0';
  if (mag_.chip() == MagChip::None) {
    if (otherDevices_[0]) {
      snprintf(detail_, sizeof(detail_), "No 5883. Other device(s): %s", otherDevices_);
      snprintf(message_, sizeof(message_),
               "No 5883 Magnetometer found, but another I2C device (%s) is answering. "
               "Check the Magnetometer is the correct type.", otherDevices_);
    } else {
      snprintf(detail_, sizeof(detail_), "No I2C device on Port B - check lead");
      snprintf(message_, sizeof(message_),
               "No Magnetometer detected. Check the cable to the Magnetometer "
               "or try replacing the Magnetometer");
    }
    return;
  }
  if (!haveReading_) {
    snprintf(detail_, sizeof(detail_), failStreak_ ? "I2C read failed" : "Waiting for data...");
    snprintf(message_, sizeof(message_), failStreak_
             ? "The Magnetometer was found but is not returning data. Check the cable to "
               "the Magnetometer or try replacing the Magnetometer"
             : "Waiting for data from the Magnetometer...");
    return;
  }
  if (last_.overflow) {
    snprintf(detail_, sizeof(detail_), "Sensor overflow (magnet nearby?)");
    return;
  }
  if (millis() - lastChangeMs_ > cfg::MAG_STUCK_MS) {
    snprintf(detail_, sizeof(detail_), "Data stuck - readings not changing");
    snprintf(message_, sizeof(message_),
             "The Magnetometer is connected but its readings are not changing. "
             "Try replacing the Magnetometer");
    return;
  }
  ok_ = true;
  if (!mag_.idMatches()) snprintf(detail_, sizeof(detail_), "Data OK, unexpected ID 0x%02X", mag_.idByte());
  else snprintf(detail_, sizeof(detail_), "Rotate module - heading should follow");
}

void Mag5883Test::buildBody(lv_obj_t* body) {
  data_ = ui::box(body);
  panel_.build(data_, "Chip", "(uT)");
  messageLbl_ = ui::message(body);
  lv_obj_align(messageLbl_, LV_ALIGN_TOP_MID, 0, 40);
}

void Mag5883Test::updateBody() {
  // No data: show only the explanation, not the readings and compass.
  ui::setVisible(data_, !message_[0]);
  ui::setVisible(messageLbl_, message_[0]);
  if (message_[0]) {
    ui::setText(messageLbl_, message_);
    return;
  }

  if (mag_.chip() == MagChip::None) {
    panel_.setInfo("Searching...");
  } else {
    char info[32];
    snprintf(info, sizeof(info), "%s @ 0x%02X", mag_.chipName(), mag_.address());
    panel_.setInfo(info);
  }
  panel_.show(haveReading_ ? last_.ut : nullptr, rate_.rate());

  char f[80];
  snprintf(f, sizeof(f), "SDA G%d SCL G%d%s   reads %lu  err %lu", sda_, scl_,
           swapped_ ? " (swapped)" : "", (unsigned long)readsOk_, (unsigned long)readsErr_);
  panel_.setFooter(f);
}
