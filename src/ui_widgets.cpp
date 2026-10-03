#include "ui_widgets.h"
#include "ui_style.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <set>

static std::set<Tile *> tiles;
static std::set<TempCard *> temp_cards;
static bool listener_registered = false;

static void register_listener();

/* ---------------------------------------------------------------- Tile */

Tile::Tile(lv_obj_t *parent, const char *icon_txt, const char *text,
           lv_event_cb_t cb, void *user_data)
  : cont(lv_obj_create(parent))
  , bubble(lv_obj_create(cont))
  , icon(ui::icon_label(bubble, icon_txt, &mdi_28, ui::text()))
  , label(ui::text_label(cont, text, &manrope_16, ui::text()))
  , active(false)
  , ring(false)
  , paused(false)
  , pct(0)
{
  ui::card(cont);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(cont, 8, 0);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(cont, 10, 0);
  lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(cont, 40, LV_STATE_PRESSED);

  ui::icon_bubble(bubble);
  lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(bubble, 52, 52);
  lv_obj_center(icon);

  if (cb != NULL) {
    lv_obj_add_event_cb(cont, cb, LV_EVENT_CLICKED, user_data);
  }
  lv_obj_add_event_cb(cont, &Tile::draw_ring_cb, LV_EVENT_DRAW_POST, this);

  tiles.insert(this);
  register_listener();
}

lv_obj_t *Tile::get_container() {
  return cont;
}

void Tile::set_active(bool a) {
  if (a != active) {
    active = a;
    restyle();
  }
}

void Tile::set_progress(bool show, int p, bool is_paused) {
  p = std::max(0, std::min(100, p));
  if (show != ring || p != pct || is_paused != paused) {
    ring = show;
    pct = p;
    paused = is_paused;
    restyle();
    lv_obj_invalidate(cont);
  }
}

void Tile::restyle() {
  lv_color_t c = paused ? ui::warn() : ui::accent();
  if (active) {
    lv_obj_set_style_text_color(icon, c, 0);
    lv_obj_set_style_bg_color(bubble, c, 0);
    lv_obj_set_style_bg_opa(bubble, 50, 0);
  } else {
    lv_obj_set_style_text_color(icon, ui::text(), 0);
    lv_obj_remove_local_style_prop(bubble, LV_STYLE_BG_COLOR, 0);
    lv_obj_remove_local_style_prop(bubble, LV_STYLE_BG_OPA, 0);
  }
  // the progress outline replaces the glass border while it is shown
  if (ring) {
    lv_obj_set_style_border_opa(cont, LV_OPA_TRANSP, 0);
  } else {
    lv_obj_remove_local_style_prop(cont, LV_STYLE_BORDER_OPA, 0);
  }
}

void Tile::draw_ring_cb(lv_event_t *e) {
  Tile *t = (Tile *)lv_event_get_user_data(e);
  if (t->ring) {
    t->draw_ring(lv_event_get_draw_ctx(e));
  }
}

/* Draws a rounded rectangle outline that starts at the top center and fills
 * clockwise up to pct percent of its perimeter. */
void Tile::draw_ring(lv_draw_ctx_t *draw_ctx) {
  const lv_coord_t stroke = 3;
  lv_area_t a;
  lv_obj_get_coords(cont, &a);

  float half = stroke / 2.0f;
  float x0 = a.x1 + half, y0 = a.y1 + half;
  float x1 = a.x2 - half, y1 = a.y2 - half;
  float r = std::max(0.0f, ui::RADIUS_CARD - half);
  float cx = (x0 + x1) / 2.0f;
  const float q = 1.5707963f * r; // quarter arc length

  struct Seg { bool arc; float ax, ay, bx, by; float ccx, ccy; int a0; float len; };
  Seg segs[9] = {
    { false, cx, y0, x1 - r, y0, 0, 0, 0, (x1 - r) - cx },
    { true, 0, 0, 0, 0, x1 - r, y0 + r, 270, q },
    { false, x1, y0 + r, x1, y1 - r, 0, 0, 0, (y1 - r) - (y0 + r) },
    { true, 0, 0, 0, 0, x1 - r, y1 - r, 0, q },
    { false, x1 - r, y1, x0 + r, y1, 0, 0, 0, (x1 - r) - (x0 + r) },
    { true, 0, 0, 0, 0, x0 + r, y1 - r, 90, q },
    { false, x0, y1 - r, x0, y0 + r, 0, 0, 0, (y1 - r) - (y0 + r) },
    { true, 0, 0, 0, 0, x0 + r, y0 + r, 180, q },
    { false, x0 + r, y0, cx, y0, 0, 0, 0, cx - (x0 + r) },
  };

  float total = 0;
  for (auto &s : segs) total += std::max(0.0f, s.len);

  lv_color_t col = paused ? ui::warn() : ui::accent();

  for (int pass = 0; pass < 2; pass++) {
    // pass 0: faint track, pass 1: progress
    float remaining = pass == 0 ? total : total * pct / 100.0f;
    if (remaining <= 0.5f) continue;

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.width = stroke;
    line.color = pass == 0 ? lv_color_white() : col;
    line.opa = pass == 0 ? 26 : LV_OPA_COVER;
    line.round_start = pass == 1;
    line.round_end = pass == 1;

    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.width = stroke;
    arc.color = line.color;
    arc.opa = line.opa;

    for (auto &s : segs) {
      if (remaining <= 0 || s.len <= 0) continue;
      float part = std::min(remaining, s.len);
      float f = part / s.len;
      if (s.arc) {
        lv_point_t c = { (lv_coord_t)s.ccx, (lv_coord_t)s.ccy };
        uint16_t start = s.a0;
        uint16_t end = s.a0 + (uint16_t)(90 * f + 0.5f);
        if (end > start) {
          lv_draw_arc(draw_ctx, &arc, &c, (uint16_t)(r + half), start, end);
        }
      } else {
        lv_point_t p1 = { (lv_coord_t)s.ax, (lv_coord_t)s.ay };
        lv_point_t p2 = { (lv_coord_t)(s.ax + (s.bx - s.ax) * f), (lv_coord_t)(s.ay + (s.by - s.ay) * f) };
        lv_draw_line(draw_ctx, &line, &p1, &p2);
      }
      remaining -= part;
    }
  }
}

