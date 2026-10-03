#include "setting_panel.h"
#include "config.h"
#include "ui_style.h"
#include "spdlog/spdlog.h"
#include "subprocess.hpp"

#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;
namespace sp = subprocess;

LV_IMG_DECLARE(network_img);
LV_IMG_DECLARE(refresh_img);
LV_IMG_DECLARE(spoolman_img);
LV_IMG_DECLARE(update_img);

#ifdef ZBOLT
LV_IMG_DECLARE(info_img);
#else
LV_IMG_DECLARE(sysinfo_img);
#endif

LV_IMG_DECLARE(print);

SettingPanel::SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm)
  : ws(c)
  , cont(lv_obj_create(parent))
  , appearance(NULL)
  , acrylic_switch(NULL)
#ifndef OS_ANDROID
  , wifi_panel(l)
#endif
  , sysinfo_panel()
  , spoolman_panel(sm)
  , wifi_btn(cont, &network_img, "WIFI", &SettingPanel::_handle_callback, this)
  , restart_klipper_btn(cont, &refresh_img, "Restart Klipper", &SettingPanel::_handle_callback, this)
  , restart_firmware_btn(cont, &refresh_img, "Restart\nFirmware", &SettingPanel::_handle_callback, this)
#ifdef ZBOLT
  , sysinfo_btn(cont, &info_img, "System", &SettingPanel::_handle_callback, this)
#else
  , sysinfo_btn(cont, &sysinfo_img, "System", &SettingPanel::_handle_callback, this)
#endif
  , spoolman_btn(cont, &spoolman_img, "Spoolman", &SettingPanel::_handle_callback, this)
  , guppy_restart_btn(cont, &refresh_img, "Restart Guppy", &SettingPanel::_handle_callback, this)
  , guppy_update_btn(cont, &update_img, "Update Guppy", &SettingPanel::_handle_callback, this)
  , printer_select_btn(cont, &print, "Printers", &SettingPanel::_handle_callback, this)
{
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));

  spoolman_btn.disable();
#ifdef OS_ANDROID
  wifi_btn.disable();
#endif

  static lv_coord_t grid_main_row_dsc[] = {68, LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_style_pad_all(cont, 0, 0);
  lv_obj_set_style_pad_row(cont, ui::GAP, 0);
  lv_obj_set_style_pad_column(cont, ui::GAP, 0);
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
      LV_GRID_TEMPLATE_LAST};

  lv_obj_set_grid_dsc_array(cont, grid_main_col_dsc, grid_main_row_dsc);

  // row 1
  lv_obj_set_grid_cell(wifi_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 1, 1);
  lv_obj_set_grid_cell(restart_klipper_btn.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, 1, 1);
  lv_obj_set_grid_cell(restart_firmware_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_START, 1, 1);
  lv_obj_set_grid_cell(sysinfo_btn.get_container(), LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_START, 1, 1);

  // row 2
  lv_obj_set_grid_cell(spoolman_btn.get_container(), LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 2, 1);
  lv_obj_set_grid_cell(guppy_restart_btn.get_container(), LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, 2, 1);
  lv_obj_set_grid_cell(guppy_update_btn.get_container(), LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_START, 2, 1);
  lv_obj_set_grid_cell(printer_select_btn.get_container(), LV_GRID_ALIGN_CENTER, 3, 1, LV_GRID_ALIGN_START, 2, 1);

  ButtonContainer *tiles[] = {&wifi_btn, &restart_klipper_btn, &restart_firmware_btn, &sysinfo_btn,
                              &spoolman_btn, &guppy_restart_btn, &guppy_update_btn, &printer_select_btn};
  for (int i = 0; i < 8; i++) {
    tiles[i]->make_tile();
    lv_obj_set_grid_cell(tiles[i]->get_container(), LV_GRID_ALIGN_STRETCH, i % 4, 1, LV_GRID_ALIGN_STRETCH, 1 + i / 4, 1);
  }

  create_appearance();
}

SettingPanel::~SettingPanel() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

lv_obj_t *SettingPanel::get_container() {
  return cont;
}

