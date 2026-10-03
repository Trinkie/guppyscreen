#ifndef __UI_WIDGETS_H__
#define __UI_WIDGETS_H__

// Building blocks of the glass UI: action tile with a progress outline and
// the temperature card used on the home screen.

#include "websocket_client.h"
#include "numpad.h"
#include "lvgl/lvgl.h"

#include <string>
#include <ctime>

class Tile {
 public:
  Tile(lv_obj_t *parent, const char *icon, const char *text, lv_event_cb_t cb, void *user_data);

  lv_obj_t *get_container();
  // icon turns accent colored (e.g. Print while a job is running)
  void set_active(bool active);
  // progress outline around the tile, pct in [0, 100]
  void set_progress(bool show, int pct, bool paused);
  // re-apply accent dependent colors
  void restyle();

 private:
  static void draw_ring_cb(lv_event_t *e);
  void draw_ring(lv_draw_ctx_t *draw_ctx);

  lv_obj_t *cont;
  lv_obj_t *bubble;
  lv_obj_t *icon;
  lv_obj_t *label;
  bool active;
  bool ring;
  bool paused;
  int pct;
};

class TempCard {
 public:
  TempCard(KWebSocketClient &ws,
           lv_obj_t *parent,
           const char *icon,
           const char *text,
           bool use_accent,
           lv_color_t color,
           bool editable,
           Numpad &np,
           const std::string &sensor_id,
           lv_obj_t *chart,
           lv_chart_series_t *series);
  ~TempCard();

  lv_obj_t *get_container();
  void update_target(int new_target);
  void update_value(int new_value);
  void update_series(int value);
  // re-apply accent dependent colors
  void apply_color();

 private:
  void refresh();
  static void edit_cb(lv_event_t *e);

  KWebSocketClient &ws;
  lv_obj_t *cont;
  lv_obj_t *bubble;
  lv_obj_t *icon;
  lv_obj_t *name;
  lv_obj_t *value_label;
  lv_obj_t *target_label;
  lv_obj_t *meter;
  bool use_accent;
  bool editable;
  lv_color_t color;
  Numpad &numpad;
  std::string id;
  lv_obj_t *chart;
  lv_chart_series_t *series;
  int value;
  int target;
  std::time_t last_updated_ts;
};

#endif // __UI_WIDGETS_H__
