#include "main_panel.h"
#include "state.h"
#include "config.h"
#include "ui_style.h"
#include "lvgl/lvgl.h"
#include "spdlog/spdlog.h"

#include <string>
#include <ctime>

MainPanel::MainPanel(KWebSocketClient &websocket,
		     std::mutex &lock,
		     SpoolmanPanel &sm)
  : NotifyConsumer(lock)
  , ws(websocket)
  , homing_panel(ws, lock)
  , fan_panel(ws, lock)
  , led_panel(ws, lock)
  , tabview(lv_tabview_create(lv_scr_act(), LV_DIR_LEFT, 64))
  , main_tab(lv_tabview_add_tab(tabview, ICON_HOME))
  , macros_tab(lv_tabview_add_tab(tabview, ICON_MACROS))
  , macros_panel(ws, lock, macros_tab)
  , console_tab(lv_tabview_add_tab(tabview, ICON_CONSOLE))
  , console_panel(ws, lock, console_tab)
  , printertune_tab(lv_tabview_add_tab(tabview, ICON_TUNE))
  , setting_tab(lv_tabview_add_tab(tabview, ICON_SETTINGS))
  , setting_panel(websocket, lock, setting_tab, sm)
  , main_cont(lv_obj_create(main_tab))
  , print_status_panel(websocket, lock, main_cont)
  , print_panel(ws, lock, print_status_panel)
  , printertune_panel(ws, lock, printertune_tab, print_status_panel.get_finetune_panel())
  , numpad(Numpad(main_cont))
  , extruder_panel(ws, lock, numpad, sm)
  , prompt_panel(websocket, lock, main_cont)
  , spoolman_panel(sm)
  , topbar(lv_obj_create(main_cont))
  , title_label(NULL)
  , state_chip(NULL)
  , state_label(NULL)
  , clock_label(NULL)
  , clock_timer(NULL)
  , temp_cont(lv_obj_create(main_cont))
  , chart_card(lv_obj_create(main_cont))
  , temp_chart(lv_chart_create(chart_card))
  , tiles_cont(lv_obj_create(main_cont))
  , homing_btn(tiles_cont, ICON_MOVE, "Move", &MainPanel::_handle_homing_cb, this)
  , extrude_btn(tiles_cont, ICON_NOZZLE, "Extrude", &MainPanel::_handle_extrude_cb, this)
  , action_btn(tiles_cont, ICON_FAN, "Fans", &MainPanel::_handle_fanpanel_cb, this)
  , led_btn(tiles_cont, ICON_LIGHT, "LED", &MainPanel::_handle_ledpanel_cb, this)
  , cooldown_btn(tiles_cont, ICON_COOLDOWN, "Cooldown", &MainPanel::_handle_cooldown_cb, this)
  , print_btn(tiles_cont, ICON_PRINTER, "Print", &MainPanel::_handle_print_cb, this)
  , print_active(false)
{
    lv_style_init(&style);

    print_status_panel.get_mini_status().set_listener(
      [this](bool active, int progress, const std::string &status) {
	update_print_state(active, progress, status);
      });

    ws.register_notify_update(this);
}

MainPanel::~MainPanel() {
  if (clock_timer != NULL) {
    lv_timer_del(clock_timer);
    clock_timer = NULL;
  }

  if (tabview != NULL) {
    lv_obj_del(tabview);
    tabview = NULL;
  }

  sensors.clear();
}

void MainPanel::subscribe() {
  spdlog::trace("main panel subscribing");
  ws.send_jsonrpc("printer.gcode.help", [this](json &d) { console_panel.handle_macros(d); });
  print_panel.subscribe();
}

PrinterTunePanel& MainPanel::get_tune_panel() {
  return printertune_panel;
}

static const char *state_text(const std::string &s) {
  if (s == "printing") return "Printing";
  if (s == "paused") return "Paused";
  if (s == "error") return "Error";
  if (s == "complete") return "Done";
  return "Ready";
}

