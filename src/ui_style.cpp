#include "ui_style.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace ui {

static const uint32_t ACCENTS[ACCENT_COUNT] = {
  0x0CF300, // fluidd green
  0x2F9BFF, // blue
  0x8B5CF6, // violet
  0xFF4FA3, // pink
  0xFF6B2C, // orange
  0xFFC21A, // amber
  0x19D3C5, // teal
  0xE9ECEA, // mono
};

static lv_color_t s_accent = lv_color_hex(ACCENTS[0]);
static bool s_acrylic = true;
static std::vector<std::function<void()>> s_listeners;

static lv_color_t *wall_px = NULL;
static lv_img_dsc_t wall_dsc;

static lv_style_t st_screen;       // screen and full screen panels: wallpaper
static lv_style_t st_dialog;       // screen level objects smaller than the screen
static lv_style_t st_clear;
static lv_style_t st_scrollbar;
static lv_style_t st_card;
static lv_style_t st_card_solid;
static lv_style_t st_pill;
static lv_style_t st_bubble;
static lv_style_t st_accent;
static lv_style_t st_danger_out;
static lv_style_t st_danger_fill;
static lv_style_t st_btn;
static lv_style_t st_pressed;
static lv_style_t st_checked;
static lv_style_t st_btnm_main;
static lv_style_t st_btnm_item;
static lv_style_t st_track;
static lv_style_t st_indicator;
static lv_style_t st_knob;
static lv_style_t st_arc_main;
static lv_style_t st_arc_ind;
static lv_style_t st_chart;
static lv_style_t st_field;
static lv_style_t st_imgbtn_pressed;
static lv_style_t st_imgbtn_disabled;

lv_color_t base()    { return lv_color_hex(0x0A0C0E); }
lv_color_t text()    { return lv_color_hex(0xF2F4F3); }
lv_color_t text2()   { return lv_color_hex(0xA7AFAB); }
lv_color_t text3()   { return lv_color_hex(0x6F7773); }
lv_color_t bed()     { return lv_color_hex(0xFF8A4C); }
lv_color_t chamber() { return lv_color_hex(0x8FA3B8); }
lv_color_t danger()  { return lv_color_hex(0xFF4D4F); }
lv_color_t warn()    { return lv_color_hex(0xFFC21A); }

lv_color_t accent_preset(size_t i) { return lv_color_hex(ACCENTS[i % ACCENT_COUNT]); }
lv_color_t accent() { return s_accent; }
bool acrylic() { return s_acrylic; }

static void rgb(lv_color_t c, float &r, float &g, float &b) {
  uint32_t v = lv_color_to32(c);
  r = (v >> 16) & 0xFF;
  g = (v >> 8) & 0xFF;
  b = v & 0xFF;
}

