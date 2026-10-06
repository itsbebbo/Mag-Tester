// Driver for the 5883-family compasses fitted to HGLRC M100 GPS modules.
// Handles the chip variants seen on these boards, identified by address + ID register:
//   HMC5883L  @ 0x1E  (Honeywell original; ID regs 0x0A-0x0C = "H43")
//   QMC5883L  @ 0x0D  (QST; reg 0x0D = 0xFF)
//   QMC5883P  @ 0x2C  (QST, newer; reg 0x00 = 0x80)
#pragma once

#include <M5Unified.h>

enum class MagChip : uint8_t { None, HMC5883L, QMC5883L, QMC5883P };

struct MagReading {
  int16_t raw[3];  // X, Y, Z counts
  float ut[3];     // X, Y, Z in microtesla
  bool overflow;
};

class Mag5883 {
 public:
  explicit Mag5883(m5::I2C_Class& bus) : bus_(bus) {}

  // Probe the known addresses; initialises the first chip found.
  bool detect(uint32_t freq);
  bool read(MagReading& out);
  void forget() { chip_ = MagChip::None; }

  MagChip chip() const { return chip_; }
  uint8_t address() const { return addr_; }
  bool idMatches() const { return idOk_; }
  uint8_t idByte() const { return idByte_; }
  const char* chipName() const;

 private:
  bool wr(uint8_t reg, uint8_t val) { return bus_.writeRegister8(addr_, reg, val, freq_); }
  bool rd(uint8_t reg, uint8_t* buf, size_t len) { return bus_.readRegister(addr_, reg, buf, len, freq_); }
  bool initChip();

  m5::I2C_Class& bus_;
  uint32_t freq_ = 100000;
  MagChip chip_ = MagChip::None;
  uint8_t addr_ = 0;
  bool idOk_ = false;
  uint8_t idByte_ = 0;
};
