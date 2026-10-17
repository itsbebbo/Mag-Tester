// Interface implemented by each of the three tests.
#pragma once

#include <lvgl.h>

class Tester {
 public:
  virtual ~Tester() = default;

  virtual const char* menuLabel() const = 0; // home screen button text
  virtual const char* menuIcon() const = 0;  // LV_SYMBOL_*
  virtual const char* portLabel() const = 0; // e.g. "Port C  UART"

  virtual void start() = 0; // configure the port, reset state
  virtual void stop() = 0;  // release the port
  virtual void poll() = 0;  // called every loop iteration

  // Build widgets into the area below the status banner (320 x 176).
  virtual void buildBody(lv_obj_t* body) = 0;
  // Refresh the widgets created by buildBody() (called every cfg::UI_REFRESH_MS).
  virtual void updateBody() = 0;

  enum class Status { Ok, Fail, Idle }; // banner colour: green / red / grey

  virtual bool ok() const = 0;
  virtual Status status() const { return ok() ? Status::Ok : Status::Fail; }
  virtual const char* statusText() const { return ok() ? "OK" : "NOT OKAY"; }
  virtual const char* detail() const = 0; // one-line reason shown under the status
};