static float lin(float c) {
  c /= 255.0f;
  return c <= 0.03928f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

lv_color_t on_accent() {
  float r, g, b;
  rgb(s_accent, r, g, b);
  float l = 0.2126f * lin(r) + 0.7152f * lin(g) + 0.0722f * lin(b);
  return l > 0.35f ? lv_color_hex(0x07140A) : lv_color_white();
}

static lv_color_t mix(lv_color_t a, lv_color_t b, float t) {
  return lv_color_mix(a, b, (lv_opa_t)(t * 255));
}

void load_config() {
  Config *conf = Config::get_instance();
  auto a = conf->get_json("/accent_color");
  if (!a.is_null() && a.is_string()) {
    s_accent = lv_color_hex(std::stoul(a.template get<std::string>(), nullptr, 16));
  }
  auto ac = conf->get_json("/acrylic");
  if (!ac.is_null() && ac.is_boolean()) {
    s_acrylic = ac.template get<bool>();
  }
}

static void save_config() {
  Config *conf = Config::get_instance();
  conf->set<std::string>("/accent_color", fmt::format("0x{:06X}", lv_color_to32(s_accent) & 0xFFFFFF));
  conf->set<bool>("/acrylic", s_acrylic);
  conf->save();
}

/* Wallpaper: dark base with soft accent glows. Rendered once into a
 * true color image; every full screen panel uses it as background, and the
 * translucent cards on top of it read as frosted glass. */
static void render_wallpaper() {
  lv_coord_t w = lv_disp_get_hor_res(NULL);
  lv_coord_t h = lv_disp_get_ver_res(NULL);
  if (wall_px == NULL) {
    wall_px = (lv_color_t *)malloc(sizeof(lv_color_t) * w * h);
    if (wall_px == NULL) {
      spdlog::error("wallpaper allocation failed");
      return;
    }
  }

  struct Blob { float cx, cy, rx, ry, a; lv_color_t c; };
  float sx = w / 800.0f, sy = h / 480.0f;
  float k = s_acrylic ? 1.0f : 0.25f;
  Blob blobs[3] = {
    {  90 * sx,  70 * sy, 300 * sx, 230 * sy, 0.42f * k, s_accent },
    { 710 * sx, 450 * sy, 280 * sx, 220 * sy, 0.34f * k, mix(s_accent, lv_color_hex(0x2F6BFF), 0.55f) },
    { 460 * sx, 280 * sy, 190 * sx, 160 * sy, 0.10f * k, mix(s_accent, lv_color_white(), 0.30f) },
  };

  // gaussian falloff is separable: precompute per column and per row terms
  std::vector<float> fx[3], fy[3];
  for (int i = 0; i < 3; i++) {
    fx[i].resize(w);
    fy[i].resize(h);
    for (int x = 0; x < w; x++) {
      float d = (x - blobs[i].cx) / blobs[i].rx;
      fx[i][x] = std::exp(-2.5f * d * d);
    }
    for (int y = 0; y < h; y++) {
      float d = (y - blobs[i].cy) / blobs[i].ry;
      fy[i][y] = std::exp(-2.5f * d * d);
    }
  }

  static const float bayer[4][4] = {
    { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 }
  };

  float br, bg, bb;
  rgb(base(), br, bg, bb);
  float cr[3], cg[3], cb[3];
  for (int i = 0; i < 3; i++) rgb(blobs[i].c, cr[i], cg[i], cb[i]);

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float r = br, g = bg, b = bb;
      for (int i = 0; i < 3; i++) {
        float a = blobs[i].a * fx[i][x] * fy[i][y];
        r += (cr[i] - r) * a;
        g += (cg[i] - g) * a;
        b += (cb[i] - b) * a;
      }
      float d = bayer[y & 3][x & 3] / 16.0f - 0.47f;
      wall_px[y * w + x] = lv_color_make((uint8_t)LV_CLAMP(0.0f, r + d, 255.0f),
                                         (uint8_t)LV_CLAMP(0.0f, g + d, 255.0f),
                                         (uint8_t)LV_CLAMP(0.0f, b + d, 255.0f));
    }
  }

  wall_dsc.header.always_zero = 0;
  wall_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  wall_dsc.header.w = w;
  wall_dsc.header.h = h;
  wall_dsc.data_size = sizeof(lv_color_t) * w * h;
  wall_dsc.data = (const uint8_t *)wall_px;
  lv_img_cache_invalidate_src(&wall_dsc);
}

const lv_img_dsc_t *wallpaper() {
  return &wall_dsc;
}

