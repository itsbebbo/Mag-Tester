#include "gps_test.h"

#include "config.h"
#include "ports.h"
#include "ui.h"

static constexpr uint32_t kStaleMs = 3000; // nav data older than this is ignored

static uint16_t u16le(const uint8_t* p) { return p[0] | (p[1] << 8); }
static int32_t i32le(const uint8_t* p) {
  return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

GpsTest::GpsTest()
    : scan_(Serial1, cfg::GPS_BAUDS, sizeof(cfg::GPS_BAUDS) / sizeof(cfg::GPS_BAUDS[0]),
            cfg::GPS_SCAN_DWELL_MS, cfg::GPS_RESCAN_AFTER_MS),
      gsaFixGP_(gps_, "GPGSA", 2), gsaFixGN_(gps_, "GNGSA", 2),
      gsvGP_(gps_, "GPGSV", 3), gsvGL_(gps_, "GLGSV", 3),
      gsvGA_(gps_, "GAGSV", 3), gsvGB_(gps_, "GBGSV", 3) {}

void GpsTest::start() {
  // TinyGPSPlus can't be reset without unregistering the custom fields, so
  // count from a baseline instead. Stale fix data is rejected by age checks.
  nmeaOkBase_ = lastNmeaOk_ = gps_.passedChecksum();
  nmeaErrBase_ = gps_.failedChecksum();
  nav_ = Nav();
  ubxOk_ = ubxErr_ = 0;
  lastUbxMs_ = 0;
  resetParsers();
  const auto p = ports::portC();
  scan_.start(p.pin1, p.pin2); // RX on G18 expected; G17 tried in case the lead is crossed
  evaluate();
}

void GpsTest::stop() { scan_.stop(); }

void GpsTest::resetParsers() { ubxState_ = UbxState::Sync1; }

void GpsTest::poll() {
  if (scan_.poll()) resetParsers();

  HardwareSerial& s = scan_.serial();
  int n = s.available();
  while (n-- > 0) {
    const uint8_t b = s.read();
    feedUbx(b);
    gps_.encode(b);
    if (gps_.passedChecksum() != lastNmeaOk_) {
      lastNmeaOk_ = gps_.passedChecksum();
      scan_.noteValid();
    }
  }

  updateFromNmea();
  evaluate();
}

// ---- NMEA --------------------------------------------------------------------

void GpsTest::updateFromNmea() {
  // Prefer UBX NAV-PVT when it is flowing: it is a single consistent solution.
  if (lastUbxMs_ && millis() - lastUbxMs_ < kStaleMs && nav_.updatedMs == lastUbxMs_) return;
  if (!gps_.location.isUpdated() && !gps_.satellites.isUpdated() && !gps_.time.isUpdated()) return;

  Nav n;
  n.posValid = gps_.location.isValid() && gps_.location.age() < kStaleMs;
  if (n.posValid) {
    n.lat = gps_.location.lat();
    n.lon = gps_.location.lng();
  }

  TinyGPSCustom* gsa = gsaFixGN_.age() < gsaFixGP_.age() ? &gsaFixGN_ : &gsaFixGP_;
  if (gsa->isValid() && gsa->age() < kStaleMs) {
    const int f = atoi(gsa->value());
    n.fixType = (f >= 2 && n.posValid) ? f : 0;
  } else {
    n.fixType = n.posValid ? 1 : 0; // fix, dimension unknown
  }

  if (gps_.satellites.isValid() && gps_.satellites.age() < kStaleMs) n.satsUsed = gps_.satellites.value();
  int view = 0;
  bool anyView = false;
  for (TinyGPSCustom* g : {&gsvGP_, &gsvGL_, &gsvGA_, &gsvGB_}) {
    if (g->isValid() && g->age() < 5000) {
      view += atoi(g->value());
      anyView = true;
    }
  }
  if (anyView) n.satsView = view;

  if (gps_.altitude.isValid() && gps_.altitude.age() < kStaleMs) n.altM = gps_.altitude.meters();
  if (gps_.speed.isValid() && gps_.speed.age() < kStaleMs) n.speedKmh = gps_.speed.kmph();
  if (gps_.course.isValid() && gps_.course.age() < kStaleMs) n.courseDeg = gps_.course.deg();
  if (gps_.hdop.isValid() && gps_.hdop.age() < kStaleMs) n.dop = gps_.hdop.hdop();

  if (gps_.time.isValid() && gps_.time.age() < kStaleMs) {
    n.timeValid = true;
    n.hour = gps_.time.hour();
    n.minute = gps_.time.minute();
    n.second = gps_.time.second();
    if (gps_.date.isValid()) {
      n.year = gps_.date.year();
      n.month = gps_.date.month();
      n.day = gps_.date.day();
    }
  }
  n.updatedMs = millis();
  nav_ = n;
}

// ---- UBX ---------------------------------------------------------------------

void GpsTest::feedUbx(uint8_t b) {
  auto ck = [this](uint8_t v) {
    ubxCkA_ += v;
    ubxCkB_ += ubxCkA_;
  };
  switch (ubxState_) {
    case UbxState::Sync1:
      if (b == 0xB5) ubxState_ = UbxState::Sync2;
      break;
    case UbxState::Sync2:
      ubxState_ = (b == 0x62) ? UbxState::Class : (b == 0xB5 ? UbxState::Sync2 : UbxState::Sync1);
      break;
    case UbxState::Class:
      ubxCkA_ = ubxCkB_ = 0;
      ck(b);
      ubxCls_ = b;
      ubxState_ = UbxState::Id;
      break;
    case UbxState::Id:
      ck(b);
      ubxId_ = b;
      ubxState_ = UbxState::Len1;
      break;
    case UbxState::Len1:
      ck(b);
      ubxLen_ = b;
      ubxState_ = UbxState::Len2;
      break;
    case UbxState::Len2:
      ck(b);
      ubxLen_ |= (uint16_t)b << 8;
      ubxPos_ = 0;
      if (ubxLen_ > 1024) ubxState_ = UbxState::Sync1; // not a real frame
      else ubxState_ = ubxLen_ ? UbxState::Payload : UbxState::CkA;
      break;
    case UbxState::Payload:
      ck(b);
      if (ubxPos_ < sizeof(ubxBuf_)) ubxBuf_[ubxPos_] = b;
      if (++ubxPos_ == ubxLen_) ubxState_ = UbxState::CkA;
      break;
    case UbxState::CkA:
      if (b == ubxCkA_) {
        ubxState_ = UbxState::CkB;
      } else {
        ubxErr_++;
        ubxState_ = UbxState::Sync1;
      }
      break;
    case UbxState::CkB:
      ubxState_ = UbxState::Sync1;
      if (b != ubxCkB_) {
        ubxErr_++;
        break;
      }
      ubxOk_++;
      scan_.noteValid();
      if (ubxLen_ <= sizeof(ubxBuf_)) handleUbx(ubxCls_, ubxId_, ubxBuf_, ubxLen_);
      break;
  }
}

void GpsTest::handleUbx(uint8_t cls, uint8_t id, const uint8_t* p, uint16_t len) {
  if (cls != 0x01 || id != 0x07 || len < 92) return; // NAV-PVT only

  Nav n;
  const uint8_t valid = p[11];
  const uint8_t fixType = p[20];
  const bool fixOk = p[21] & 0x01;
  n.fixType = !fixOk ? 0 : (fixType == 2 ? 2 : (fixType == 3 || fixType == 4) ? 3 : 0);
  n.satsUsed = p[23];
  n.posValid = n.fixType >= 2;
  n.lon = i32le(p + 24) * 1e-7;
  n.lat = i32le(p + 28) * 1e-7;
  n.altM = i32le(p + 36) / 1000.0f;
  n.speedKmh = i32le(p + 60) * 0.0036f; // mm/s -> km/h
  n.courseDeg = i32le(p + 64) * 1e-5f;
  n.dop = u16le(p + 76) * 0.01f;      // PDOP
  n.satsView = nav_.satsView;          // not in PVT; keep NMEA value if any
  n.timeValid = valid & 0x02;
  n.year = u16le(p + 4);
  n.month = p[6];
  n.day = p[7];
  n.hour = p[8];
  n.minute = p[9];
  n.second = p[10];
  lastUbxMs_ = n.updatedMs = millis();
  nav_ = n;
}

// ---- Status ------------------------------------------------------------------

void GpsTest::evaluate() {
  const uint32_t since = scan_.msSinceValid();
  if (!scan_.locked()) {
    ok_ = false;
    snprintf(detail_, sizeof(detail_), "Searching: %lu baud, RX G%d (%u/%u)",
             (unsigned long)scan_.baud(), scan_.rxPin(),
             (unsigned)scan_.comboIndex() + 1, (unsigned)scan_.comboCount());
    return;
  }
  if (since > cfg::GPS_DATA_TIMEOUT_MS) {
    ok_ = false;
    snprintf(detail_, sizeof(detail_), "No valid data for %lus", (unsigned long)(since / 1000));
    return;
  }
  const bool fix = nav_.fixType >= 1 && nav_.posValid && millis() - nav_.updatedMs < kStaleMs;
  ok_ = fix || !cfg::GPS_OK_REQUIRES_FIX;
  const int sats = nav_.satsUsed < 0 ? 0 : nav_.satsUsed;
  if (fix) {
    const char* dim = nav_.fixType == 3 ? "3D fix" : nav_.fixType == 2 ? "2D fix" : "Fix";
    snprintf(detail_, sizeof(detail_), "%s, %d sats", dim, sats);
  } else {
    snprintf(detail_, sizeof(detail_), cfg::GPS_OK_REQUIRES_FIX ? "Data OK, waiting for fix (%d sats)"
                                                                : "Data OK, no fix yet (%d sats)", sats);
  }
}

// ---- UI ------------------------------------------------------------------------

void GpsTest::buildBody(lv_obj_t* body) {
  constexpr lv_coord_t w = ui::SCREEN_W / 2;
  data_ = ui::box(body);
  message_ = ui::message(body);
  lv_obj_align(message_, LV_ALIGN_TOP_MID, 0, 40);

  lv_obj_t* g = ui::grid(data_, ui::SCREEN_W, ui::BODY_H - 18);
  fix_ = ui::cell(g, "Fix", w);
  sats_ = ui::cell(g, "Sats used / view", w);
  lat_ = ui::cell(g, "Latitude", w);
  lon_ = ui::cell(g, "Longitude", w);
  alt_ = ui::cell(g, "Altitude", w);
  speed_ = ui::cell(g, "Speed / course", w);
  utc_ = ui::cell(g, "UTC time", w);
  dop_ = ui::cell(g, "HDOP", w);
  footer_ = ui::footer(data_, ui::SCREEN_W);
  lv_obj_align(footer_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void GpsTest::updateBody() {
  // No data: show only the explanation, not the readings.
  const char* msg = nullptr;
  if (!scan_.locked()) {
    msg = "Searching for data from the GPS...\n\nIf this does not change, check the cable to "
          "the GPS or try replacing the GPS";
  } else if (scan_.msSinceValid() > cfg::GPS_DATA_TIMEOUT_MS) {
    msg = "Data from the GPS has stopped. Check the cable to the GPS or try replacing the GPS";
  }
  ui::setVisible(data_, !msg);
  ui::setVisible(message_, msg);
  if (msg) {
    ui::setText(message_, msg);
    return;
  }

  const bool fresh = scan_.locked() && millis() - nav_.updatedMs < kStaleMs;
  const Nav& n = nav_;

  static const char* kFix[] = {"None", "Fix", "2D", "3D"};
  ui::setText(fix_, fresh ? kFix[n.fixType > 3 ? 0 : n.fixType] : "--");

  char used[8] = "--", view[8] = "--";
  if (fresh && n.satsUsed >= 0) snprintf(used, sizeof(used), "%d", n.satsUsed);
  if (fresh && n.satsView >= 0) snprintf(view, sizeof(view), "%d", n.satsView);
  ui::setTextf(sats_, "%s / %s", used, view);

  if (fresh && n.posValid) {
    ui::setTextf(lat_, "%.6f", n.lat);
    ui::setTextf(lon_, "%.6f", n.lon);
  } else {
    ui::setText(lat_, "--");
    ui::setText(lon_, "--");
  }

  if (fresh && n.posValid && !isnan(n.altM)) ui::setTextf(alt_, "%.1f m", n.altM);
  else ui::setText(alt_, "--");

  if (fresh && n.posValid && !isnan(n.speedKmh)) {
    if (!isnan(n.courseDeg)) ui::setTextf(speed_, "%.1f km/h %03d", n.speedKmh, (int)lroundf(n.courseDeg) % 360);
    else ui::setTextf(speed_, "%.1f km/h", n.speedKmh);
  } else {
    ui::setText(speed_, "--");
  }

  if (fresh && n.timeValid) ui::setTextf(utc_, "%02u:%02u:%02u", n.hour, n.minute, n.second);
  else ui::setText(utc_, "--");

  if (fresh && !isnan(n.dop)) ui::setTextf(dop_, "%.2f", n.dop);
  else ui::setText(dop_, "--");

  const unsigned long nmeaOk = gps_.passedChecksum() - nmeaOkBase_;
  const unsigned long nmeaErr = gps_.failedChecksum() - nmeaErrBase_;
  if (ubxOk_ || ubxErr_) {
    ui::setTextf(footer_, "%lu bd G%d  NMEA %lu/%lu err  UBX %lu/%lu err", (unsigned long)scan_.baud(),
                 scan_.rxPin(), nmeaOk, nmeaErr, (unsigned long)ubxOk_, (unsigned long)ubxErr_);
  } else {
    ui::setTextf(footer_, "NMEA  %lu baud  RX G%d   ok %lu  err %lu", (unsigned long)scan_.baud(),
                 scan_.rxPin(), nmeaOk, nmeaErr);
  }
}
