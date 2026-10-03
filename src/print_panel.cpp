#include "print_panel.h"
#include "file_panel.h"
#include "state.h"
#include "utils.h"
#include "spdlog/spdlog.h"
#include "ui_style.h"

#include <map>
#include <sstream>

LV_IMG_DECLARE(info_img);
LV_IMG_DECLARE(print);
LV_IMG_DECLARE(back);

#define SORTED_BY_NAME 1 << 0
#define SORTED_BY_MODIFIED  1 << 1

PrintPanel::PrintPanel(KWebSocketClient &websocket, std::mutex &lock, PrintStatusPanel &ps)
  : NotifyConsumer(lock)
  , ws(websocket)
  , files_cont(lv_obj_create(lv_scr_act()))
  , prompt_cont(lv_obj_create(lv_scr_act()))
  , msgbox(lv_obj_create(prompt_cont))
  , job_btn(lv_btn_create(msgbox))
  , cancel_btn(lv_btn_create(msgbox))
  , queue_btn(lv_btn_create(msgbox))
  , left_cont(lv_obj_create(files_cont))
  , file_table_btns(lv_obj_create(left_cont))
  , refresh_btn(lv_btn_create(file_table_btns))
  , modified_sort_btn(lv_btn_create(file_table_btns))
  , az_sort_btn(lv_btn_create(file_table_btns))
  , file_table(lv_table_create(left_cont))
  , file_view(lv_obj_create(files_cont))
  , status_btn(file_view, &info_img, "Status", &PrintPanel::_handle_status_btn, this)
  , print_btn(file_view, &print, "Print", &PrintPanel::_handle_print_callback, this)
  , back_btn(file_view, &back, "Back", &PrintPanel::_handle_back_btn, this)
  , root("", "", 0)
  , cur_dir(&root)
  , cur_file(NULL)
  , file_panel(file_view)
  , print_status(ps)
  , sorted_by(SORTED_BY_MODIFIED)
{
  spdlog::trace("building print panel");
  lv_obj_move_background(files_cont);

  lv_obj_set_size(files_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(files_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(files_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(files_cont, ui::GAP, 0);

  // header: back, title, sort/reload pills on the right
  lv_obj_set_parent(back_btn.get_container(), files_cont);
  back_btn.make_pill(ICON_BACK, ButtonContainer::PILL_GLASS, true);
  ui::panel_header(files_cont, back_btn.get_container(), "Files");

  ui::clear(file_table_btns);
  lv_obj_set_parent(file_table_btns, files_cont);
  lv_obj_add_flag(file_table_btns, LV_OBJ_FLAG_FLOATING);
  lv_obj_clear_flag(file_table_btns, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(file_table_btns, LV_SIZE_CONTENT, 48);
  lv_obj_set_flex_flow(file_table_btns, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(file_table_btns, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(file_table_btns, 8, 0);
  lv_obj_align(file_table_btns, LV_ALIGN_TOP_RIGHT, 0, -(56 + ui::GAP) + 4);

  struct { lv_obj_t *btn; const char *txt; } sort_btns[] = {
    {refresh_btn, ICON_REFRESH},
    {modified_sort_btn, ICON_SORT_TIME " Recent"},
    {az_sort_btn, ICON_SORT_AZ " A-Z"},
  };
  for (auto &sb : sort_btns) {
    lv_obj_set_height(sb.btn, 48);
    lv_obj_set_style_pad_hor(sb.btn, 16, 0);
    lv_obj_t *l = lv_label_create(sb.btn);
    lv_label_set_text(l, sb.txt);
    lv_obj_set_style_text_font(l, &manrope_16, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(sb.btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  }
  // icons in the sort pills come from the icon font, the words from manrope
  lv_obj_t *reload_label = lv_obj_get_child(refresh_btn, 0);
  lv_obj_set_style_text_font(reload_label, &mdi_20, 0);
  lv_obj_set_width(refresh_btn, 48);
  lv_obj_set_style_pad_hor(refresh_btn, 0, 0);
  for (lv_obj_t *sb : {modified_sort_btn, az_sort_btn}) {
    lv_obj_t *l = lv_obj_get_child(sb, 0);
    lv_label_set_text(l, sb == modified_sort_btn ? "Recent" : "A-Z");
  }

  // left: file list card
  ui::card(left_cont);
  lv_obj_set_size(left_cont, LV_PCT(58), LV_PCT(100));
  lv_obj_clear_flag(left_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(left_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(left_cont, 6, 0);
  lv_obj_set_style_clip_corner(left_cont, true, 0);

  lv_obj_set_size(file_table, LV_PCT(100), LV_PCT(100));
  lv_table_set_col_cnt(file_table, 1);
  lv_table_set_col_width(file_table, 0, 436);
  lv_obj_set_style_bg_opa(file_table, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(file_table, 0, 0);
  lv_obj_set_style_pad_all(file_table, 0, 0);
  lv_obj_set_style_bg_opa(file_table, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_border_width(file_table, 1, LV_PART_ITEMS);
  lv_obj_set_style_border_side(file_table, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
  lv_obj_set_style_border_color(file_table, lv_color_white(), LV_PART_ITEMS);
  lv_obj_set_style_border_opa(file_table, 18, LV_PART_ITEMS);
  lv_obj_set_style_pad_ver(file_table, 14, LV_PART_ITEMS);
  lv_obj_set_style_pad_hor(file_table, 14, LV_PART_ITEMS);
  lv_obj_set_style_text_font(file_table, &manrope_16, LV_PART_ITEMS);
  lv_obj_set_style_text_color(file_table, ui::text(), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(file_table, lv_color_white(), LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(file_table, 30, LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_add_event_cb(file_table, &PrintPanel::_handle_callback, LV_EVENT_ALL, this);
  lv_obj_set_scroll_dir(file_table, LV_DIR_TOP | LV_DIR_BOTTOM);

  // right: selected file card with actions
  ui::card(file_view);
  lv_obj_set_height(file_view, LV_PCT(100));
  lv_obj_set_flex_grow(file_view, 1);
  lv_obj_clear_flag(file_view, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(file_view, 14, 0);
  lv_obj_set_style_pad_row(file_view, 12, 0);
  lv_obj_set_flex_flow(file_view, LV_FLEX_FLOW_COLUMN);

  lv_obj_set_width(file_panel.get_container(), LV_PCT(100));
  lv_obj_set_flex_grow(file_panel.get_container(), 1);

  lv_obj_t *actions = lv_obj_create(file_view);
  ui::clear(actions);
  lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(actions, LV_PCT(100), 56);
  lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(actions, 10, 0);
  lv_obj_set_parent(status_btn.get_container(), actions);
  lv_obj_set_parent(print_btn.get_container(), actions);
  status_btn.make_pill(ICON_INFO, ButtonContainer::PILL_GLASS, true);
  print_btn.make_pill(ICON_PRINTER, ButtonContainer::PILL_ACCENT);
  lv_obj_set_flex_grow(print_btn.get_container(), 1);

  // "printing in progress" prompt
  lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);  
  lv_obj_set_size(prompt_cont, LV_PCT(100), LV_PCT(100));
  lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_img_src(prompt_cont, NULL, 0);
  lv_obj_set_style_bg_color(prompt_cont, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(prompt_cont, LV_OPA_60, 0);

  ui::card_solid(msgbox);
  lv_obj_set_size(msgbox, 460, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(msgbox, 22, 0);
  lv_obj_set_style_pad_row(msgbox, 18, 0);
  lv_obj_set_style_pad_column(msgbox, 10, 0);
  lv_obj_set_flex_flow(msgbox, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(msgbox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_center(msgbox);

  lv_obj_t *msg = ui::text_label(msgbox, "A print is already running", &manrope_20, ui::text());
  lv_obj_set_width(msg, LV_PCT(100));
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_move_to_index(msg, 0);

  struct { lv_obj_t *btn; const char *txt; } prompt_btns[] = {
    {queue_btn, "Queue Job"}, {job_btn, "View Job"}, {cancel_btn, "Close"},
  };
  for (auto &pb : prompt_btns) {
    lv_obj_set_height(pb.btn, 48);
    lv_obj_set_style_pad_hor(pb.btn, 18, 0);
    lv_obj_t *l = lv_label_create(pb.btn);
    lv_label_set_text(l, pb.txt);
    lv_obj_set_style_text_font(l, &manrope_16, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(pb.btn, &PrintPanel::_handle_btns, LV_EVENT_CLICKED, this);
  }

  ws.register_notify_update(this);
}

PrintPanel::~PrintPanel() {
  if (files_cont != NULL) {
    lv_obj_del(files_cont);
    files_cont = NULL;
  }

  if (prompt_cont != NULL) {
    lv_obj_del(prompt_cont);
    prompt_cont = NULL;
  }
}

void PrintPanel::populate_files(json &j) {
  sorted_by = SORTED_BY_MODIFIED;
  show_dir(cur_dir, SORTED_BY_MODIFIED);
}

void PrintPanel::consume(json &j) {  
  json &pstat_state = j["/params/0/print_stats/state"_json_pointer];
  if (pstat_state.is_null()) {
    return;
  }
  
  std::lock_guard<std::mutex> lock(lv_lock);
  if(pstat_state.template get<std::string>() != "printing"
     && pstat_state.template get<std::string>() != "paused") {
    status_btn.disable();
  } else {
    status_btn.enable();
  }
}

void PrintPanel::subscribe() {
  ws.send_jsonrpc("server.files.list", R"({"root":"gcodes"})"_json, [this](json &d) {
    std::lock_guard<std::mutex> lock(lv_lock);
    std::string cur_path = cur_dir->full_path;
    root.clear();
    cur_file = NULL;
    cur_dir = NULL;

    if (d.contains("result")) {
      for (auto f : d["result"]) {
        root.add_path(KUtils::split(f["path"], '/'), f["path"], f["modified"].template get<uint32_t>());
      }
    }
    Tree *dir = root.find_path(KUtils::split(cur_path, '/'));
    // need to simply this using the directory endpoint
    cur_dir = dir;
    this->populate_files(d);
  });
}

void PrintPanel::foreground() {
  json &pstat_state = State::get_instance()
    ->get_data("/printer_state/print_stats/state"_json_pointer);
  spdlog::debug("print panel print stats {}",
		pstat_state.is_null() ? "nil" : pstat_state.template get<std::string>());
    
  if (!pstat_state.is_null()
      && pstat_state.template get<std::string>() != "printing"
      && pstat_state.template get<std::string>() != "paused") {
    status_btn.disable();
  } else {
    status_btn.enable();
  }
  
  lv_obj_move_foreground(files_cont);
}

void PrintPanel::handle_callback(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);

  if(code == LV_EVENT_VALUE_CHANGED) {
    const char * str_fn = NULL;
    uint16_t row;
    uint16_t col;

    lv_table_get_selected_cell(file_table, &row, &col);
    uint16_t row_count = lv_table_get_row_cnt(file_table);
    if (row == LV_TABLE_CELL_NONE || col == LV_TABLE_CELL_NONE || row >= row_count) {
      return;
    }

    str_fn = lv_table_get_cell_value(file_table, row, col);
    
    const char *filename = str_fn+5; // +5 skips the LV_SYMBOL and spaces
    if (std::memcmp(LV_SYMBOL_DIRECTORY, str_fn, 3) == 0) {
      if ((strcmp(filename, "..") == 0)) {
	if (cur_dir->parent != cur_dir) {
	  cur_dir = cur_dir->parent;
	  show_dir(cur_dir, sorted_by);
	}
      } else {
	Tree *dir = cur_dir->get_child(filename);
	if (dir != NULL) {
	  cur_dir = dir;
	  show_dir(cur_dir, sorted_by);
	}
      }
    }
    else {
      if (cur_file != cur_dir->get_child(filename)) {
	cur_file = cur_dir->get_child(filename);
	show_file_detail(cur_file);
      }
    }
  }
}

void PrintPanel::show_dir(Tree *dir, uint32_t sort_type) {
  uint32_t index = 0;
  lv_table_set_cell_value_fmt(file_table, index++, 0, LV_SYMBOL_DIRECTORY "  %s", "..");

  bool reversed = sorted_by & sort_type;
  std::vector<Tree> sorted_files;
  if (sort_type == SORTED_BY_MODIFIED) {
    KUtils::sort_map_values<std::string, Tree>(dir->children, sorted_files, [reversed](Tree &x, Tree &y) {
	if (x.is_leaf() && !y.is_leaf()) {
	  return false;
	} else if (!x.is_leaf() && y.is_leaf()) {
	  return true;
	}

	return reversed ? x.date_modified > y.date_modified : y.date_modified > x.date_modified;
      });
  } else {
    KUtils::sort_map_values<std::string, Tree>(dir->children, sorted_files, [reversed](Tree &x, Tree &y) {
	if (x.is_leaf() && !y.is_leaf()) {
	  return false;
	} else if (!x.is_leaf() && y.is_leaf()) {
	  return true;
	}

	return reversed ? x.name > y.name : y.name > x.name;
      });
  }
      
  sorted_by = (sorted_by ^ sort_type) & sort_type;
  for (const auto &c : sorted_files) {
    if (c.is_leaf()) {
      lv_table_set_cell_value_fmt(file_table, index, 0, LV_SYMBOL_FILE "  %s", c.name.c_str());
    } else {
      lv_table_set_cell_value_fmt(file_table, index, 0, LV_SYMBOL_DIRECTORY "  %s", c.name.c_str());
    }
    index++;
  }

  lv_table_set_row_cnt(file_table, index);
  lv_obj_scroll_to_y(file_table, 0, LV_ANIM_OFF);

  // XXX: maybe use the directory instead of file endpoint in moonraker
  for (auto &c : sorted_files) {
    if (c.is_leaf()) {
      const auto &selected = dir->children.find(c.name);
      if (selected != dir->children.cend()) {
	cur_file = &selected->second;
	show_file_detail(cur_file);
      }
      break;
    }
  }

}

void PrintPanel::show_file_detail(Tree *f) {
  if (f->is_leaf()) {
    if (f->contains_metadata()) {
      file_panel.refresh_view(f->metadata, f->full_path);
    } else {
      spdlog::trace("getting metadata for {}", f->name);
      ws.send_jsonrpc("server.files.metadata",
		      json::parse(R"({"filename":")" + f->full_path + R"("})"),
		      [f, this](json &d) { this->handle_metadata(f, d); });
    }
  }
}

void PrintPanel::handle_metadata(Tree *f, json &j) {
  spdlog::trace("handling metadata callback");  
  if (f->is_leaf()) {
    if (j.contains("result")) {
      std::lock_guard<std::mutex> lock(lv_lock);
      f->set_metadata(j);
      file_panel.refresh_view(f->metadata, f->full_path);
    }
  }
}

void PrintPanel::handle_back_btn(lv_event_t *event) {
  lv_obj_t *btn = lv_event_get_current_target(event);
  if (btn == back_btn.get_container()) {
    lv_obj_move_background(files_cont);
    print_status.background();    
  }
}

void PrintPanel::handle_print_callback(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED && cur_file != NULL) {

    json &pstat_state = State::get_instance()
      ->get_data("/printer_state/print_stats/state"_json_pointer);
    spdlog::debug("print panel print stats {}",
		  pstat_state.is_null() ? "nil" : pstat_state.template get<std::string>());
    
    if (!pstat_state.is_null()
	&& pstat_state.template get<std::string>() != "printing"
	&& pstat_state.template get<std::string>() != "paused") {
      spdlog::debug("printer ready to print. print file {}", cur_file->full_path);
	
      // ws.send_jsonrpc("printer.gcode.script",
      // 		    json::parse(R"({"script":"PRINT_PREPARE_CLEAR"})"));

      json fname_input = {{"filename", cur_file->full_path }};
      ws.send_jsonrpc("printer.print.start", fname_input);
      print_status.foreground();

    } else {
      lv_obj_clear_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(prompt_cont);
    }
  }
}

void PrintPanel::handle_status_btn(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED && cur_file != NULL) {
    spdlog::trace("status button clicked");
    print_status.foreground();
  }
}

void PrintPanel::handle_btns(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_CLICKED) {
    lv_obj_t *btn = lv_event_get_current_target(event);
    if (cur_file != NULL) {
      spdlog::trace("status prompt clicked");
      if (btn == queue_btn) {
	spdlog::trace("status prompt queue clicked");
      }

      if (btn == job_btn) {
	spdlog::trace("status prompt job clicked");
      }

      if (btn == cancel_btn) {
	spdlog::trace("status prompt cancel clicked");
	lv_obj_move_background(prompt_cont);
	lv_obj_add_flag(prompt_cont, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (btn == refresh_btn) {
      subscribe();
      
    } else if (btn == modified_sort_btn) {
      show_dir(cur_dir, SORTED_BY_MODIFIED);

    } else if (btn == az_sort_btn) {
      show_dir(cur_dir, SORTED_BY_NAME);
    }
  }
}