// colors that depend on accent / acrylic
static void update_dynamic_styles() {
  lv_color_t a = s_accent;
  lv_color_t on = on_accent();
  lv_color_t white = lv_color_white();

  // frosted card
  if (s_acrylic) {
    lv_style_set_bg_color(&st_card, white);
    lv_style_set_bg_grad_color(&st_card, lv_color_hex(0x9AA6B2));
    lv_style_set_bg_grad_dir(&st_card, LV_GRAD_DIR_VER);
    lv_style_set_bg_opa(&st_card, 18);
    lv_style_set_border_color(&st_card, white);
    lv_style_set_border_opa(&st_card, 34);
  } else {
    lv_style_set_bg_color(&st_card, lv_color_hex(0x171A1E));
    lv_style_set_bg_grad_dir(&st_card, LV_GRAD_DIR_NONE);
    lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
    lv_style_set_border_color(&st_card, white);
    lv_style_set_border_opa(&st_card, 16);
  }

  lv_color_t strong = s_acrylic ? white : lv_color_hex(0x22262C);
  lv_opa_t strong_opa = s_acrylic ? 28 : LV_OPA_COVER;
  lv_style_set_bg_color(&st_pill, strong);
  lv_style_set_bg_opa(&st_pill, strong_opa);
  lv_style_set_bg_color(&st_bubble, strong);
  lv_style_set_bg_opa(&st_bubble, strong_opa);
  lv_style_set_bg_color(&st_btn, strong);
  lv_style_set_bg_opa(&st_btn, strong_opa);
  lv_style_set_bg_color(&st_btnm_item, strong);
  lv_style_set_bg_opa(&st_btnm_item, strong_opa);

  lv_style_set_bg_color(&st_accent, a);
  lv_style_set_text_color(&st_accent, on);
  lv_style_set_img_recolor(&st_accent, on);
  lv_style_set_bg_color(&st_pressed, a);
  lv_style_set_text_color(&st_pressed, on);
  lv_style_set_bg_color(&st_checked, a);
  lv_style_set_text_color(&st_checked, on);
  lv_style_set_bg_color(&st_indicator, a);
  lv_style_set_arc_color(&st_arc_ind, a);
  lv_style_set_border_color(&st_field, a);
  lv_style_set_img_recolor(&st_imgbtn_pressed, a);
}

