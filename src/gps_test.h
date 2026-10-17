// HGLRC M100 GPS test on Port C (UART, receive only).
#pragma once

#include <TinyGPSPlus.h>

#include "tester.h"
#include "uart_scan.h"

class GpsTest : public Tester {
 public:
  GpsTest();

  const char* menuLabel() const override { return "Test M100 GPS"; }
  const char* menuIcon() const override { return LV_SYMBOL_GPS; }
  const char* portLabel() const override { return "Port C  UART"; }

  void start() override;
  void stop() override;
  void poll() override;
  void buildBody(lv_obj_t* body) override;
  void updateBody() override;
  bool ok() const override { return ok_; }
  const char* detail() const override { return detail_; }

 private:
  // Snapshot of the latest navigation solution, from NMEA or UBX.
  struct Nav {
    uint8_t fixType = 0; // 0 = none, 2 = 2D, 3 = 3D
    int satsUsed = -1;
    int satsView = -1;
    bool posValid = false;
    double lat = 0, lon = 0;
    float altM = NAN, speedKmh = NAN, courseDeg = NAN, dop = NAN;
    bool timeValid = false;
    uint16_t year = 0;
    uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
    uint32_t updatedMs = 0;
  };

  void resetParsers();
  void feedUbx(uint8_t b);
  void handleUbx(uint8_t cls, uint8_t id, const uint8_t* p, uint16_t len);
  void updateFromNmea();
  void evaluate();

  UartScanner scan_;
  TinyGPSPlus gps_;
  TinyGPSCustom gsaFixGP_, gsaFixGN_;
  TinyGPSCustom gsvGP_, gsvGL_, gsvGA_, gsvGB_;
  uint32_t lastNmeaOk_ = 0, nmeaOkBase_ = 0, nmeaErrBase_ = 0;

  // UBX frame parser
  enum class UbxState : uint8_t { Sync1, Sync2, Class, Id, Len1, Len2, Payload, CkA, CkB };
  UbxState ubxState_ = UbxState::Sync1;
  uint8_t ubxCls_ = 0, ubxId_ = 0, ubxCkA_ = 0, ubxCkB_ = 0;
  uint16_t ubxLen_ = 0, ubxPos_ = 0;
  uint8_t ubxBuf_[128];
  uint32_t ubxOk_ = 0, ubxErr_ = 0;
  uint32_t lastUbxMs_ = 0;

  Nav nav_;
  bool ok_ = false;
  char detail_[64] = "";

  // Widgets
  lv_obj_t *fix_ = nullptr, *sats_ = nullptr, *lat_ = nullptr, *lon_ = nullptr;
  lv_obj_t *alt_ = nullptr, *speed_ = nullptr, *utc_ = nullptr, *dop_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t *data_ = nullptr, *message_ = nullptr; // readings vs. no-data explanation
};
