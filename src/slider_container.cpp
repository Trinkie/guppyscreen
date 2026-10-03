#include "slider_container.h"
#include "spdlog/spdlog.h"
#include "ui_style.h"

SliderContainer::SliderContainer(lv_obj_t *parent,
				 const char *label_text,
				 const void *off_btn_img,
				 const char *off_text,
				 const void *max_btn_img,
				 const char *max_text,
				 lv_event_cb_t cb,
				 void *user_data)
  : SliderContainer(parent,
		    label_text,
		    off_btn_img,
		    off_text,
		    cb,
		    user_data,
		    max_btn_img,
		    max_text,
		    cb,
		    user_data,
		    cb,
		    user_data,
		    "%") {
}

SliderContainer::SliderContainer(lv_obj_t *parent,
				 const char *label_text,
				 const void *off_btn_img,
				 const char *off_text,
				 const void *max_btn_img,
				 const char *max_text,
				 lv_event_cb_t cb,
				 void *user_data,
				 std::string u)
  : SliderContainer(parent,
		    label_text,
		    off_btn_img,
		    off_text,
		    cb,
		    user_data,
		    max_btn_img,
		    max_text,
		    cb,
		    user_data,
		    cb,
		    user_data,
		    u) {
}


SliderContainer::SliderContainer(lv_obj_t *parent,
				 const char *label_text,
				 const void *off_btn_img,
				 const char *off_text,
				 lv_event_cb_t off_cb,
				 void * off_cb_user_data,
				 const void *max_btn_img,
				 const char *max_text,
				 lv_event_cb_t max_cb,
				 void * max_cb_user_data,
				 lv_event_cb_t slider_cb,
				 void * slider_user_data,
				 std::string u)
  : cont(lv_obj_create(parent))
  , label(lv_label_create(cont))
  , control_cont(lv_obj_create(cont))
  , off_btn(control_cont, off_btn_img, off_text, off_cb, off_cb_user_data)
  , slider_cont(lv_obj_create(control_cont))
  , slider(lv_slider_create(slider_cont))
  , slider_value(lv_label_create(slider_cont))
  , max_btn(control_cont, max_btn_img, max_text, max_cb, max_cb_user_data)
  , unit(u)
{
  // glass card: [name ........ value] / [Off] [=====slider=====] [Max]
  ui::card(cont);
  lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_hor(cont, 16, 0);
  lv_obj_set_style_pad_ver(cont, 12, 0);
  lv_obj_set_style_pad_row(cont, 8, 0);
  lv_obj_set_size(cont, lv_pct(100), LV_SIZE_CONTENT);

  lv_obj_t *head = lv_obj_create(cont);
  ui::clear(head);
  lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_move_to_index(head, 0);

  lv_obj_set_parent(label, head);
  lv_label_set_text(label, label_text);
  lv_obj_set_style_text_font(label, &manrope_16, 0);
  lv_obj_set_style_text_color(label, ui::text2(), 0);
  lv_obj_set_style_pad_bottom(label, 4, 0);

  lv_obj_set_parent(slider_value, head);
  lv_label_set_text(slider_value, fmt::format("0{}", unit).c_str());
  lv_obj_set_style_text_font(slider_value, &manrope_28, 0);
  lv_obj_set_style_text_color(slider_value, ui::text(), 0);

  ui::clear(control_cont);
  lv_obj_clear_flag(control_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(control_cont, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(control_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(control_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(control_cont, 12, 0);

  ui::clear(slider_cont);
  lv_obj_clear_flag(slider_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_height(slider_cont, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(slider_cont, 1);
  lv_obj_set_style_pad_ver(slider_cont, 6, 0);

  lv_obj_set_width(slider, LV_PCT(100));
  lv_obj_center(slider);
  ui::slider_big(slider);

  off_btn.make_pill(NULL, ButtonContainer::PILL_GLASS);
  max_btn.make_pill(NULL, ButtonContainer::PILL_GLASS);
  lv_obj_set_height(off_btn.get_container(), 44);
  lv_obj_set_height(max_btn.get_container(), 44);
  lv_obj_set_width(off_btn.get_container(), 84);
  lv_obj_set_width(max_btn.get_container(), 84);
  lv_obj_set_style_pad_hor(off_btn.get_container(), 0, 0);
  lv_obj_set_style_pad_hor(max_btn.get_container(), 0, 0);

  if (max_text == NULL) {
    max_btn.hide();
  }

  if (slider_cb != NULL) {
    lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_RELEASED, slider_user_data);
  }

  lv_obj_add_event_cb(slider, &SliderContainer::_handle_value_update,
		      LV_EVENT_VALUE_CHANGED, this);
}

SliderContainer::~SliderContainer() {
  if (cont != NULL) {
    lv_obj_del(cont);
    cont = NULL;
  }
}

lv_obj_t *SliderContainer::get_container() {
  return cont;
}

lv_obj_t *SliderContainer::get_slider() {
  return slider;
}

lv_obj_t *SliderContainer::get_off() {
  return off_btn.get_container();
}

lv_obj_t *SliderContainer::get_max() {
  return max_btn.get_container();
}

void SliderContainer::set_range(int min_range, int max_range) {
  lv_slider_set_range(slider, min_range, max_range);
}

void SliderContainer::update_value(int value) {
  lv_label_set_text(slider_value, fmt::format("{}{}", value, unit).c_str());
  lv_slider_set_value(slider, value, LV_ANIM_ON);
}

void SliderContainer::handle_value_update(lv_event_t *event) {
  lv_obj_t * obj = lv_event_get_target(event);
  update_value((int)lv_slider_get_value(obj));
}
