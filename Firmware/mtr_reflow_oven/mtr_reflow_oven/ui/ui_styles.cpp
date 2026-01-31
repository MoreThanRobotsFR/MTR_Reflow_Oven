#include "ui_shared.h"

lv_style_t style_screen_bg;
lv_style_t style_title;
lv_style_t style_text_normal;
lv_style_t style_btn_default;
lv_style_t style_btn_pressed;
lv_style_t style_btn_selected;

void ui_styles_init(void) {
    // Screen Background (Dark)
    lv_style_init(&style_screen_bg);
    lv_style_set_bg_color(&style_screen_bg, lv_color_hex(0x111111));
    lv_style_set_bg_opa(&style_screen_bg, LV_OPA_COVER);
    lv_style_set_text_color(&style_screen_bg, lv_color_white());

    // Title Font
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_20); // Check available fonts
    lv_style_set_text_color(&style_title, lv_color_hex(0x00D0FF)); // Cyan
    lv_style_set_text_align(&style_title, LV_TEXT_ALIGN_CENTER);

    // Normal Text
    lv_style_init(&style_text_normal);
    lv_style_set_text_color(&style_text_normal, lv_color_hex(0xDDDDDD));
    lv_style_set_text_font(&style_text_normal, &lv_font_montserrat_14);

    // Buttons (Default)
    lv_style_init(&style_btn_default);
    lv_style_set_bg_color(&style_btn_default, lv_color_hex(0x333333));
    lv_style_set_border_color(&style_btn_default, lv_color_hex(0x555555));
    lv_style_set_border_width(&style_btn_default, 2);
    lv_style_set_radius(&style_btn_default, 8);
    lv_style_set_pad_all(&style_btn_default, 10);
    lv_style_set_text_color(&style_btn_default, lv_color_white());

    // Buttons (Selected/Focused)
    lv_style_init(&style_btn_selected);
    lv_style_set_bg_color(&style_btn_selected, lv_color_hex(0x007ACC)); // Blue
    lv_style_set_border_color(&style_btn_selected, lv_color_white());
    lv_style_set_border_width(&style_btn_selected, 3);
    lv_style_set_shadow_width(&style_btn_selected, 10);
    lv_style_set_shadow_color(&style_btn_selected, lv_color_hex(0x007ACC));

    // Buttons (Pressed)
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, lv_color_hex(0x005A99));
    lv_style_set_translate_y(&style_btn_pressed, 2);
}

void ui_create_header(lv_obj_t* parent, const char* title) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_add_style(lbl, &style_title, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 10);
}