void SettingPanel::handle_callback(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(event);

    if (btn == wifi_btn.get_container()) {
      spdlog::trace("wifi pressed");
#ifndef OS_ANDROID
      wifi_panel.foreground();
#endif
    } else if (btn == sysinfo_btn.get_container()) {
      spdlog::trace("setting system info pressed");
      sysinfo_panel.foreground();
    } else if (btn == restart_klipper_btn.get_container()) {
      spdlog::trace("setting restart klipper pressed");
      ws.send_jsonrpc("printer.restart");
    } else if (btn == restart_firmware_btn.get_container()) {
      spdlog::trace("setting restart klipper pressed");
      ws.send_jsonrpc("printer.firmware_restart");
    } else if (btn == spoolman_btn.get_container()) {
      spdlog::trace("setting spoolman pressed");
      spoolman_panel.foreground();
    } else if (btn == guppy_restart_btn.get_container()) {
      spdlog::trace("restart guppy pressed");
      Config *conf = Config::get_instance();
      auto init_script = conf->get<std::string>("/guppy_init_script");
      const fs::path script(init_script);
      if (fs::exists(script) || init_script.rfind("service guppyscreen", 0) == 0) {
        sp::call({init_script, "restart"});
      } else {
        	spdlog::warn("Failed to restart Guppy Screen. Did not find restart script.");
      }
    } else if (btn == guppy_update_btn.get_container()) {
      spdlog::trace("update guppy pressed");
      // TODO: throw this inside the global threadpool to make it async
      auto update_script = fs::canonical("/proc/self/exe").parent_path() / "update.sh";
      const fs::path script(update_script);
      if (fs::exists(script)) {
	sp::call(script);
      } else {
	spdlog::warn("Failed to update Guppy Screen. Did not find update script.");
      }
    } else if (btn == printer_select_btn.get_container()) {
      spdlog::trace("setting printers pressed");
      printer_select_panel.foreground();
    }
  }
}

void SettingPanel::enable_spoolman() {
  spoolman_btn.enable();
}

// Appearance card: accent color swatches and the acrylic (frosted glass) switch
void SettingPanel::create_appearance() {
  appearance = lv_obj_create(cont);
  ui::card(appearance);
  lv_obj_clear_flag(appearance, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_hor(appearance, 18, 0);
  lv_obj_set_style_pad_ver(appearance, 0, 0);
  lv_obj_set_style_pad_column(appearance, 10, 0);
  lv_obj_set_flex_flow(appearance, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(appearance, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_grid_cell(appearance, LV_GRID_ALIGN_STRETCH, 0, 4, LV_GRID_ALIGN_STRETCH, 0, 1);

  ui::text_label(appearance, "Accent", &manrope_20, ui::text());

  for (size_t i = 0; i < ui::ACCENT_COUNT; i++) {
    lv_obj_t *sw = lv_obj_create(appearance);
    lv_obj_clear_flag(sw, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(sw, 32, 32);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sw, ui::accent_preset(i), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_outline_color(sw, ui::accent_preset(i), 0);
    lv_obj_set_style_outline_pad(sw, 3, 0);
    lv_obj_set_style_outline_width(sw, 0, 0);
    lv_obj_set_ext_click_area(sw, 4);
    lv_obj_add_event_cb(sw, [](lv_event_t *e) {
      SettingPanel *panel = (SettingPanel *)lv_event_get_user_data(e);
      lv_obj_t *target = lv_event_get_current_target(e);
      for (size_t idx = 0; idx < panel->swatches.size(); idx++) {
        if (panel->swatches[idx] == target) {
          ui::set_accent(ui::accent_preset(idx));
        }
      }
      panel->select_swatch();
    }, LV_EVENT_CLICKED, this);
    swatches.push_back(sw);
  }

  lv_obj_t *spacer = lv_obj_create(appearance);
  ui::clear(spacer);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);

  ui::text_label(appearance, "Acrylic", &manrope_20, ui::text());
  acrylic_switch = lv_switch_create(appearance);
  lv_obj_set_size(acrylic_switch, 56, 30);
  if (ui::acrylic()) {
    lv_obj_add_state(acrylic_switch, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(acrylic_switch, [](lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    ui::set_acrylic(lv_obj_has_state(sw, LV_STATE_CHECKED));
  }, LV_EVENT_VALUE_CHANGED, NULL);

  select_swatch();
}

void SettingPanel::select_swatch() {
  uint32_t current = lv_color_to32(ui::accent()) & 0xFFFFFF;
  for (size_t i = 0; i < swatches.size(); i++) {
    bool selected = (lv_color_to32(ui::accent_preset(i)) & 0xFFFFFF) == current;
    lv_obj_set_style_outline_width(swatches[i], selected ? 2 : 0, 0);
  }
}
