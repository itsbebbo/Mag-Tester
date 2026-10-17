#include "ui.h"

#include <M5Unified.h>
#include <math.h>
#include <stdarg.h>

namespace ui {

// ---- LVGL driver glue -------------------------------------------------------

static constexpr size_t kBufLines = 40;
static lv_disp_draw_buf_t s_drawBuf;
static lv_disp_drv_t s_dispDrv;
static lv_indev_drv_t s_indevDrv;

static void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px) {
  const uint32_t w = area->x2 - area->x1 + 1;
  const uint32_t h = area->y2 - area->y1 + 1;
  M5.Display.startWrite();
  M5.Display.setAddrWindow(area->x1, area->y1, w, h);
  M5.Display.writePixels(reinterpret_cast<m5gfx::rgb565_t*>(&px->full), w * h);
  M5.Display.endWrite();
  lv_disp_flush_ready(drv);
}

static void touchCb(lv_indev_drv_t*, lv_indev_data_t* data) {
  // M5.update() (called from loop) refreshes the touch state.
  const auto t = M5.Touch.getDetail();
  if (t.isPressed()) {
    data->point.x = t.x;
    data->point.y = t.y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void init() {
  lv_init();

  const size_t px = SCREEN_W * kBufLines;
  auto* buf1 = static_cast<lv_color_t*>(heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA));
  auto* buf2 = static_cast<lv_color_t*>(heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_DMA));
  lv_disp_draw_buf_init(&s_drawBuf, buf1, buf2, px);

  lv_disp_drv_init(&s_dispDrv);
  s_dispDrv.hor_res = M5.Display.width();
  s_dispDrv.ver_res = M5.Display.height();
  s_dispDrv.flush_cb = flushCb;
  s_dispDrv.draw_buf = &s_drawBuf;
  lv_disp_drv_register(&s_dispDrv);

  lv_indev_drv_init(&s_indevDrv);
  s_indevDrv.type = LV_INDEV_TYPE_POINTER;
  s_indevDrv.read_cb = touchCb;
  lv_indev_drv_register(&s_indevDrv);
}

// ---- Widgets -----------------------------------------------------------------

lv_obj_t* grid(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
  lv_obj_t* g = lv_obj_create(parent);
  lv_obj_remove_style_all(g);
  lv_obj_set_size(g, w, h);
  lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(g, 2, 0);
  lv_obj_set_style_pad_column(g, 0, 0);
  lv_obj_clear_flag(g, LV_OBJ_FLAG_SCROLLABLE);
  return g;
}

lv_obj_t* cell(lv_obj_t* parent, const char* key, lv_coord_t w, const lv_font_t* valueFont) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_set_size(c, w, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_hor(c, 4, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* k = lv_label_create(c);
  lv_label_set_text(k, key);
  lv_obj_set_style_text_font(k, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(k, lv_palette_main(LV_PALETTE_GREY), 0);

  lv_obj_t* v = lv_label_create(c);
  lv_label_set_text(v, "--");
  lv_obj_set_width(v, lv_pct(100));
  lv_label_set_long_mode(v, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(v, valueFont, 0);
  lv_obj_set_style_text_color(v, lv_color_white(), 0);
  return v;
}

lv_obj_t* footer(lv_obj_t* parent, lv_coord_t w) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_width(l, w);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_label_set_text(l, "");
  lv_obj_set_style_pad_hor(l, 4, 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
  return l;
}

lv_obj_t* box(lv_obj_t* parent) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, lv_pct(100), lv_pct(100));
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  return b;
}

lv_obj_t* message(lv_obj_t* parent) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_width(l, SCREEN_W - 24);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(l, lv_color_white(), 0);
  lv_label_set_text(l, "");
  return l;
}

void setVisible(lv_obj_t* obj, bool visible) {
  if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == !visible) return;
  if (visible) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void setText(lv_obj_t* label, const char* text) {
  if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void setTextf(lv_obj_t* label, const char* fmt, ...) {
  char buf[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  setText(label, buf);
}

} // namespace ui
