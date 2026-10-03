#ifndef __BUTTON_CONTAINER_H__
#define __BUTTON_CONTAINER_H__

#include "lvgl/lvgl.h"

#include <string>
#include <functional>

class ButtonContainer {
 public:
  ButtonContainer(lv_obj_t *parent,
		  const void *btn_img,
		  const char *text,
		  lv_event_cb_t cb,
		  void *user_data,
		  const std::string &prompt_text = {},
		  const std::function<void()> &prompt_callback = {});
  ~ButtonContainer();

  lv_obj_t *get_container();
  lv_obj_t *get_button();
  void disable();
  void enable();
  void hide();

  void set_image(const void *img);

  enum PillVariant { PILL_GLASS, PILL_ACCENT, PILL_DANGER_OUTLINE, PILL_DANGER_FILL };
  // turns the image tile into a horizontal capsule with an icon font glyph;
  // an empty text makes a round icon-only button
  // icon may be NULL (text only); icon_only hides the text and makes a round button
  void make_pill(const char *icon, PillVariant variant, bool icon_only = false);
  // fills its grid cell as a glass tile with the image and label centered
  void make_tile();
  // changes the icon glyph of a pill button
  void set_icon(const char *icon);

  void handle_callback(lv_event_t *event);

  void handle_prompt();
  void run_callback();
  
  static void _handle_callback(lv_event_t *event) {
    ButtonContainer *button_container = (ButtonContainer*)event->user_data;
    button_container->handle_callback(event);
  };

 private:
  lv_obj_t *btn_cont;
  lv_obj_t *btn;
  lv_obj_t *label;
  lv_obj_t *pill_icon;
  std::string prompt_text;
  std::function<void()> prompt_callback;
};

#endif // __BUTTON_CONTAINER_H__
