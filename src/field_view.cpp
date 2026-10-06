// 3D wireframe magnetic field view (declared in ui.h).
//
// World axes: E to the right, N away from the viewer, U up. The camera looks
// down on the horizontal compass ring from kTilt above the horizon, with mild
// perspective so near parts of the scene are drawn slightly larger.
#include <math.h>

#include <initializer_list>

#include "ui.h"

namespace ui {
namespace {

constexpr float kDeg = (float)M_PI / 180.0f;
constexpr float kTilt = 30.0f * kDeg;
constexpr float kPerspective = 0.15f;
constexpr int kRingSegments = 48;

struct FieldState {
  float azDeg = NAN; // NAN = no arrow
  float elDeg = 0;
};

struct Vec3 {
  float e, n, u;
};

Vec3 operator*(const Vec3& v, float k) { return {v.e * k, v.n * k, v.u * k}; }
Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.e + b.e, a.n + b.n, a.u + b.u}; }
Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.n * b.u - a.u * b.n, a.u * b.e - a.e * b.u, a.e * b.n - a.n * b.e};
}

struct Projector {
  float cx, cy, r, st, ct;

  explicit Projector(const lv_area_t& a)
      : cx((a.x1 + a.x2) * 0.5f), cy((a.y1 + a.y2) * 0.5f), r((a.x2 - a.x1 + 1) * 0.34f),
        st(sinf(kTilt)), ct(cosf(kTilt)) {}

  lv_point_t operator()(const Vec3& v) const {
    const float up = v.u * ct + v.n * st;    // towards the top of the screen
    const float depth = v.n * ct - v.u * st; // + = away from the viewer
    const float s = 1.0f / (1.0f + kPerspective * depth);
    return {(lv_coord_t)lroundf(cx + v.e * r * s), (lv_coord_t)lroundf(cy - up * r * s)};
  }
};

Vec3 onRing(float azDeg, float radius) {
  return {sinf(azDeg * kDeg) * radius, cosf(azDeg * kDeg) * radius, 0};
}

void line(lv_draw_ctx_t* ctx, const lv_draw_line_dsc_t& dsc, const Projector& p, const Vec3& a,
          const Vec3& b) {
  const lv_point_t pa = p(a), pb = p(b);
  lv_draw_line(ctx, &dsc, &pa, &pb);
}

void drawCb(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target(e);
  lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
  const auto* st = static_cast<const FieldState*>(lv_obj_get_user_data(obj));
  lv_area_t area;
  lv_obj_get_coords(obj, &area);
  const Projector p(area);
  const Vec3 origin{0, 0, 0};

  lv_draw_line_dsc_t ld;
  lv_draw_line_dsc_init(&ld);

  // Vertical reference axis.
  ld.color = lv_palette_darken(LV_PALETTE_GREY, 2);
  ld.width = 1;
  ld.dash_width = 3;
  ld.dash_gap = 3;
  line(ctx, ld, p, {0, 0, -0.9f}, {0, 0, 0.9f});
  ld.dash_width = ld.dash_gap = 0;

  // Horizontal compass ring, with ticks every 30 deg and longer ones at N/E/S/W.
  ld.color = lv_palette_main(LV_PALETTE_GREY);
  ld.width = 2;
  for (int i = 0; i < kRingSegments; i++) {
    line(ctx, ld, p, onRing(i * 360.0f / kRingSegments, 1), onRing((i + 1) * 360.0f / kRingSegments, 1));
  }
  for (int a = 0; a < 360; a += 30) {
    const bool cardinal = a % 90 == 0;
    ld.color = cardinal ? lv_color_white() : lv_palette_main(LV_PALETTE_GREY);
    line(ctx, ld, p, onRing(a, cardinal ? 0.78f : 0.88f), onRing(a, 1));
  }

  lv_draw_label_dsc_t td;
  lv_draw_label_dsc_init(&td);
  td.font = &lv_font_montserrat_14;
  td.align = LV_TEXT_ALIGN_CENTER;
  static const char* kNames[] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; i++) {
    const lv_point_t c = p(onRing(i * 90.0f, 1.22f));
    const lv_area_t box = {(lv_coord_t)(c.x - 8), (lv_coord_t)(c.y - 8), (lv_coord_t)(c.x + 8),
                           (lv_coord_t)(c.y + 8)};
    td.color = i == 0 ? lv_palette_main(LV_PALETTE_RED) : lv_color_white();
    lv_draw_label(ctx, &td, &box, kNames[i], nullptr);
  }

  if (isnan(st->azDeg)) return;

  // Field direction: azimuth clockwise from N, elevation above the ring plane.
  const float az = st->azDeg * kDeg, el = st->elDeg * kDeg;
  const Vec3 dir{sinf(az) * cosf(el), cosf(az) * cosf(el), sinf(el)};
  const Vec3 tip = dir * 1.05f;
  const Vec3 ground{tip.e, tip.n, 0};

  // Shadow on the ring plane plus a drop line, so the 3D direction reads clearly.
  ld.color = lv_palette_darken(LV_PALETTE_ORANGE, 3);
  ld.width = 2;
  line(ctx, ld, p, origin, ground);
  ld.width = 1;
  line(ctx, ld, p, ground, tip);

  // Arrow shaft and a four-sided cone head.
  ld.color = lv_palette_main(LV_PALETTE_ORANGE);
  ld.width = 4;
  ld.round_start = ld.round_end = 1;
  line(ctx, ld, p, origin, tip);

  const Vec3 side{cosf(az), -sinf(az), 0}; // horizontal, perpendicular to the arrow
  const Vec3 side2 = cross(dir, side);
  const Vec3 back = dir * 0.72f;
  ld.width = 2;
  for (const Vec3& s : {side, side * -1.0f, side2, side2 * -1.0f}) line(ctx, ld, p, tip, back + s * 0.14f);
}

void deleteCb(lv_event_t* e) {
  delete static_cast<FieldState*>(lv_obj_get_user_data(lv_event_get_target(e)));
}

} // namespace

lv_obj_t* fieldView(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_size(obj, w, h);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_user_data(obj, new FieldState);
  lv_obj_add_event_cb(obj, drawCb, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_add_event_cb(obj, deleteCb, LV_EVENT_DELETE, nullptr);
  return obj;
}

void fieldViewSet(lv_obj_t* obj, float azimuthDeg, float elevationDeg) {
  auto* st = static_cast<FieldState*>(lv_obj_get_user_data(obj));
  const bool hide = isnan(azimuthDeg);
  if (hide && isnan(st->azDeg)) return;
  if (!hide && !isnan(st->azDeg) && fabsf(azimuthDeg - st->azDeg) < 0.5f &&
      fabsf(elevationDeg - st->elDeg) < 0.5f) {
    return; // no visible change - skip the redraw
  }
  st->azDeg = azimuthDeg;
  st->elDeg = elevationDeg;
  lv_obj_invalidate(obj);
}

} // namespace ui
