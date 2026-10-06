#include "mag5883.h"

// Sensitivities for the ranges configured in initChip().
static constexpr float kHmcLsbPerGauss = 1090.0f;  // +/-1.3 Ga
static constexpr float kQmcLLsbPerGauss = 3000.0f; // +/-8 G
static constexpr float kQmcPLsbPerGauss = 3750.0f; // +/-8 G
static constexpr float kUtPerGauss = 100.0f;

static int16_t s16le(const uint8_t* p) { return (int16_t)(p[0] | (p[1] << 8)); }
static int16_t s16be(const uint8_t* p) { return (int16_t)((p[0] << 8) | p[1]); }

const char* Mag5883::chipName() const {
  switch (chip_) {
    case MagChip::HMC5883L: return "HMC5883L";
    case MagChip::QMC5883L: return "QMC5883L";
    case MagChip::QMC5883P: return "QMC5883P";
    default: return "None";
  }
}

bool Mag5883::detect(uint32_t freq) {
  freq_ = freq;
  chip_ = MagChip::None;

  struct Candidate { MagChip chip; uint8_t addr; };
  static constexpr Candidate kCandidates[] = {
      {MagChip::QMC5883P, 0x2C},
      {MagChip::QMC5883L, 0x0D},
      {MagChip::HMC5883L, 0x1E},
  };

  for (const auto& c : kCandidates) {
    if (!bus_.scanID(c.addr, freq_)) continue;
    addr_ = c.addr;
    chip_ = c.chip;
    uint8_t id[3] = {0, 0, 0};
    switch (c.chip) {
      case MagChip::QMC5883P:
        rd(0x00, id, 1);
        idOk_ = id[0] == 0x80;
        break;
      case MagChip::QMC5883L:
        rd(0x0D, id, 1);
        idOk_ = id[0] == 0xFF;
        break;
      case MagChip::HMC5883L:
        rd(0x0A, id, 3);
        idOk_ = id[0] == 'H' && id[1] == '4' && id[2] == '3';
        break;
      default: break;
    }
    idByte_ = id[0];
    if (initChip()) return true;
  }
  chip_ = MagChip::None;
  return false;
}

bool Mag5883::initChip() {
  bool ok = true;
  switch (chip_) {
    case MagChip::QMC5883P:
      ok &= wr(0x0B, 0x80); // soft reset
      delay(5);
      ok &= wr(0x29, 0x06); // axis sign definition (per QST application note)
      ok &= wr(0x0B, 0x08); // RNG = 8 G, set/reset on
      ok &= wr(0x0A, 0xCB); // continuous, ODR 100 Hz, OSR1 = 8, OSR2 = 8
      break;
    case MagChip::QMC5883L:
      ok &= wr(0x0A, 0x80); // soft reset
      delay(5);
      ok &= wr(0x0B, 0x01); // set/reset period
      ok &= wr(0x09, 0x1D); // continuous, ODR 200 Hz, RNG 8 G, OSR 512
      break;
    case MagChip::HMC5883L:
      ok &= wr(0x00, 0x70); // 8-sample average, 15 Hz, normal measurement
      ok &= wr(0x01, 0x20); // gain: +/-1.3 Ga
      ok &= wr(0x02, 0x00); // continuous measurement
      break;
    default: return false;
  }
  delay(10);
  return ok;
}

bool Mag5883::read(MagReading& out) {
  uint8_t b[7];
  float lsbPerGauss = 1.0f;
  out.overflow = false;

  switch (chip_) {
    case MagChip::QMC5883P:
      if (!rd(0x01, b, 6) || !rd(0x09, b + 6, 1)) return false;
      out.raw[0] = s16le(b);
      out.raw[1] = s16le(b + 2);
      out.raw[2] = s16le(b + 4);
      out.overflow = b[6] & 0x02;
      lsbPerGauss = kQmcPLsbPerGauss;
      break;
    case MagChip::QMC5883L:
      if (!rd(0x00, b, 7)) return false; // X, Y, Z (LE) + status
      out.raw[0] = s16le(b);
      out.raw[1] = s16le(b + 2);
      out.raw[2] = s16le(b + 4);
      out.overflow = b[6] & 0x02;
      lsbPerGauss = kQmcLLsbPerGauss;
      break;
    case MagChip::HMC5883L:
      if (!rd(0x03, b, 6)) return false; // X, Z, Y (big-endian, in that order)
      out.raw[0] = s16be(b);
      out.raw[2] = s16be(b + 2);
      out.raw[1] = s16be(b + 4);
      out.overflow = out.raw[0] == -4096 || out.raw[1] == -4096 || out.raw[2] == -4096;
      lsbPerGauss = kHmcLsbPerGauss;
      break;
    default: return false;
  }

  for (int i = 0; i < 3; i++) out.ut[i] = out.raw[i] / lsbPerGauss * kUtPerGauss;
  return true;
}
