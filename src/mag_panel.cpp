#include "mag_panel.h"

#include "ui.h"

static constexpr lv_coord_t kCompass = 116;
static constexpr lv_coord_t kCompassH = 150;
static constexpr lv_coord_t kLeftW = ui::SCREEN_W - kCompass - 4;
static constexpr lv_coord_t kFooterH = 18;

void MagPanel::build(lv_obj_t* body, const char* infoKey, const char* unit) {
  lv_obj_t* g = ui::grid(body, kLeftW, ui::BODY_H - kFooterH);
  lv_obj_align(g, LV_ALIGN_TOP_LEFT, 0, 0);

  char key[16];
  const lv_coord_t half = kLeftW / 2;
  info_ = ui::cell(g, infoKey, kLeftW);
  snprintf(key, sizeof(key), "X %s", unit);
  x_ = ui::cell(g, key, half);
  snprintf(key, sizeof(key), "Y %s", unit);
  y_ = ui::cell(g, key, half);
  snprintf(key, sizeof(key), "Z %s", unit);
  z_ = ui::cell(g, key, half);
  snprintf(key, sizeof(key), "|B| %s", unit);
  mag_ = ui::cell(g, key, half);
  heading_ = ui::cell(g, "Heading", half);
  rate_ = ui::cell(g, "Rate", half);

  compass_ = ui::fieldView(body, kCompass, kCompassH);
  lv_obj_align(compass_, LV_ALIGN_TOP_RIGHT, -2, 2);

  footer_ = ui::footer(body, ui::SCREEN_W);
  lv_obj_align(footer_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void MagPanel::setInfo(const char* text) { ui::setText(info_, text); }
void MagPanel::setFooter(const char* text) { ui::setText(footer_, text); }
void MagPanel::setRate(float rateHz) { ui::setTextf(rate_, "%.0f Hz", rateHz); }

void MagPanel::show(const float v[3], float rateHz) {
  setRate(rateHz);
  if (!v) {
    for (lv_obj_t* l : {x_, y_, z_, mag_, heading_}) ui::setText(l, "--");
    ui::fieldViewSet(compass_, NAN, 0);
    return;
  }
  ui::setTextf(x_, "%.1f", v[0]);
  ui::setTextf(y_, "%.1f", v[1]);
  ui::setTextf(z_, "%.1f", v[2]);
  ui::setTextf(mag_, "%.1f", magMagnitude(v));
  const float h = magHeading(v[0], v[1]);
  ui::setTextf(heading_, "%.0f deg", h);
  ui::fieldViewSet(compass_, h, magElevation(v));
}
