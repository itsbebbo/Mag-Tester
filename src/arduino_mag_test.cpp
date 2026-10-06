#include "arduino_mag_test.h"

#include "config.h"
#include "ports.h"
#include "ui.h"

ArduinoMagTest::ArduinoMagTest()
    : scan_(Serial2, cfg::ARD_BAUDS, sizeof(cfg::ARD_BAUDS) / sizeof(cfg::ARD_BAUDS[0]),
            cfg::ARD_SCAN_DWELL_MS, cfg::ARD_RESCAN_AFTER_MS) {}

// ---- Frame parsing -------------------------------------------------------------

// Byte-at-a-time frame assembler. Waits for the 'M' sync byte, collects 8
// bytes, then checks the XOR. On a bad checksum the 'M' was probably a data
// byte, so resync on the next 'M' already in the buffer rather than dropping
// the whole frame.
void ArduinoMagTest::feed(uint8_t b) {
  if (frameLen_ == 0 && b != kSync) return;
  frame_[frameLen_++] = b;
  if (frameLen_ < kFrameLen) return;

  uint8_t x = 0;
  for (size_t i = 0; i < kFrameLen - 1; i++) x ^= frame_[i];
  if (x == frame_[kFrameLen - 1]) {
    acceptFrame();
    frameLen_ = 0;
    return;
  }

  size_t next = 1;
  while (next < kFrameLen && frame_[next] != kSync) next++;
  frameLen_ = kFrameLen - next;
  memmove(frame_, frame_ + next, frameLen_);
}

void ArduinoMagTest::acceptFrame() {
  const uint32_t now = millis();
  float v[3];
  for (int i = 0; i < 3; i++) v[i] = (int16_t)(frame_[1 + 2 * i] | (frame_[2 + 2 * i] << 8));

  if (!haveReading_) {
    firstDataMs_ = lastChangeMs_ = now;
  } else if (memcmp(v, xyz_, sizeof(v)) != 0) {
    changed_ = true;
    lastChangeMs_ = now;
  }
  memcpy(xyz_, v, sizeof(xyz_));
  haveReading_ = true;
  rate_.tick();
  scan_.noteValid();
}

// ---- Test flow -----------------------------------------------------------------

void ArduinoMagTest::start() {
  phase_ = view_ = Phase::Prompt;
  buttonPressed_ = false;
  evaluate();
}

void ArduinoMagTest::stop() { scan_.stop(); }

void ArduinoMagTest::beginCheck() {
  frameLen_ = 0;
  haveReading_ = false;
  changed_ = false;
  rate_.reset();
  checkStartMs_ = millis();
  scan_.start(ports::portA().pin2); // RX-only on G2; only the baud rate is scanned
  phase_ = Phase::Checking;
}

// Start Test / Retry Test. Only sets a flag: the body (and this button) is
// rebuilt later from updateBody(), never from inside the event handler.
void ArduinoMagTest::onButton(lv_event_t* e) {
  static_cast<ArduinoMagTest*>(lv_event_get_user_data(e))->buttonPressed_ = true;
}

void ArduinoMagTest::poll() {
  if (buttonPressed_) {
    buttonPressed_ = false;
    beginCheck();
  }
  if (phase_ == Phase::Prompt || phase_ == Phase::NoConnection || phase_ == Phase::MagFault) {
    evaluate();
    return;
  }

  if (scan_.poll()) frameLen_ = 0;

  // Drain everything in bulk so a backlog (e.g. after a slow screen redraw)
  // clears in one pass and the display always shows the newest frame.
  HardwareSerial& s = scan_.serial();
  uint8_t buf[256];
  size_t n;
  while ((n = s.read(buf, sizeof(buf))) > 0) {
    for (size_t i = 0; i < n; i++) feed(buf[i]);
  }

  if (phase_ == Phase::Checking) {
    const uint32_t now = millis();
    const bool fresh = scan_.locked() && scan_.msSinceValid() <= cfg::ARD_DATA_TIMEOUT_MS;
    if (fresh && changed_) {
      phase_ = Phase::Live;
    } else if (fresh && now - firstDataMs_ >= cfg::ARD_STATIC_CHECK_MS) {
      phase_ = Phase::MagFault;
      scan_.stop();
    } else if (!fresh && now - checkStartMs_ >= cfg::ARD_CONNECT_TIMEOUT_MS) {
      phase_ = Phase::NoConnection;
      scan_.stop();
    }
  }
  evaluate();
}

