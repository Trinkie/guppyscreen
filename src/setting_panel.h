#ifndef __SETTING_PANEL_H__
#define __SETTING_PANEL_H__

#include "platform.h"

#ifndef OS_ANDROID
#include "wifi_panel.h"
#endif

#include "sysinfo_panel.h"
#include "spoolman_panel.h"
#include "printer_select_panel.h"
#include "button_container.h"
#include "websocket_client.h"
#include "lvgl/lvgl.h"

#include <mutex>
#include <vector>

class SettingPanel {
 public:
  SettingPanel(KWebSocketClient &c, std::mutex &l, lv_obj_t *parent, SpoolmanPanel &sm);
  ~SettingPanel();

  lv_obj_t *get_container();
  void enable_spoolman();

  void handle_callback(lv_event_t *event);

  static void _handle_callback(lv_event_t *event) {
    SettingPanel *panel = (SettingPanel*)event->user_data;
    panel->handle_callback(event);
  };

 private:
  void create_appearance();
  void select_swatch();

  KWebSocketClient &ws;
  lv_obj_t *cont;
  lv_obj_t *appearance;
  lv_obj_t *acrylic_switch;
  std::vector<lv_obj_t *> swatches;

#ifndef OS_ANDROID
  WifiPanel wifi_panel;
#endif

  SysInfoPanel sysinfo_panel;
  SpoolmanPanel &spoolman_panel;
  PrinterSelectPanel printer_select_panel;
  ButtonContainer wifi_btn;
  ButtonContainer restart_klipper_btn;
  ButtonContainer restart_firmware_btn;
  ButtonContainer sysinfo_btn;
  ButtonContainer spoolman_btn;
  ButtonContainer guppy_restart_btn;
  ButtonContainer guppy_update_btn;
  ButtonContainer printer_select_btn;
  
};

#endif // __SETTING_PANEL_H__