static void init_styles() {
  lv_color_t white = lv_color_white();

  lv_style_init(&st_screen);
  lv_style_set_bg_color(&st_screen, base());
  lv_style_set_bg_opa(&st_screen, LV_OPA_COVER);
  lv_style_set_bg_img_src(&st_screen, &wall_dsc);
  lv_style_set_radius(&st_screen, 0);
  lv_style_set_border_width(&st_screen, 0);
  lv_style_set_text_color(&st_screen, text());

  lv_style_init(&st_dialog);
  lv_style_set_bg_color(&st_dialog, lv_color_hex(0x14171B));
  lv_style_set_bg_opa(&st_dialog, 250);
  lv_style_set_radius(&st_dialog, 22);
  lv_style_set_border_width(&st_dialog, 1);
  lv_style_set_border_color(&st_dialog, white);
  lv_style_set_border_opa(&st_dialog, 30);
  lv_style_set_text_color(&st_dialog, text());

  lv_style_init(&st_clear);
  lv_style_set_bg_opa(&st_clear, LV_OPA_TRANSP);
  lv_style_set_border_width(&st_clear, 0);
  lv_style_set_radius(&st_clear, 0);
  lv_style_set_shadow_width(&st_clear, 0);
  lv_style_set_outline_width(&st_clear, 0);

  lv_style_init(&st_scrollbar);
  lv_style_set_bg_color(&st_scrollbar, white);
  lv_style_set_bg_opa(&st_scrollbar, 60);
  lv_style_set_radius(&st_scrollbar, LV_RADIUS_CIRCLE);
  lv_style_set_width(&st_scrollbar, 4);
  lv_style_set_pad_right(&st_scrollbar, 4);

  lv_style_init(&st_card);
  lv_style_set_radius(&st_card, RADIUS_CARD);
  lv_style_set_border_width(&st_card, 1);
  lv_style_set_shadow_width(&st_card, 0);
  lv_style_set_text_color(&st_card, text());

  lv_style_init(&st_card_solid);
  lv_style_set_bg_color(&st_card_solid, lv_color_hex(0x14171B));
  lv_style_set_bg_opa(&st_card_solid, 250);
  lv_style_set_radius(&st_card_solid, 22);
  lv_style_set_border_width(&st_card_solid, 1);
  lv_style_set_border_color(&st_card_solid, white);
  lv_style_set_border_opa(&st_card_solid, 30);
  lv_style_set_text_color(&st_card_solid, text());

  lv_style_init(&st_pill);
  lv_style_set_radius(&st_pill, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&st_pill, 0);
  lv_style_set_text_color(&st_pill, text());

  lv_style_init(&st_bubble);
  lv_style_set_radius(&st_bubble, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&st_bubble, 0);
  lv_style_set_pad_all(&st_bubble, 0);

  lv_style_init(&st_accent);
  lv_style_set_bg_opa(&st_accent, LV_OPA_COVER);
  lv_style_set_bg_grad_dir(&st_accent, LV_GRAD_DIR_NONE);
  lv_style_set_border_width(&st_accent, 0);
  lv_style_set_img_recolor_opa(&st_accent, LV_OPA_COVER);

  lv_style_init(&st_danger_out);
  lv_style_set_border_color(&st_danger_out, danger());
  lv_style_set_border_opa(&st_danger_out, 110);
  lv_style_set_border_width(&st_danger_out, 1);
  lv_style_set_text_color(&st_danger_out, danger());
  lv_style_set_img_recolor(&st_danger_out, danger());
  lv_style_set_img_recolor_opa(&st_danger_out, LV_OPA_COVER);

  lv_style_init(&st_danger_fill);
  lv_style_set_bg_color(&st_danger_fill, danger());
  lv_style_set_bg_opa(&st_danger_fill, LV_OPA_COVER);
  lv_style_set_bg_grad_dir(&st_danger_fill, LV_GRAD_DIR_NONE);
  lv_style_set_border_width(&st_danger_fill, 0);
  lv_style_set_text_color(&st_danger_fill, white);
  lv_style_set_img_recolor(&st_danger_fill, white);
  lv_style_set_img_recolor_opa(&st_danger_fill, LV_OPA_COVER);

  lv_style_init(&st_btn);
  lv_style_set_radius(&st_btn, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&st_btn, 0);
  lv_style_set_shadow_width(&st_btn, 0);
  lv_style_set_bg_grad_dir(&st_btn, LV_GRAD_DIR_NONE);
  lv_style_set_text_color(&st_btn, text());

  lv_style_init(&st_pressed);
  lv_style_set_bg_opa(&st_pressed, LV_OPA_COVER);
  lv_style_set_bg_grad_dir(&st_pressed, LV_GRAD_DIR_NONE);

  lv_style_init(&st_checked);
  lv_style_set_bg_opa(&st_checked, LV_OPA_COVER);
  lv_style_set_bg_grad_dir(&st_checked, LV_GRAD_DIR_NONE);

  lv_style_init(&st_btnm_main);
  lv_style_set_bg_opa(&st_btnm_main, LV_OPA_TRANSP);
  lv_style_set_border_width(&st_btnm_main, 0);
  lv_style_set_pad_all(&st_btnm_main, 6);
  lv_style_set_pad_gap(&st_btnm_main, 8);

  lv_style_init(&st_btnm_item);
  lv_style_set_radius(&st_btnm_item, RADIUS_INNER);
  lv_style_set_border_width(&st_btnm_item, 0);
  lv_style_set_shadow_width(&st_btnm_item, 0);
  lv_style_set_bg_grad_dir(&st_btnm_item, LV_GRAD_DIR_NONE);
  lv_style_set_text_color(&st_btnm_item, text());

  lv_style_init(&st_track);
  lv_style_set_bg_color(&st_track, white);
  lv_style_set_bg_opa(&st_track, 24);
  lv_style_set_radius(&st_track, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&st_track, 0);

  lv_style_init(&st_indicator);
  lv_style_set_bg_opa(&st_indicator, LV_OPA_COVER);
  lv_style_set_bg_grad_dir(&st_indicator, LV_GRAD_DIR_NONE);
  lv_style_set_radius(&st_indicator, LV_RADIUS_CIRCLE);

  lv_style_init(&st_knob);
  lv_style_set_bg_color(&st_knob, white);
  lv_style_set_bg_opa(&st_knob, LV_OPA_COVER);
  lv_style_set_radius(&st_knob, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&st_knob, 0);
  lv_style_set_shadow_width(&st_knob, 0);

  lv_style_init(&st_arc_main);
  lv_style_set_arc_color(&st_arc_main, white);
  lv_style_set_arc_opa(&st_arc_main, 30);
  lv_style_set_arc_rounded(&st_arc_main, true);

  lv_style_init(&st_arc_ind);
  lv_style_set_arc_rounded(&st_arc_ind, true);

  lv_style_init(&st_chart);
  lv_style_set_bg_opa(&st_chart, LV_OPA_TRANSP);
  lv_style_set_border_width(&st_chart, 0);
  lv_style_set_line_color(&st_chart, white);
  lv_style_set_line_opa(&st_chart, 20);
  lv_style_set_text_color(&st_chart, text3());
  lv_style_set_text_font(&st_chart, &manrope_14);

  lv_style_init(&st_field);
  lv_style_set_bg_color(&st_field, white);
  lv_style_set_bg_opa(&st_field, 22);
  lv_style_set_radius(&st_field, RADIUS_INNER);
  lv_style_set_border_width(&st_field, 1);
  lv_style_set_border_opa(&st_field, 120);
  lv_style_set_text_color(&st_field, text());

  lv_style_init(&st_imgbtn_pressed);
  lv_style_set_img_recolor_opa(&st_imgbtn_pressed, LV_OPA_COVER);

  lv_style_init(&st_imgbtn_disabled);
  lv_style_set_img_recolor_opa(&st_imgbtn_disabled, LV_OPA_COVER);
  lv_style_set_img_recolor(&st_imgbtn_disabled, text3());

  update_dynamic_styles();
}

