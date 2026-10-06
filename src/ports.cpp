#include "ports.h"

#include <M5Unified.h>

namespace ports {

static Pins s_a{1, 2};
static Pins s_b{8, 9};
static Pins s_c{18, 17};

void init() {
  using m5::pin_name_t;
  s_a = {M5.getPin(pin_name_t::port_a_pin1), M5.getPin(pin_name_t::port_a_pin2)};
  s_b = {M5.getPin(pin_name_t::port_b_pin1), M5.getPin(pin_name_t::port_b_pin2)};
  s_c = {M5.getPin(pin_name_t::port_c_rxd), M5.getPin(pin_name_t::port_c_txd)};

  // Supply 5V to the Grove ports (required when running from battery).
  M5.Power.setExtOutput(true);

  // Port A is used as a UART here, so make sure M5Unified's external I2C bus
  // isn't holding the pins.
  M5.Ex_I2C.release();
  for (int pin : {s_a.pin1, s_a.pin2, s_b.pin1, s_b.pin2, s_c.pin1, s_c.pin2}) {
    pinMode(pin, INPUT);
  }
}

Pins portA() { return s_a; }
Pins portB() { return s_b; }
Pins portC() { return s_c; }

} // namespace ports
