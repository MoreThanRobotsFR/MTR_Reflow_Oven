#ifndef UI_SHARED_H
#define UI_SHARED_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Styles ---
extern lv_style_t style_screen_bg;
extern lv_style_t style_title;
extern lv_style_t style_text_normal;
extern lv_style_t style_btn_default;
extern lv_style_t style_btn_pressed;
extern lv_style_t style_btn_selected;

void ui_styles_init(void);

// --- Common UI Helpers ---
void ui_create_header(lv_obj_t* parent, const char* title);

#ifdef __cplusplus
}
#endif

#endif // UI_SHARED_H