void init() {
  render_wallpaper();
  init_styles();
}

static void notify() {
  lv_obj_report_style_change(NULL);
  for (auto &cb : s_listeners) {
    cb();
  }
  lv_obj_invalidate(lv_scr_act());
  lv_obj_invalidate(lv_layer_top());
}

void set_accent(lv_color_t c) {
  s_accent = c;
  lv_disp_t *disp = lv_disp_get_default();
  // refresh the parent (default) theme so stock widgets pick up the accent too
  lv_theme_default_init(disp, s_accent, danger(), true, &manrope_20);
  render_wallpaper();
  update_dynamic_styles();
  save_config();
  notify();
}

void set_acrylic(bool on) {
  s_acrylic = on;
  render_wallpaper();
  update_dynamic_styles();
  save_config();
  notify();
}

void on_change(std::function<void()> cb) {
  s_listeners.push_back(cb);
}

static bool is_screen_level(lv_obj_t *obj) {
  lv_obj_t *parent = lv_obj_get_parent(obj);
  if (parent == NULL) {
    return obj != lv_layer_top() && obj != lv_layer_sys();
  }
  return lv_obj_get_parent(parent) == NULL
    && parent != lv_layer_top()
    && parent != lv_layer_sys();
}

// screen level containers get the wallpaper only when they cover the whole
// screen (panels); smaller ones (prompts) become opaque dialogs.
static void panel_size_cb(lv_event_t *e) {
  lv_obj_t *obj = lv_event_get_target(e);
  bool full = lv_obj_get_width(obj) >= lv_disp_get_hor_res(NULL)
    && lv_obj_get_height(obj) >= lv_disp_get_ver_res(NULL);
  lv_obj_remove_style(obj, &st_screen, LV_PART_MAIN);
  lv_obj_remove_style(obj, &st_dialog, LV_PART_MAIN);
  lv_obj_add_style(obj, full ? &st_screen : &st_dialog, LV_PART_MAIN);
}

