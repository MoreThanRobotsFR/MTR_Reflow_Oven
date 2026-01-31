#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include "../project_defs.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Screen Objects (Global accessible for manager)
extern lv_obj_t* scr_menu;
extern lv_obj_t* scr_dashboard;
extern lv_obj_t* scr_manual;
extern lv_obj_t* scr_settings;
extern lv_obj_t* scr_profile;

// Init Functions
void ui_screens_init(void);
void ui_create_menu(void);
void ui_create_dashboard(void);
void ui_create_manual(void);
void ui_create_settings(void);
void ui_create_profile(void);

// Screen specific input handlers
void ui_screen_menu_input(InputEvent evt);
void ui_screen_dashboard_input(InputEvent evt);

void ui_screen_manual_input(InputEvent evt);
void ui_screen_settings_input(InputEvent evt);
void ui_screen_profile_input(InputEvent evt);

// Screen specific updaters
void ui_screen_dashboard_update(OvenState* state);
void ui_screen_manual_update(OvenState* state);
void ui_screen_settings_update(OvenState* state);
void ui_screen_profile_update(OvenState* state);

void ui_refresh_dashboard_chart(void);

#ifdef __cplusplus
}
#endif

#endif // UI_SCREENS_H
