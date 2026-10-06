// Shared display + helpers for the two magnetometer tests.
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>

// Heading from the horizontal components, 0..360 (uncalibrated, sensor frame).
inline float magHeading(float x, float y) {
  float h = atan2f(y, x) * 180.0f / (float)M_PI;
  return h < 0 ? h + 360.0f : h;
}

// Field angle above the sensor's XY plane, -90..90 (about -66 in the UK with the module level).
inline float magElevation(const float v[3]) {
  return atan2f(v[2], hypotf(v[0], v[1])) * 180.0f / (float)M_PI;
}

inline float magMagnitude(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

// Samples-per-second over a rolling one-second window.
class RateMeter {
 public:
  void reset() { count_ = 0; windowStart_ = millis(); rate_ = 0; }
  void tick() { count_++; }
  float rate() {
    const uint32_t now = millis();
    if (now - windowStart_ >= 1000) {
      rate_ = count_ * 1000.0f / (now - windowStart_);
      count_ = 0;
      windowStart_ = now;
    }
    return rate_;
  }

 private:
  uint32_t count_ = 0, windowStart_ = 0;
  float rate_ = 0;
};

// Layout (320 x 176):  info cell + X/Y/Z/|B|/heading/rate grid on the left,
// 3D field view on the right, one footer line along the bottom.
class MagPanel {
 public:
  void build(lv_obj_t* body, const char* infoKey, const char* unit);
  void setInfo(const char* text);
  void setFooter(const char* text);
  void show(const float v[3], float rateHz); // pass nullptr to blank the values
  void setRate(float rateHz);

 private:
  lv_obj_t *info_ = nullptr, *x_ = nullptr, *y_ = nullptr, *z_ = nullptr;
  lv_obj_t *mag_ = nullptr, *heading_ = nullptr, *rate_ = nullptr;
  lv_obj_t *compass_ = nullptr, *footer_ = nullptr;
};