/* ------------------------------------------------------------ TempCard */

TempCard::TempCard(KWebSocketClient &c,
                   lv_obj_t *parent,
                   const char *icon_txt,
                   const char *text,
                   bool accent,
                   lv_color_t col,
                   bool can_edit,
                   Numpad &np,
                   const std::string &sensor_id,
                   lv_obj_t *chart_obj,
                   lv_chart_series_t *chart_series)
  : ws(c)
  , cont(lv_obj_create(parent))
  , bubble(NULL)
  , icon(NULL)
  , name(NULL)
  , value_label(NULL)
  , target_label(NULL)
  , meter(NULL)
  , use_accent(accent)
  , editable(can_edit)
  , color(col)
  , numpad(np)
  , id(sensor_id)
  , chart(chart_obj)
  , series(chart_series)
  , value(0)
  , target(-1)
  , last_updated_ts(std::time(nullptr))
{
  ui::card(cont);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_hor(cont, 16, 0);
  lv_obj_set_style_pad_ver(cont, 12, 0);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  lv_obj_t *head = lv_obj_create(cont);
  ui::clear(head);
  lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(head, 8, 0);

  bubble = lv_obj_create(head);
  ui::icon_bubble(bubble);
  lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(bubble, 30, 30);
  icon = ui::icon_label(bubble, icon_txt, &mdi_20, ui::text());
  lv_obj_center(icon);
  name = ui::text_label(head, text, &manrope_16, ui::text2());

  lv_obj_t *row = lv_obj_create(cont);
  ui::clear(row);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(row, 6, 0);
  value_label = ui::text_label(row, "0\xC2\xB0", &manrope_28, ui::text());
  target_label = ui::text_label(row, editable ? "/ off" : "", &manrope_16, ui::text3());
  lv_obj_set_style_pad_bottom(target_label, 4, 0);

  meter = lv_bar_create(cont);
  lv_obj_set_size(meter, LV_PCT(100), 4);
  lv_bar_set_range(meter, 0, 100);
  lv_bar_set_value(meter, 0, LV_ANIM_OFF);

  if (editable) {
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(cont, 40, LV_STATE_PRESSED);
    lv_obj_add_event_cb(cont, &TempCard::edit_cb, LV_EVENT_CLICKED, this);
  }

  apply_color();
  temp_cards.insert(this);
  register_listener();
}

TempCard::~TempCard() {
  temp_cards.erase(this);
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
  if (series != NULL && chart != NULL) {
    lv_chart_remove_series(chart, series);
    series = NULL;
  }
}

lv_obj_t *TempCard::get_container() {
  return cont;
}

void TempCard::apply_color() {
  lv_color_t c = use_accent ? ui::accent() : color;
  lv_obj_set_style_text_color(icon, c, 0);
  lv_obj_set_style_bg_color(bubble, c, 0);
  lv_obj_set_style_bg_opa(bubble, 56, 0);
  lv_obj_set_style_bg_color(meter, c, LV_PART_INDICATOR);
  if (series != NULL && chart != NULL) {
    lv_chart_set_series_color(chart, series, c);
    lv_chart_refresh(chart);
  }
}

void TempCard::refresh() {
  int scale = target > 0 ? target : (id == "extruder" ? 300 : (id == "heater_bed" ? 120 : 80));
  int pct = scale > 0 ? std::min(100, value * 100 / scale) : 0;
  lv_bar_set_value(meter, std::max(0, pct), LV_ANIM_OFF);
}

void TempCard::update_target(int new_target) {
  if (new_target >= 0 && new_target != target) {
    target = new_target;
    if (editable) {
      lv_label_set_text(target_label, target > 0
                        ? fmt::format("/ {}\xC2\xB0", target).c_str()
                        : "/ off");
    }
    refresh();
  }
}

void TempCard::update_value(int new_value) {
  if (new_value != value) {
    value = new_value;
    lv_label_set_text(value_label, fmt::format("{}\xC2\xB0", value).c_str());
    refresh();
  }
}

void TempCard::update_series(int v) {
  if (series != NULL && chart != NULL) {
    auto delta = std::time(nullptr) - last_updated_ts;
    if (delta > 1) {
      lv_chart_set_next_value(chart, series, v);
      last_updated_ts = std::time(nullptr);
    }
  }
}

void TempCard::edit_cb(lv_event_t *e) {
  TempCard *t = (TempCard *)lv_event_get_user_data(e);
  t->numpad.set_callback([t](double v) {
    t->ws.gcode_script(fmt::format("SET_HEATER_TEMPERATURE HEATER={} TARGET={}", t->id, v));
  });
  t->numpad.foreground_reset();
}

/* ------------------------------------------------------------ restyle */

static void register_listener() {
  if (listener_registered) return;
  listener_registered = true;
  ui::on_change([]() {
    for (auto t : tiles) {
      t->restyle();
    }
    for (auto c : temp_cards) {
      c->apply_color();
    }
  });
}
