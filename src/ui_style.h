#ifndef __UI_STYLE_H__
#define __UI_STYLE_H__

// Glass UI design system: tokens, fonts, icons, accent color, baked
// "acrylic" wallpaper and the theme callback that styles every widget.

#include "lvgl/lvgl.h"

#include <functional>

LV_FONT_DECLARE(manrope_14);
LV_FONT_DECLARE(manrope_16);
LV_FONT_DECLARE(manrope_20);
LV_FONT_DECLARE(manrope_28);
LV_FONT_DECLARE(manrope_48);
LV_FONT_DECLARE(mdi_20);
LV_FONT_DECLARE(mdi_28);

// Material Design Icons glyphs available in mdi_20 / mdi_28
#define ICON_HOME         "\xF3\xB0\x8B\x9C" // U+F02DC
#define ICON_MACROS       "\xF3\xB1\xB2\x83" // U+F1C83
#define ICON_CONSOLE      "\xF3\xB0\x86\x8D" // U+F018D
#define ICON_TUNE         "\xF3\xB1\x95\x82" // U+F1542
#define ICON_SETTINGS     "\xF3\xB0\x92\x93" // U+F0493
#define ICON_MOVE         "\xF3\xB0\x81\x81" // U+F0041
#define ICON_NOZZLE       "\xF3\xB0\xB9\x9B" // U+F0E5B
#define ICON_NOZZLE_HEAT  "\xF3\xB1\xA2\xB8" // U+F18B8
#define ICON_FAN          "\xF3\xB0\x88\x90" // U+F0210
#define ICON_LIGHT        "\xF3\xB0\x8C\xB6" // U+F0336
#define ICON_COOLDOWN     "\xF3\xB0\x9C\x97" // U+F0717
#define ICON_PRINTER      "\xF3\xB0\x90\xAB" // U+F042B
#define ICON_BED          "\xF3\xB1\xA9\x85" // U+F1A45
#define ICON_THERMO       "\xF3\xB0\x94\x8F" // U+F050F
#define ICON_CHAMBER      "\xF3\xB0\x86\xA7" // U+F01A7
#define ICON_LAYERS       "\xF3\xB0\xBD\x98" // U+F0F58
#define ICON_SPEED        "\xF3\xB0\x93\x85" // U+F04C5
#define ICON_FLOW         "\xF3\xB0\x96\x8C" // U+F058C
#define ICON_ZOFFSET      "\xF3\xB0\xA1\x8D" // U+F084D
#define ICON_PAUSE        "\xF3\xB0\x8F\xA4" // U+F03E4
#define ICON_PLAY         "\xF3\xB0\x90\x8A" // U+F040A
#define ICON_CLOSE        "\xF3\xB0\x85\x96" // U+F0156
#define ICON_ESTOP        "\xF3\xB0\x80\xA9" // U+F0029
#define ICON_BACK         "\xF3\xB0\x81\x8D" // U+F004D
#define ICON_HOURGLASS    "\xF3\xB0\x94\x9F" // U+F051F
#define ICON_CLOCK        "\xF3\xB0\x85\x90" // U+F0150
#define ICON_PALETTE      "\xF3\xB0\x8F\x98" // U+F03D8
#define ICON_BLUR         "\xF3\xB0\x82\xB5" // U+F00B5
#define ICON_WIFI         "\xF3\xB0\x96\xA9" // U+F05A9
#define ICON_CHECK        "\xF3\xB0\x84\xAC" // U+F012C

namespace ui {

// layout tokens (800x480)
constexpr lv_coord_t GAP = 12;
constexpr lv_coord_t RADIUS_CARD = 18;
constexpr lv_coord_t RADIUS_INNER = 14;

// fixed palette
lv_color_t base();
lv_color_t text();
lv_color_t text2();
lv_color_t text3();
lv_color_t bed();
lv_color_t chamber();
lv_color_t danger();
lv_color_t warn();

// accent
constexpr size_t ACCENT_COUNT = 8;
lv_color_t accent_preset(size_t i);
lv_color_t accent();
lv_color_t on_accent();          // readable text/icon color on top of accent
bool acrylic();

// Reads accent/acrylic from guppyconfig.json. Call before the display theme is created.
void load_config();
// Renders the wallpaper and builds the styles. Call after the display is registered.
void init();
// Changes accent / acrylic at runtime, restyles everything and persists to config.
void set_accent(lv_color_t c);
void set_acrylic(bool on);
// Called after accent or acrylic changed (custom drawn widgets, chart series...).
void on_change(std::function<void()> cb);

// LVGL theme apply callback: styles every newly created widget.
void theme_apply_cb(lv_theme_t *th, lv_obj_t *obj);

const lv_img_dsc_t *wallpaper();

// style helpers
void card(lv_obj_t *obj);          // frosted glass card
void card_solid(lv_obj_t *obj);    // opaque card for overlays (numpad, dialogs)
void pill(lv_obj_t *obj);          // small glass capsule
void icon_bubble(lv_obj_t *obj);   // round glass bubble behind an icon
void clear(lv_obj_t *obj);         // transparent container, no padding
void accent_fill(lv_obj_t *obj);   // accent background + on_accent text
void danger_outline(lv_obj_t *obj);
void danger_fill(lv_obj_t *obj);
lv_obj_t *icon_label(lv_obj_t *parent, const char *icon, const lv_font_t *font, lv_color_t color);
lv_obj_t *text_label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t color);

} // namespace ui

#endif // __UI_STYLE_H__
