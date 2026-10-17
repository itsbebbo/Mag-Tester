// LVGL driver glue and shared widgets.
#pragma once

#include <lvgl.h>

namespace ui {

constexpr lv_coord_t SCREEN_W = 320;
constexpr lv_coord_t SCREEN_H = 240;
constexpr lv_coord_t BANNER_H = 64;
constexpr lv_coord_t BODY_H = SCREEN_H - BANNER_H;

void init(); // LVGL + display + touch; call after M5.begin()

// A key/value cell: small grey caption above a large value. Returns the value label.
lv_obj_t* cell(lv_obj_t* parent, const char* key, lv_coord_t w,
               const lv_font_t* valueFont = &lv_font_montserrat_20);

// Flex-wrap container filling `w` x `h`, for laying out cells.
lv_obj_t* grid(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);

// Small grey single-line footer label.
lv_obj_t* footer(lv_obj_t* parent, lv_coord_t w);

// Plain transparent container filling the parent, used to show/hide a group of widgets.
lv_obj_t* box(lv_obj_t* parent);

// Centred, wrapping information message for the test body.
lv_obj_t* message(lv_obj_t* parent);

// Show or hide an object (no-op if already in that state).
void setVisible(lv_obj_t* obj, bool visible);

// Update label text only if it changed (avoids needless redraws).
void setText(lv_obj_t* label, const char* text);
void setTextf(lv_obj_t* label, const char* fmt, ...);

// 3D wireframe view (field_view.cpp): a horizontal N/E/S/W ring seen in perspective,
// with an arrow along the magnetic field. Azimuth is clockwise from N; elevation is
// above (+) or below (-) the ring plane. Pass NAN azimuth to hide the arrow.
lv_obj_t* fieldView(lv_obj_t* parent, lv_coord_t w, lv_coord_t h);
void fieldViewSet(lv_obj_t* view, float azimuthDeg, float elevationDeg);

} // namespace ui
