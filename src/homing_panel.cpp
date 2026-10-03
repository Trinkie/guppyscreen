#include "homing_panel.h"
#include "state.h"
#include "spdlog/spdlog.h"
#include "config.h"
#include "ui_style.h"

static const float distances[] = {0.1, 0.5, 1, 5, 10, 25, 50};

LV_IMG_DECLARE(arrow_left);
LV_IMG_DECLARE(arrow_up);
LV_IMG_DECLARE(arrow_right);
LV_IMG_DECLARE(arrow_down);
LV_IMG_DECLARE(home);
LV_IMG_DECLARE(back);
LV_IMG_DECLARE(z_closer);
LV_IMG_DECLARE(z_farther);
LV_IMG_DECLARE(emergency);
LV_IMG_DECLARE(motor_off_img);

HomingPanel::HomingPanel(KWebSocketClient &websocket_client, std::mutex &lock)
  : NotifyConsumer(lock)
  , ws(websocket_client)
  , homing_cont(lv_obj_create(lv_scr_act()))
  , home_all_btn(homing_cont, &home, "Home All", &HomingPanel::_handle_callback, this)
  , home_xy_btn(homing_cont, &home, "Home XY", &HomingPanel::_handle_callback, this)
  , y_up_btn(homing_cont, &arrow_up, "Y+", &HomingPanel::_handle_callback, this)
  , y_down_btn(homing_cont, &arrow_down, "Y-", &HomingPanel::_handle_callback, this)    
  , x_up_btn(homing_cont, &arrow_right, "X+", &HomingPanel::_handle_callback, this)
  , x_down_btn(homing_cont, &arrow_left, "X-", &HomingPanel::_handle_callback, this)
  , z_up_btn(homing_cont, &z_closer, "Z+", &HomingPanel::_handle_callback, this)
  , z_down_btn(homing_cont, &z_farther, "Z-", &HomingPanel::_handle_callback, this)
  , emergency_btn(homing_cont, &emergency, "Stop", &HomingPanel::_handle_callback, this,
		  "Do you want to emergency stop?",
		  [&websocket_client]() {
		    spdlog::debug("emergency stop pressed");
		    websocket_client.send_jsonrpc("printer.emergency_stop");
		  })
  , motoroff_btn(homing_cont, &motor_off_img, "Motor Off", &HomingPanel::_handle_callback, this)
  , back_btn(homing_cont, &back, "Back", &HomingPanel::_handle_callback, this)
  , distance_selector(homing_cont, "Move Distance (mm)",
		     {".1", ".5", "1", "5", "10", "25", "50", ""}, 2, 70, 15, &HomingPanel::_handle_selector_cb, this)
{
  lv_obj_clear_flag(homing_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_height(homing_cont, lv_pct(100));
  lv_obj_set_width(homing_cont, lv_pct(100));

  back_btn.make_pill(ICON_BACK, ButtonContainer::PILL_GLASS, true);
  ui::panel_header(homing_cont, back_btn.get_container(), "Move");

  // [ XY pad | Z | actions ]
  // [ distance selector    ]
  static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(5), LV_GRID_FR(2), LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(homing_cont, grid_main_col_dsc, grid_main_row_dsc);
  lv_obj_set_style_pad_row(homing_cont, ui::GAP, 0);
  lv_obj_set_style_pad_column(homing_cont, ui::GAP, 0);

  auto make_card = [this](int col) {
    lv_obj_t *c = lv_obj_create(homing_cont);
    ui::card(c);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(c, 12, 0);
    lv_obj_set_grid_cell(c, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    return c;
  };

  auto round_btn = [](ButtonContainer &b, lv_obj_t *parent, const char *icon, ButtonContainer::PillVariant v) {
    lv_obj_set_parent(b.get_container(), parent);
    b.make_pill(icon, v, true);
    lv_obj_set_size(b.get_container(), 72, 72);
    lv_obj_set_style_text_font(b.get_container(), &mdi_28, 0);
  };

  // XY pad
  lv_obj_t *xy = make_card(0);
  static lv_coord_t grid_xy_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(xy, grid_xy_dsc, grid_xy_dsc);
  round_btn(y_up_btn, xy, ICON_ARROW_UP, ButtonContainer::PILL_GLASS);
  round_btn(x_down_btn, xy, ICON_ARROW_LEFT, ButtonContainer::PILL_GLASS);
  round_btn(home_xy_btn, xy, ICON_HOME, ButtonContainer::PILL_ACCENT);
  round_btn(x_up_btn, xy, ICON_ARROW_RIGHT, ButtonContainer::PILL_GLASS);
  round_btn(y_down_btn, xy, ICON_ARROW_DOWN, ButtonContainer::PILL_GLASS);
  lv_obj_set_grid_cell(y_up_btn.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_set_grid_cell(x_down_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  lv_obj_set_grid_cell(home_xy_btn.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  lv_obj_set_grid_cell(x_up_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  lv_obj_set_grid_cell(y_down_btn.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);
  lv_obj_t *xy_caption = ui::text_label(xy, "XY", &manrope_16, ui::text3());
  lv_obj_set_grid_cell(xy_caption, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_START, 0, 1);

  // Z column
  lv_obj_t *z = make_card(1);
  lv_obj_set_flex_flow(z, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(z, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  round_btn(z_up_btn, z, ICON_ARROW_UP, ButtonContainer::PILL_GLASS);
  ui::text_label(z, "Z", &manrope_20, ui::text2());
  round_btn(z_down_btn, z, ICON_ARROW_DOWN, ButtonContainer::PILL_GLASS);
  lv_obj_move_to_index(z_up_btn.get_container(), 0);

  // actions
  lv_obj_t *actions = make_card(2);
  lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_parent(home_all_btn.get_container(), actions);
  lv_obj_set_parent(motoroff_btn.get_container(), actions);
  lv_obj_set_parent(emergency_btn.get_container(), actions);
  home_all_btn.make_pill(ICON_HOME_AXES, ButtonContainer::PILL_ACCENT);
  motoroff_btn.make_pill(ICON_MOTOR_OFF, ButtonContainer::PILL_GLASS);
  emergency_btn.make_pill(ICON_ESTOP, ButtonContainer::PILL_DANGER_FILL);
  for (auto *b : {&home_all_btn, &motoroff_btn, &emergency_btn}) {
    lv_obj_set_width(b->get_container(), LV_PCT(100));
  }

  lv_obj_set_grid_cell(distance_selector.get_container(), LV_GRID_ALIGN_STRETCH, 0, 3, LV_GRID_ALIGN_CENTER, 1, 1);

  ws.register_notify_update(this);
}

HomingPanel::~HomingPanel() {
}

void HomingPanel::consume(json &j) {
  auto v = j["/params/0/toolhead/homed_axes"_json_pointer];
  if (!v.is_null()) {
    std::string homed_axes = v.template get<std::string>();
    std::lock_guard<std::mutex> lock(lv_lock);
    if (homed_axes.find("x") != std::string::npos) {
      x_up_btn.enable();
      x_down_btn.enable();
    } else {
      x_up_btn.disable();
      x_down_btn.disable();
    }

    if (homed_axes.find("y") != std::string::npos) {
      y_up_btn.enable();
      y_down_btn.enable();
    } else {
      y_up_btn.disable();
      y_down_btn.disable();
    }

    if (homed_axes.find("z") != std::string::npos) {
      z_up_btn.enable();
      z_down_btn.enable();
    } else {
      z_up_btn.disable();
      z_down_btn.disable();
    }
  }
}

lv_obj_t *HomingPanel::get_container() {
  return homing_cont;
}

void HomingPanel::foreground() {
  auto v = State::get_instance()
    ->get_data("/printer_state/toolhead/homed_axes"_json_pointer);
  if (!v.is_null()) {
    std::string homed_axes = v.template get<std::string>();
    if (homed_axes.find("x") != std::string::npos) {
      x_up_btn.enable();
      x_down_btn.enable();
    } else {
      x_up_btn.disable();
      x_down_btn.disable();
    }

    if (homed_axes.find("y") != std::string::npos) {
      y_up_btn.enable();
      y_down_btn.enable();
    } else {
      y_up_btn.disable();
      y_down_btn.disable();
    }

    //Set the Z axis buttons
    z_up_btn.set_image(&z_farther);
    z_down_btn.set_image(&z_closer);

    if (homed_axes.find("z") != std::string::npos) {
      z_up_btn.enable();
      z_down_btn.enable();
    } else {
      z_up_btn.disable();
      z_down_btn.disable();
    }
  }

  //Set the Z axis buttons

  v = Config::get_instance()->get_json("/invert_z_icon");
  bool inverted = !v.is_null() && v.template get<bool>();
  if (inverted) {
    // UP arrow
    z_up_btn.set_image(&z_farther);
    z_down_btn.set_image(&z_closer);
  } else {
    // DOWN arrow
    z_up_btn.set_image(&z_closer);
    z_down_btn.set_image(&z_farther);
  }

  lv_obj_move_foreground(homing_cont);
}

void HomingPanel::handle_callback(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);  
  const char * distance = lv_btnmatrix_get_btn_text(distance_selector.get_selector(),
						    distance_selector.get_selected_idx());
  std::string move_op;

  if (btn == home_all_btn.get_container()) {
    spdlog::debug("home all pressed");
    ws.gcode_script("G28 X Y Z");

  }
  else if (btn == home_xy_btn.get_container()) {
    spdlog::debug("home xy pressed");
    ws.gcode_script("G28 X Y");

  }
  else if (btn == y_up_btn.get_container()) {
    spdlog::debug("y up pressed");
    move_op = fmt::format("G0 Y+{} F1200", distance);

  }
  else if (btn == y_down_btn.get_container()) {
    spdlog::debug("y down pressed");
    move_op = fmt::format("G0 Y-{} F1200", distance);

  }
  else if (btn == x_up_btn.get_container()) {
    spdlog::debug("x up pressed");
    move_op = fmt::format("G0 X+{} F1200", distance);

  }
  else if (btn == x_down_btn.get_container()) {
    spdlog::debug("x down pressed");
    move_op = fmt::format("G0 X-{} F1200", distance);

  }
  else if (btn == z_up_btn.get_container()) {
    spdlog::debug("z up pressed");
    move_op = fmt::format("G0 Z+{} F1200", distance);

  }
  else if (btn == z_down_btn.get_container()) {
    spdlog::debug("z down pressed");
    move_op = fmt::format("G0 Z-{} F1200", distance);

  }
  else if (btn == emergency_btn.get_container()) {
    spdlog::debug("emergency stop pressed");
    ws.send_jsonrpc("printer.emergency_stop");

  }
  else if (btn == motoroff_btn.get_container()) {
    spdlog::debug("motor off pressed");
    ws.gcode_script("M84");

  } else if (btn == back_btn.get_container()) {
    lv_obj_move_background(homing_cont);
  }
  else {
    spdlog::debug("Unknown action button pressed");
  }

  if (move_op.size() > 0) {
    // ws.gcode_script("G91");
    ws.gcode_script(fmt::format("G91\n{}", move_op));
  }
}

void HomingPanel::handle_selector_cb(lv_event_t *event) {
  lv_obj_t * obj = lv_event_get_target(event);
  uint32_t idx = lv_btnmatrix_get_selected_btn(obj);
  distance_selector.set_selected_idx(idx);
  spdlog::debug("selector move distance index {}", idx);
}
