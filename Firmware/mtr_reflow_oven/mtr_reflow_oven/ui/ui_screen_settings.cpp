#include "ui_screens.h"
#include "ui_shared.h"
#include <stdio.h>

lv_obj_t* scr_settings;
extern UIContext uiCtx;
extern SystemConfig sysConfig; // Defined in mtr_reflow_oven.cpp
extern void save_system_config(); // Defined in mtr_reflow_oven.cpp

// Settings State
static int selected_idx = 0;
static bool edit_mode = false;
static lv_obj_t* item_containers[6]; // Track containers for highlighting
static lv_obj_t* value_labels[6];    // Track labels for updating text

// Config References (Pointers to sysConfig vars)
// 0: Kp, 1: Ki, 2: Kd, 3: Sound
// We can't use simple array of floats because Sound is int, but checking struct, sound is int volume.
// For simplicity, let's treat everything as float 1 decimal or 3 decimal.

typedef struct {
    const char* name;
    float* val_ptr;
    float step;
    const char* fmt;
} SettingItem;

static SettingItem items[6]; 

// Init items dynamically
void init_settings_data() {
    items[0] = {"SSR1 Kp", &sysConfig.pid_ssr1_kp, 0.1f, "%.1f"};
    items[1] = {"SSR1 Ki", &sysConfig.pid_ssr1_ki, 0.001f, "%.3f"};
    items[2] = {"SSR1 Kd", &sysConfig.pid_ssr1_kd, 0.5f, "%.1f"};
    items[3] = {"SSR2 Kp", &sysConfig.pid_ssr2_kp, 0.1f, "%.1f"};
    items[4] = {"SSR2 Ki", &sysConfig.pid_ssr2_ki, 0.001f, "%.3f"};
    items[5] = {"SSR2 Kd", &sysConfig.pid_ssr2_kd, 0.5f, "%.1f"};
}

void update_settings_ui() {
    for (int i=0; i<6; i++) {
        // Highlight logic
        if (i == selected_idx) {
            lv_obj_set_style_bg_opa(item_containers[i], LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(item_containers[i], edit_mode ? lv_color_hex(0xFF4400) : lv_color_hex(0x007ACC), 0);
            
            // Ensure visible
            lv_obj_scroll_to_view(item_containers[i], LV_ANIM_OFF);
        } else {
            lv_obj_set_style_bg_opa(item_containers[i], LV_OPA_TRANSP, 0);
        }
        
        // Update Text
        if (items[i].val_ptr) {
            lv_label_set_text_fmt(value_labels[i], items[i].fmt, *items[i].val_ptr);
        }
    }
}

void ui_create_settings(void) {
    init_settings_data();
    
    scr_settings = lv_obj_create(NULL);
    lv_obj_add_style(scr_settings, &style_screen_bg, 0);
    ui_create_header(scr_settings, "SETTINGS");
    
    lv_obj_t* cont = lv_obj_create(scr_settings);
    lv_obj_set_size(cont, 440, 220);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), 0);
    lv_obj_set_style_pad_gap(cont, 5, 0);
    
    for (int i=0; i<6; i++) {
        item_containers[i] = lv_obj_create(cont);
        lv_obj_set_width(item_containers[i], LV_PCT(100));
        lv_obj_set_height(item_containers[i], 40);
        lv_obj_set_flex_flow(item_containers[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item_containers[i], LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_border_width(item_containers[i], 0, 0);
        lv_obj_set_style_pad_all(item_containers[i], 5, 0);
        
        lv_obj_t* l1 = lv_label_create(item_containers[i]);
        lv_label_set_text(l1, items[i].name);
        lv_obj_set_style_text_color(l1, lv_color_white(), 0);
        
        value_labels[i] = lv_label_create(item_containers[i]);
        lv_label_set_text(value_labels[i], "---");
        lv_obj_set_style_text_color(value_labels[i], lv_color_hex(0x00D0FF), 0);
    }
    
    update_settings_ui();
}

void ui_screen_settings_input(InputEvent evt) {
    if (evt.type == EVT_BTN2_PRESS) {
        if (edit_mode) {
             edit_mode = false;
             update_settings_ui();
        } else {
             // Save System Config
             save_system_config(); 
             
             uiCtx.current_screen = UI_SCREEN_MAIN_MENU;
             uiCtx.full_redraw = true;
        }
    }
    else if (evt.type == EVT_ENC_BTN_PRESS) {
        edit_mode = !edit_mode;
        update_settings_ui();
    }
    else if (evt.type == EVT_ENC_CW) {
        if (edit_mode) {
            *items[selected_idx].val_ptr += items[selected_idx].step;
        } else {
            selected_idx++;
            if (selected_idx > 5) selected_idx = 0;
        }
        update_settings_ui();
    }
    else if (evt.type == EVT_ENC_CCW) {
        if (edit_mode) {
            *items[selected_idx].val_ptr -= items[selected_idx].step;
             if (*items[selected_idx].val_ptr < 0) *items[selected_idx].val_ptr = 0;
        } else {
            selected_idx--;
            if (selected_idx < 0) selected_idx = 5;
        }
        update_settings_ui();
    }
}

void ui_screen_settings_update(OvenState* state) {
    (void)state;
}
