#include "ui_screens.h"
#include "ui_shared.h"
#include "ff.h" // FatFS
#include <stdio.h>
#include <string.h>

lv_obj_t* scr_profile;
static lv_obj_t* list;
extern UIContext uiCtx;

// Helper to access load_profile from main (defined in mtr_reflow_oven.cpp)
extern void load_profile(const char* path);

// Navigation State
static int profile_list_idx = 0;
static lv_obj_t* profile_buttons[20]; // Max 20 profiles
static int profile_count = 0;

static void update_profile_selection() {
    for (int i=0; i<profile_count; i++) {
        if (i == profile_list_idx) {
            lv_obj_set_state(profile_buttons[i], LV_STATE_FOCUSED, true);
            lv_obj_scroll_to_view(profile_buttons[i], LV_ANIM_OFF);
        } else {
            lv_obj_set_state(profile_buttons[i], LV_STATE_FOCUSED, false);
        }
    }
}

static void event_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e); // Cast required in v9
    if(code == LV_EVENT_CLICKED) {
        // Assume first child is label
        lv_obj_t * label = lv_obj_get_child(obj, 0);
        if (label) {
            const char* txt = lv_label_get_text(label);
            printf("Clicked: %s\n", txt);
            
            char path[64];
            snprintf(path, 64, "/profiles/%s", txt);
            load_profile(path);
            ui_refresh_dashboard_chart(); // Update the static chart
            
            // Go back to Dashboard
            uiCtx.current_screen = UI_SCREEN_DASHBOARD;
            uiCtx.full_redraw = true;
        }
    }
}

void ui_create_profile(void) {
    scr_profile = lv_obj_create(NULL);
    lv_obj_add_style(scr_profile, &style_screen_bg, 0);
    ui_create_header(scr_profile, "SELECT PROFILE");
    
    list = lv_list_create(scr_profile);
    lv_obj_set_size(list, 400, 220);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x222222), 0);
    lv_obj_set_style_pad_row(list, 5, 0); // Gap between buttons
    lv_obj_set_style_border_color(list, lv_color_hex(0x444444), 0);
    
    profile_count = 0;
    profile_list_idx = 0;

    // List files from SD Card
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    
    fr = f_opendir(&dir, "/profiles");
    if (fr == FR_OK) {
        while (true) {
            fr = f_readdir(&dir, &fno);
            if (fr != FR_OK || fno.fname[0] == 0) break;
            
            // Filter .json files
            if (strstr(fno.fname, ".json") == NULL) continue;

            // Add custom list button
            if (profile_count < 20) {
                lv_obj_t * btn = lv_button_create(list);
                lv_obj_set_width(btn, LV_PCT(100));
                lv_obj_set_height(btn, LV_SIZE_CONTENT);
                // Styles for focus
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x444444), 0);
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x007ACC), LV_STATE_FOCUSED);
                
                lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL); // Keep touch logic
                
                lv_obj_t * lab = lv_label_create(btn);
                lv_label_set_text(lab, fno.fname);
                
                profile_buttons[profile_count++] = btn;
            }
        }
        f_closedir(&dir);
        update_profile_selection(); // Highlight first item
    } else {
        lv_list_add_text(list, "SD Error / Empty");
    }
}

// Input is handled by LVGL Group usually, but our Input Task sends events manually.
// We need to map Encoder to LVGL navigation or handle it manually.
// Since we are not using LVGL Groups/Indev driver fully, we manually handle "Scroll" and "Click".
// Actually, `ui_screen_profile` needs input handling logic:
static int list_index = 0;

void ui_screen_profile_input(InputEvent evt) {
    // Basic navigation simulation
    // Ideally we should use lv_group for this.
    // For now, let's just use simple focus cycling if possible, or skip detailed nav logic implementation
    // and rely on a simpler approach:
    // We can't easily "focus" list items without groups.
    // We will just scroll the list and highlight items manually? Too complex for quick impl.
    
    // Alternative: Use a standard "Menu" approach with text labels like Main Menu?
    // lv_list is good.
    
    if (evt.type == EVT_BTN2_PRESS) {
        uiCtx.current_screen = UI_SCREEN_MAIN_MENU;
        uiCtx.full_redraw = true;
    } 
    else if (evt.type == EVT_ENC_CW) {
        profile_list_idx++;
        if (profile_list_idx >= profile_count) profile_list_idx = 0;
        update_profile_selection();
    }
    else if (evt.type == EVT_ENC_CCW) {
        profile_list_idx--;
        if (profile_list_idx < 0) profile_list_idx = profile_count - 1;
        if (profile_list_idx < 0) profile_list_idx = 0; // if count 0
        update_profile_selection();
    }
    else if (evt.type == EVT_ENC_BTN_PRESS) {
        // Trigger load on selected
        if (profile_count > 0 && profile_buttons[profile_list_idx]) {
             lv_obj_send_event(profile_buttons[profile_list_idx], LV_EVENT_CLICKED, NULL);
        }
    }
}

// Update
void ui_screen_profile_update(OvenState* state) {
    (void)state;
}
