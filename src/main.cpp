// Mag / GPS tester for M5Stack CoreS3.
//
//   Port A (G1/G2)   - Arduino magnetometer (UART)
//   Port B (G8/G9)   - HGLRC M100 5883 magnetometer (I2C)
//   Port C (G18/G17) - HGLRC M100 GPS (UART)
#include <M5Unified.h>
#include <lvgl.h>

#include "arduino_mag_test.h"
#include "config.h"
#include "gps_test.h"
#include "mag5883_test.h"
#include "ports.h"
#include "ui.h"

static GpsTest s_gps;
static Mag5883Test s_mag5883;
static ArduinoMagTest s_ardMag;
static Tester* const kTests[] = {&s_ardMag, &s_gps, &s_mag5883};
static constexpr int kTestCount = sizeof(kTests) / sizeof(kTests[0]);

static Tester* s_active = nullptr;

// Screen changes are deferred to loop() so a button's own screen is never
// deleted from inside its event handler.
static constexpr int kNavNone = -2;
static constexpr int kNavHome = -1;
static int s_pendingNav = kNavNone;

static lv_obj_t* s_banner = nullptr;
static lv_obj_t* s_status = nullptr;
static lv_obj_t* s_detail = nullptr;
static lv_obj_t* s_battery = nullptr;
static uint32_t s_lastUiMs = 0;
static uint32_t s_lastBatteryMs = 0;

static void onNav(lv_event_t* e) { s_pendingNav = (int)(intptr_t)lv_event_get_user_data(e); }

static lv_obj_t* newScreen() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

static void showScreen(lv_obj_t* scr) {
  lv_obj_t* old = lv_scr_act();
  lv_scr_load(scr);
  if (old && old != scr) lv_obj_del(old);
  s_banner = s_status = s_detail = s_battery = nullptr;
}

// ---- Home ----------------------------------------------------------------------

static void updateBattery() {
  if (!s_battery) return;
  const int level = M5.Power.getBatteryLevel();
  const char* sym = level > 80 ? LV_SYMBOL_BATTERY_FULL : level > 55 ? LV_SYMBOL_BATTERY_3
                  : level > 30 ? LV_SYMBOL_BATTERY_2 : level > 10 ? LV_SYMBOL_BATTERY_1
                  : LV_SYMBOL_BATTERY_EMPTY;
  const char* charge = M5.Power.isCharging() ? LV_SYMBOL_CHARGE " " : "";
  ui::setTextf(s_battery, "%s%s %d%%", charge, sym, level);
}