void theme_apply_cb(lv_theme_t *th, lv_obj_t *obj) {
  LV_UNUSED(th);

  if (lv_obj_check_type(obj, &lv_obj_class)) {
    if (lv_obj_get_parent(obj) == NULL) {
      if (obj != lv_layer_top() && obj != lv_layer_sys()) {
        lv_obj_add_style(obj, &st_screen, LV_PART_MAIN);
      }
    } else if (is_screen_level(obj)) {
      lv_obj_add_style(obj, &st_screen, LV_PART_MAIN);
      lv_obj_add_event_cb(obj, panel_size_cb, LV_EVENT_SIZE_CHANGED, NULL);
    } else {
      lv_obj_add_style(obj, &st_clear, LV_PART_MAIN);
    }
    lv_obj_add_style(obj, &st_scrollbar, LV_PART_SCROLLBAR);
    return;
  }

  if (lv_obj_check_type(obj, &lv_tabview_class)) {
    lv_obj_add_style(obj, &st_screen, LV_PART_MAIN);
    return;
  }

  if (lv_obj_check_type(obj, &lv_btn_class)) {
    lv_obj_add_style(obj, &st_btn, LV_PART_MAIN);
    lv_obj_add_style(obj, &st_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(obj, &st_checked, LV_PART_MAIN | LV_STATE_CHECKED);
    return;
  }

  if (lv_obj_check_type(obj, &lv_imgbtn_class)) {
    lv_obj_add_style(obj, &st_imgbtn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(obj, &st_imgbtn_disabled, LV_STATE_DISABLED);
    return;
  }

  if (lv_obj_check_type(obj, &lv_btnmatrix_class) || lv_obj_check_type(obj, &lv_keyboard_class)) {
    lv_obj_add_style(obj, &st_btnm_main, LV_PART_MAIN);
    lv_obj_add_style(obj, &st_btnm_item, LV_PART_ITEMS);
    lv_obj_add_style(obj, &st_pressed, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_style(obj, &st_checked, LV_PART_ITEMS | LV_STATE_CHECKED);
    return;
  }

  if (lv_obj_check_type(obj, &lv_msgbox_class)) {
    lv_obj_add_style(obj, &st_card_solid, LV_PART_MAIN);
    return;
  }

  if (lv_obj_check_type(obj, &lv_bar_class)) {
    lv_obj_add_style(obj, &st_track, LV_PART_MAIN);
    lv_obj_add_style(obj, &st_indicator, LV_PART_INDICATOR);
    return;
  }

  if (lv_obj_check_type(obj, &lv_slider_class)) {
    lv_obj_add_style(obj, &st_track, LV_PART_MAIN);
    lv_obj_add_style(obj, &st_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(obj, &st_knob, LV_PART_KNOB);
    return;
  }

  if (lv_obj_check_type(obj, &lv_switch_class)) {
    lv_obj_add_style(obj, &st_track, LV_PART_MAIN);
    lv_obj_add_style(obj, &st_indicator, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(obj, &st_knob, LV_PART_KNOB);
    return;
  }

  if (lv_obj_check_type(obj, &lv_arc_class) || lv_obj_check_type(obj, &lv_spinner_class)) {
    lv_obj_add_style(obj, &st_arc_main, LV_PART_MAIN);
    lv_obj_add_style(obj, &st_arc_ind, LV_PART_INDICATOR);
    return;
  }

  if (lv_obj_check_type(obj, &lv_chart_class)) {
    lv_obj_add_style(obj, &st_chart, LV_PART_MAIN);
    lv_obj_add_style(obj, &st_chart, LV_PART_TICKS);
    return;
  }

  if (lv_obj_check_type(obj, &lv_textarea_class) || lv_obj_check_type(obj, &lv_dropdown_class)) {
    lv_obj_add_style(obj, &st_field, LV_PART_MAIN);
    return;
  }

  if (lv_obj_check_type(obj, &lv_dropdownlist_class)) {
    lv_obj_add_style(obj, &st_card_solid, LV_PART_MAIN);
    lv_obj_add_style(obj, &st_checked, LV_PART_SELECTED | LV_STATE_CHECKED);
    return;
  }
}

void card(lv_obj_t *obj) {
  lv_obj_add_style(obj, &st_card, LV_PART_MAIN);
}

void card_solid(lv_obj_t *obj) {
  lv_obj_add_style(obj, &st_card_solid, LV_PART_MAIN);
}

void pill(lv_obj_t *obj) {
  lv_obj_add_style(obj, &st_pill, LV_PART_MAIN);
}

void icon_bubble(lv_obj_t *obj) {
  lv_obj_add_style(obj, &st_bubble, LV_PART_MAIN);
}

void clear(lv_obj_t *obj) {
  lv_obj_add_style(obj, &st_clear, LV_PART_MAIN);
  lv_obj_set_style_pad_all(obj, 0, 0);
}

void accent_fill(lv_obj_t *obj) {
  lv_obj_add_style(obj, &st_accent, LV_PART_MAIN);
}

void danger_outline(lv_obj_t *obj) {
  lv_obj_add_style(obj, &st_danger_out, LV_PART_MAIN);
}

void danger_fill(lv_obj_t *obj) {
  lv_obj_add_style(obj, &st_danger_fill, LV_PART_MAIN);
}

lv_obj_t *icon_label(lv_obj_t *parent, const char *icon, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, icon);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

lv_obj_t *text_label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

} // namespace ui