void ArduinoMagTest::evaluate() {
  ok_ = false;
  view_ = phase_;
  switch (phase_) {
    case Phase::Prompt:
      snprintf(detail_, sizeof(detail_), "Connect cable, then Start Test");
      return;
    case Phase::Checking:
      snprintf(detail_, sizeof(detail_), haveReading_ ? "Checking Magnetometer data..."
                                                      : "Looking for Arduino data...");
      return;
    case Phase::NoConnection:
      snprintf(detail_, sizeof(detail_), "No connection detected");
      return;
    case Phase::MagFault:
      snprintf(detail_, sizeof(detail_), "No data from Magnetometer");
      return;
    case Phase::Live:
      break;
  }
  if (!scan_.locked() || scan_.msSinceValid() > cfg::ARD_DATA_TIMEOUT_MS) {
    view_ = Phase::NoConnection;
    snprintf(detail_, sizeof(detail_), "No data from Arduino");
    return;
  }
  if (millis() - lastChangeMs_ > cfg::MAG_STUCK_MS) {
    view_ = Phase::MagFault;
    snprintf(detail_, sizeof(detail_), "Magnetometer data static");
    return;
  }
  ok_ = true;
  snprintf(detail_, sizeof(detail_), "Arduino OK, Mag OK");
}

Tester::Status ArduinoMagTest::status() const {
  if (phase_ == Phase::Prompt || phase_ == Phase::Checking) return Status::Idle;
  return ok_ ? Status::Ok : Status::Fail;
}

const char* ArduinoMagTest::statusText() const {
  switch (phase_) {
    case Phase::Prompt: return "READY";
    case Phase::Checking: return "TESTING";
    default: return ok_ ? "ALL OKAY" : "NOT OKAY";
  }
}

// ---- Display -------------------------------------------------------------------

void ArduinoMagTest::buildBody(lv_obj_t* body) {
  body_ = body;
  buildPhase();
}

// Centred wrapping message, with an optional button underneath.
void ArduinoMagTest::buildMessage(const char* text, const char* button) {
  message_ = ui::message(body_);
  lv_label_set_text(message_, text);
  lv_obj_align(message_, LV_ALIGN_TOP_MID, 0, button ? 12 : 50);

  if (!button) return;
  lv_obj_t* btn = lv_btn_create(body_);
  lv_obj_set_size(btn, 200, 50);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_BLUE, 1), 0);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_add_event_cb(btn, onButton, LV_EVENT_CLICKED, this);
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, button);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
  lv_obj_center(lbl);
}

void ArduinoMagTest::buildPhase() {
  lv_obj_clean(body_);
  message_ = nullptr;
  builtView_ = view_;
  switch (view_) {
    case Phase::Prompt:
      buildMessage("Connect the cable to the\nArduino / Magnetometer", LV_SYMBOL_PLAY "  Start Test");
      break;
    case Phase::Checking:
      buildMessage("Testing, please wait...", nullptr);
      break;
    case Phase::NoConnection:
      buildMessage("No connection detected, this means the cable to the Arduino is broken "
                   "or the Arduino is not correctly programmed",
                   LV_SYMBOL_REFRESH "  Retry Test");
      break;
    case Phase::MagFault:
      buildMessage("The Arduino is connected and producing data but no data is coming from "
                   "the Magnetometer. Check the cable to the Magnetometer or try replacing "
                   "the Magnetometer",
                   LV_SYMBOL_REFRESH "  Retry Test");
      break;
    case Phase::Live:
      panel_.build(body_, "Link", "(raw)");
      break;
  }
}

void ArduinoMagTest::updateBody() {
  if (builtView_ != view_) buildPhase();
  if (view_ != Phase::Live) return;

  char info[32];
  if (scan_.locked()) snprintf(info, sizeof(info), "%lu baud  RX G%d", (unsigned long)scan_.baud(), scan_.rxPin());
  else snprintf(info, sizeof(info), "Searching...");
  panel_.setInfo(info);

  const bool fresh = scan_.locked() && scan_.msSinceValid() <= cfg::ARD_DATA_TIMEOUT_MS;
  panel_.show(fresh && haveReading_ ? xyz_ : nullptr, rate_.rate());
}