static void buildHome() {
  lv_obj_t* scr = newScreen();

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Mag / GPS Tester");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 8);

  lv_obj_t* battery = lv_label_create(scr);
  lv_obj_set_style_text_font(battery, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(battery, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
  lv_label_set_text(battery, "");
  lv_obj_align(battery, LV_ALIGN_TOP_RIGHT, -10, 12);

  static const lv_palette_t kColors[] = {LV_PALETTE_BLUE, LV_PALETTE_TEAL, LV_PALETTE_DEEP_PURPLE};
  for (int i = 0; i < kTestCount; i++) {
    lv_obj_t* btn = lv_btn_create(scr);
    lv_obj_set_size(btn, ui::SCREEN_W - 16, 58);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 44 + i * 64);
    lv_obj_set_style_bg_color(btn, lv_palette_darken(kColors[i], 1), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, onNav, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text_fmt(lbl, "%s  %s", kTests[i]->menuIcon(), kTests[i]->menuLabel());
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, -8);

    lv_obj_t* port = lv_label_create(btn);
    lv_label_set_text(port, kTests[i]->portLabel());
    lv_obj_set_style_text_font(port, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(port, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(port, LV_ALIGN_LEFT_MID, 30, 14);
  }

  showScreen(scr);
  s_battery = battery;
  updateBattery();
}

// ---- Test screen -------------------------------------------------------------

static void updateBanner() {
  const Tester::Status st = s_active->status();
  lv_obj_set_style_bg_color(s_banner, st == Tester::Status::Ok     ? lv_palette_darken(LV_PALETTE_GREEN, 2)
                                      : st == Tester::Status::Fail ? lv_palette_darken(LV_PALETTE_RED, 2)
                                                                   : lv_palette_darken(LV_PALETTE_BLUE_GREY, 2), 0);
  ui::setText(s_status, s_active->statusText());
  ui::setText(s_detail, s_active->detail());
}

static void buildTest(Tester* t) {
  lv_obj_t* scr = newScreen();

  lv_obj_t* banner = lv_obj_create(scr);
  lv_obj_remove_style_all(banner);
  lv_obj_set_size(banner, ui::SCREEN_W, ui::BANNER_H);
  lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
  lv_obj_clear_flag(banner, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* status = lv_label_create(banner);
  lv_obj_set_style_text_font(status, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(status, lv_color_white(), 0);
  lv_label_set_text(status, "");
  lv_obj_align(status, LV_ALIGN_TOP_LEFT, 10, 4);

  lv_obj_t* detail = lv_label_create(banner);
  lv_obj_set_width(detail, 222);
  lv_label_set_long_mode(detail, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(detail, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(detail, lv_color_white(), 0);
  lv_label_set_text(detail, "");
  lv_obj_align(detail, LV_ALIGN_TOP_LEFT, 10, 42);

  lv_obj_t* home = lv_btn_create(banner);
  lv_obj_set_size(home, 84, 54);
  lv_obj_align(home, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_obj_set_style_bg_color(home, lv_color_hex(0x2A323C), 0);
  lv_obj_set_style_radius(home, 10, 0);
  lv_obj_add_event_cb(home, onNav, LV_EVENT_CLICKED, (void*)(intptr_t)kNavHome);
  lv_obj_t* homeLbl = lv_label_create(home);
  lv_label_set_text(homeLbl, LV_SYMBOL_HOME "\nHome");
  lv_obj_set_style_text_align(homeLbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(homeLbl, &lv_font_montserrat_16, 0);
  lv_obj_center(homeLbl);

  lv_obj_t* body = lv_obj_create(scr);
  lv_obj_remove_style_all(body);
  lv_obj_set_size(body, ui::SCREEN_W, ui::BODY_H);
  lv_obj_set_pos(body, 0, ui::BANNER_H);
  lv_obj_set_style_pad_top(body, 2, 0);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  t->buildBody(body);

  showScreen(scr);
  s_banner = banner;
  s_status = status;
  s_detail = detail;
}

// ---- Navigation ----------------------------------------------------------------

static void navigate(int target) {
  if (s_active) {
    s_active->stop();
    s_active = nullptr;
  }
  if (target == kNavHome) {
    buildHome();
    return;
  }
  Tester* t = kTests[target];
  t->start();
  buildTest(t);
  s_active = t;
  updateBanner();
  t->updateBody();
}

// ---- Arduino entry points ----------------------------------------------------------

void setup() {
  auto m5cfg = M5.config();
  m5cfg.serial_baudrate = 115200;
  M5.begin(m5cfg);
  M5.Display.setBrightness(160);

  ports::init();
  ui::init();
  buildHome();
}

void loop() {
  M5.update();
  if (s_active) s_active->poll();
  lv_timer_handler();

  if (s_pendingNav != kNavNone) {
    const int target = s_pendingNav;
    s_pendingNav = kNavNone;
    navigate(target);
  }

  const uint32_t now = millis();
  if (s_active && now - s_lastUiMs >= cfg::UI_REFRESH_MS) {
    s_lastUiMs = now;
    updateBanner();
    s_active->updateBody();
  }
  if (!s_active && now - s_lastBatteryMs >= 5000) {
    s_lastBatteryMs = now;
    updateBattery();
  }
  delay(1);
}