static void style_state_chip(lv_obj_t *chip, lv_obj_t *label, const std::string &s) {
  lv_color_t c = s == "paused" ? ui::warn() : (s == "error" ? ui::danger() : ui::accent());
  lv_obj_set_style_bg_color(chip, c, 0);
  lv_obj_set_style_bg_opa(chip, 40, 0);
  lv_obj_set_style_border_color(chip, c, 0);
  lv_obj_set_style_border_opa(chip, 90, 0);
  lv_obj_set_style_text_color(label, c, 0);
  lv_label_set_text(label, fmt::format(LV_SYMBOL_BULLET " {}", state_text(s)).c_str());
}

void MainPanel::init(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  for (const auto &el : sensors) {
    auto target_value = j[json::json_pointer(fmt::format("/result/status/{}/target", el.first))];
    if (!target_value.is_null()) {
      int target = target_value.template get<int>();
      el.second->update_target(target);
    }

    auto temp_value = j[json::json_pointer(fmt::format("/result/status/{}/temperature", el.first))];
    if (!temp_value.is_null()) {
      int value = temp_value.template get<int>();
      el.second->update_series(value);
      el.second->update_value(value);
    }
  }

  auto pstate = j["/result/status/print_stats/state"_json_pointer];
  if (!pstate.is_null()) {
    style_state_chip(state_chip, state_label, pstate.template get<std::string>());
  }

  macros_panel.populate();

  auto fans = State::get_instance()->get_display_fans();
  print_status_panel.init(fans);
  printertune_panel.init(j);
}

void MainPanel::consume(json &j) {
  std::lock_guard<std::mutex> lock(lv_lock);
  for (const auto &el : sensors) {
    auto target_value = j[json::json_pointer(fmt::format("/params/0/{}/target", el.first))];
    if (!target_value.is_null()) {
      int target = target_value.template get<int>();
      el.second->update_target(target);
    }

    auto temp_value = j[json::json_pointer(fmt::format("/params/0/{}/temperature", el.first))];
    if (!temp_value.is_null()) {
      int value = temp_value.template get<int>();
      el.second->update_series(value);
      el.second->update_value(value);
    }
  }

  auto pstate = j["/params/0/print_stats/state"_json_pointer];
  if (!pstate.is_null()) {
    style_state_chip(state_chip, state_label, pstate.template get<std::string>());
  }
}

static void scroll_begin_event(lv_event_t * e)
{
  /*Disable the scroll animations. Triggered when a tab button is clicked */
  if (lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
    lv_anim_t * a = (lv_anim_t*)lv_event_get_param(e);
    if(a)  a->time = 0;
  }
}

void MainPanel::create_panel() {
  lv_obj_clear_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(lv_tabview_get_content(tabview), scroll_begin_event, LV_EVENT_SCROLL_BEGIN, NULL);

  // floating glass navigation rail
  lv_obj_set_style_pad_all(tabview, ui::GAP, 0);
  lv_obj_set_style_pad_column(tabview, ui::GAP, 0);

  lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview);
  ui::card(tab_btns);
  lv_obj_set_style_radius(tab_btns, 32, 0);
  lv_obj_set_style_pad_all(tab_btns, 8, 0);
  lv_obj_set_style_pad_row(tab_btns, 8, 0);
  lv_obj_set_style_text_font(tab_btns, &mdi_28, 0);
  lv_obj_set_style_text_font(tab_btns, &mdi_28, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_text_color(tab_btns, ui::text2(), LV_PART_ITEMS);
  lv_obj_set_style_radius(tab_btns, LV_RADIUS_CIRCLE, LV_PART_ITEMS);
  lv_obj_set_style_border_width(tab_btns, 0, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_outline_width(tab_btns, 0, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_PRESSED);
  auto recolor_rail = [tab_btns]() {
    lv_obj_set_style_bg_color(tab_btns, ui::accent(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_btns, ui::on_accent(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tab_btns, ui::accent(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(tab_btns, ui::on_accent(), LV_PART_ITEMS | LV_STATE_PRESSED);
  };
  recolor_rail();
  ui::on_change(recolor_rail);

  lv_obj_set_style_pad_all(main_tab, 0, 0);
  lv_obj_set_style_pad_all(macros_tab, 0, 0);
  lv_obj_set_style_pad_all(console_tab, 0, 0);
  lv_obj_set_style_pad_all(printertune_tab, 0, 0);
  lv_obj_set_style_pad_all(setting_tab, 0, 0);

  create_main(main_tab);
}

void MainPanel::handle_homing_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked homing");
    homing_panel.foreground();
  }
}

void MainPanel::handle_extrude_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked extruder");
    extruder_panel.foreground();
  }
}

void MainPanel::handle_fanpanel_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked fan panel");
    fan_panel.foreground();
  }
}

