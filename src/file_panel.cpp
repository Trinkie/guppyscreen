#include "file_panel.h"
#include "config.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"
#include "ui_style.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

#define THUMBSCALE = 0.78

FilePanel::FilePanel(lv_obj_t *parent)
  : file_cont(lv_obj_create(parent))
  , thumbnail(lv_img_create(file_cont))
  , fname_label(lv_label_create(file_cont))
  , detail_label(lv_label_create(file_cont))
{
  // thumbnail, file name and details stacked in the middle of the card
  lv_obj_set_size(file_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(file_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(file_cont, 0, 0);
  lv_obj_set_style_pad_row(file_cont, 8, 0);
  lv_obj_set_flex_flow(file_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(file_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_img_set_size_mode(thumbnail, LV_IMG_SIZE_MODE_REAL);

  lv_obj_set_width(fname_label, LV_PCT(100));
  lv_label_set_long_mode(fname_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_align(fname_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(fname_label, &manrope_20, 0);
  lv_obj_set_style_text_color(fname_label, ui::text(), 0);

  lv_obj_set_width(detail_label, LV_PCT(100));
  lv_obj_set_style_text_align(detail_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(detail_label, &manrope_16, 0);
  lv_obj_set_style_text_color(detail_label, ui::text2(), 0);
  lv_obj_set_style_text_line_space(detail_label, 4, 0);
}

FilePanel::~FilePanel() {
  if (file_cont != NULL) {
    lv_obj_del(file_cont);
    file_cont = NULL;
  }
}

void FilePanel::refresh_view(json &j, const std::string &gcode_path) {
  auto v = j["/result/modified"_json_pointer];
  std::stringstream time_stream;  
  if (!v.is_null()) {
    std::time_t timestamp = v.template get<std::time_t>();
    std::tm lt = *std::localtime(&timestamp);
    time_stream << std::put_time(&lt, "%Y-%m-%d %H:%M");
  } else {
    time_stream << "(unknown)";
  }
  
  v = j["/result/estimated_time"_json_pointer];
  int eta =  v.is_null() ? -1 : v.template get<int>();
  v = j["/result/filament_weight_total"_json_pointer];
  int fweight = v.is_null() ? -1 : v.template get<int>();

  auto filename = fs::path(gcode_path).filename();
  lv_label_set_text(fname_label, filename.string().c_str());
  
  std::string detail = fmt::format("{}  " LV_SYMBOL_BULLET "  {} g\n{} MB  " LV_SYMBOL_BULLET "  {}",
				   eta > 0 ? KUtils::eta_string(eta) : "--",
				   fweight > 0 ? std::to_string(fweight) : "--",
				   KUtils::bytes_to_mb(j["result"]["size"].template get<size_t>()),
				   time_stream.str());
  lv_label_set_text(detail_label, detail.c_str());

  auto width_scale = (double)lv_disp_get_physical_hor_res(NULL) / 800.0;
  auto thumb_detail = KUtils::get_thumbnail(gcode_path, j, width_scale);
  std::string fullpath = thumb_detail.first;
  if (fullpath.length() > 0) {
    auto screen_width = lv_disp_get_physical_hor_res(NULL);
    uint32_t normalized_thumb_scale = ((0.21 * (double)screen_width) / (double)thumb_detail.second) * 256;
    lv_img_set_src(thumbnail, ("A:" + fullpath).c_str());
    lv_img_set_zoom(thumbnail, normalized_thumb_scale);
  } else {
    // free src
    lv_img_set_src(thumbnail, NULL);
    // hack to color in empty space.
    ((lv_img_t*)thumbnail)->src_type = LV_IMG_SRC_SYMBOL;
  }
}

void FilePanel::foreground() {
  lv_obj_move_foreground(file_cont);
}

lv_obj_t *FilePanel::get_container() {
  return file_cont;
}

const char* FilePanel::get_thumbnail_path() {
  return (const char*)lv_img_get_src(thumbnail);
}