void MainPanel::handle_ledpanel_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked led panel");
    led_panel.foreground();
  }
}

void MainPanel::handle_cooldown_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked cooldown");
    Config *conf = Config::get_instance();
    auto v = conf->get_json(conf->df() + "default_macros/cooldown");
    if (!v.is_null()) {
      ws.gcode_script(v.template get<std::string>());
    }
  }
}

void MainPanel::handle_print_cb(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    spdlog::trace("clicked print");
    if (print_active) {
      print_status_panel.foreground();
    } else {
      print_panel.foreground();
    }
  }
}

void MainPanel::update_print_state(bool active, int progress, const std::string &status) {
  print_active = active;
  print_btn.set_active(active);
  print_btn.set_progress(active, progress, status == "paused");
}

void MainPanel::update_clock() {
  std::time_t now = std::time(nullptr);
  std::tm tm_now;
  localtime_r(&now, &tm_now);
  char buf[8];
  std::strftime(buf, sizeof(buf), "%H:%M", &tm_now);
  lv_label_set_text(clock_label, buf);
}

void MainPanel::create_main(lv_obj_t * parent)
{
    static lv_coord_t grid_main_row_dsc[] = {36, 104, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), 336, LV_GRID_TEMPLATE_LAST};

    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(main_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    lv_obj_set_style_pad_row(main_cont, ui::GAP, 0);
    lv_obj_set_style_pad_column(main_cont, ui::GAP, 0);
    lv_obj_set_grid_dsc_array(main_cont, grid_main_col_dsc, grid_main_row_dsc);

    // top bar: printer name, state chip, clock
    ui::clear(topbar);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_hor(topbar, 4, 0);
    lv_obj_set_flex_flow(topbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(topbar, 10, 0);
    lv_obj_set_grid_cell(topbar, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 0, 1);

    Config *conf = Config::get_instance();
    auto name = conf->get_json(conf->df() + "display_name");
    title_label = ui::text_label(topbar, name.is_string() ? name.template get<std::string>().c_str() : "K1 Max",
				 &manrope_20, ui::text());

    state_chip = lv_obj_create(topbar);
    ui::pill(state_chip);
    lv_obj_clear_flag(state_chip, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(state_chip, LV_SIZE_CONTENT, 28);
    lv_obj_set_style_border_width(state_chip, 1, 0);
    lv_obj_set_style_pad_hor(state_chip, 12, 0);
    lv_obj_set_style_pad_ver(state_chip, 0, 0);
    state_label = ui::text_label(state_chip, "", &manrope_14, ui::accent());
    lv_obj_center(state_label);
    style_state_chip(state_chip, state_label, "standby");
    ui::on_change([this]() {
      auto &s = State::get_instance()->get_data("/printer_state/print_stats/state"_json_pointer);
      style_state_chip(state_chip, state_label, s.is_string() ? s.template get<std::string>() : "standby");
    });

    lv_obj_t *spacer = lv_obj_create(topbar);
    ui::clear(spacer);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);

    clock_label = ui::text_label(topbar, "--:--", &manrope_20, ui::text());
    update_clock();
    clock_timer = lv_timer_create([](lv_timer_t *t) {
      ((MainPanel *)t->user_data)->update_clock();
    }, 10000, this);

    // temperature cards
    ui::clear(temp_cont);
    lv_obj_clear_flag(temp_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(temp_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(temp_cont, ui::GAP, 0);
    lv_obj_set_grid_cell(temp_cont, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_STRETCH, 1, 1);

    // temperature chart card
    ui::card(chart_card);
    lv_obj_clear_flag(chart_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_top(chart_card, 12, 0);
    lv_obj_set_style_pad_bottom(chart_card, 14, 0);
    lv_obj_set_style_pad_left(chart_card, 46, 0);
    lv_obj_set_style_pad_right(chart_card, 16, 0);
    lv_obj_set_style_pad_row(chart_card, 8, 0);
    lv_obj_set_flex_flow(chart_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_grid_cell(chart_card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

    lv_obj_t *chart_title = ui::text_label(chart_card, "Temperature", &manrope_16, ui::text2());
    lv_obj_set_style_translate_x(chart_title, -30, 0);
    lv_obj_move_to_index(chart_title, 0);

    lv_obj_set_width(temp_chart, LV_PCT(100));
    lv_obj_set_flex_grow(temp_chart, 1);
    lv_obj_set_scrollbar_mode(temp_chart, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(temp_chart, 0, 0);
    lv_obj_set_style_size(temp_chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(temp_chart, 2, LV_PART_ITEMS);
    lv_chart_set_range(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 300);
    lv_chart_set_axis_tick(temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 0, 4, 1, true, 40);
    lv_chart_set_div_line_count(temp_chart, 4, 0);
    lv_chart_set_point_count(temp_chart, 5000);
    lv_chart_set_zoom_x(temp_chart, 5000);
    lv_obj_scroll_to_x(temp_chart, LV_COORD_MAX, LV_ANIM_OFF);

    // action tiles
    static lv_coord_t grid_tiles_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_tiles_row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    ui::clear(tiles_cont);
    lv_obj_clear_flag(tiles_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_row(tiles_cont, ui::GAP, 0);
    lv_obj_set_style_pad_column(tiles_cont, ui::GAP, 0);
    lv_obj_set_grid_dsc_array(tiles_cont, grid_tiles_col_dsc, grid_tiles_row_dsc);
    lv_obj_set_grid_cell(tiles_cont, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 2, 1);

    Tile *tiles[] = {&homing_btn, &extrude_btn, &action_btn, &led_btn, &cooldown_btn, &print_btn};
    for (int i = 0; i < 6; i++) {
      lv_obj_set_grid_cell(tiles[i]->get_container(), LV_GRID_ALIGN_STRETCH, i % 3, 1,
			   LV_GRID_ALIGN_STRETCH, i / 3, 1);
    }
}

void MainPanel::create_sensors(json &temp_sensors) {
  std::lock_guard<std::mutex> lock(lv_lock);
  sensors.clear();
  for (auto &sensor : temp_sensors.items()) {
    std::string key = sensor.key();
    bool controllable = sensor.value()["controllable"].template get<bool>();
    std::string display_name = sensor.value()["display_name"].template get<std::string>();

    const char *icon = ICON_CHAMBER;
    bool use_accent = false;
    lv_color_t color = ui::chamber();
    if (key == "extruder") {
      icon = ICON_NOZZLE_HEAT;
      use_accent = true;
      color = ui::accent();
    } else if (key == "heater_bed") {
      icon = ICON_BED;
      color = ui::bed();
    }

    lv_chart_series_t *temp_series =
      lv_chart_add_series(temp_chart, color, LV_CHART_AXIS_PRIMARY_Y);

    auto card = std::make_shared<TempCard>(ws, temp_cont, icon, display_name.c_str(), use_accent, color,
					   controllable, numpad, key, temp_chart, temp_series);
    lv_obj_set_flex_grow(card->get_container(), 1);
    lv_obj_set_height(card->get_container(), LV_PCT(100));
    sensors.insert({key, card});
  }
}

void MainPanel::create_fans(json &fans) {
  fan_panel.create_fans(fans);
}

void MainPanel::create_leds(json &leds) {
  led_panel.init(leds);
}

void MainPanel::enable_spoolman() {
  spoolman_panel.init();
  setting_panel.enable_spoolman();
  extruder_panel.enable_spoolman();
}
