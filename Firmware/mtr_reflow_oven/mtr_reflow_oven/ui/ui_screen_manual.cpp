#include "ui_screens.h"
#include "ui_shared.h"
#include <stdio.h>

lv_obj_t* scr_manual;

static lv_obj_t* lbl_target_val;
static lv_obj_t* lbl_actual_val;
static lv_obj_t* lbl_p1_val;
static lv_obj_t* lbl_p2_val;
static lv_obj_t* lbl_status_manual;

static int manual_target_temp = 20; // Default 20C
static bool heater_enabled = false;

extern UIContext uiCtx;
extern OvenState ovenState; 

void ui_create_manual(void) {
    scr_manual = lv_obj_create(NULL);
    lv_obj_add_style(scr_manual, &style_screen_bg, 0);
    ui_create_header(scr_manual, "MANUAL CONTROL");
    
    // Layout Container
    lv_obj_t* cont = lv_obj_create(scr_manual);
    lv_obj_set_size(cont, 440, 220);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(cont, 15, 0);

    // Target Temp Row
    lv_obj_t* row1 = lv_obj_create(cont);
    lv_obj_set_width(row1, LV_PCT(100));
    lv_obj_set_height(row1, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);

    lv_obj_t* l1 = lv_label_create(row1);
    lv_label_set_text(l1, "Target Temp:");
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l1, lv_color_white(), 0);
    
    lbl_target_val = lv_label_create(row1);
    lv_label_set_text_fmt(lbl_target_val, "%d C", manual_target_temp);
    lv_obj_set_style_text_font(lbl_target_val, &lv_font_montserrat_40, 0); // Large
    lv_obj_set_style_text_color(lbl_target_val, lv_color_hex(0x00FF00), 0); // Green

    // Actual Temp Row
    lv_obj_t* row2 = lv_obj_create(cont);
    lv_obj_set_width(row2, LV_PCT(100));
    lv_obj_set_height(row2, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);

    lv_obj_t* l2 = lv_label_create(row2);
    lv_label_set_text(l2, "Actual Temp:");
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l2, lv_color_white(), 0);

    lbl_actual_val = lv_label_create(row2);
    lv_label_set_text(lbl_actual_val, "--- C");
    lv_obj_set_style_text_font(lbl_actual_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_actual_val, lv_color_hex(0xFF4444), 0); // Red scale

    // Power Info Row
    lv_obj_t* row3 = lv_obj_create(cont);
    lv_obj_set_width(row3, LV_PCT(100));
    lv_obj_set_height(row3, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row3, 0, 0);

    lbl_p1_val = lv_label_create(row3);
    lv_label_set_text(lbl_p1_val, "SSR1: 0%");
    lv_obj_set_style_text_color(lbl_p1_val, lv_color_hex(0x00D0FF), 0);

    lbl_p2_val = lv_label_create(row3);
    lv_label_set_text(lbl_p2_val, "SSR2: 0%");
    lv_obj_set_style_text_color(lbl_p2_val, lv_color_hex(0x00D0FF), 0);

    // Status Label
    lbl_status_manual = lv_label_create(cont);
    lv_label_set_text(lbl_status_manual, "HEATER: OFF");
    lv_obj_align(lbl_status_manual, LV_ALIGN_CENTER, 0, 0); // Flex align handles it
    lv_obj_set_style_text_font(lbl_status_manual, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_status_manual, lv_color_hex(0x888888), 0);
}

void ui_screen_manual_input(InputEvent evt) {
    if (evt.type == EVT_BTN2_PRESS) {
        // Exit
        heater_enabled = false;
        manual_target_temp = 20;
        ovenState.state = STATE_IDLE; // Stop PID safely
        ovenState.target_temp = 0;
        ovenState.power_output_1 = 0;
        ovenState.power_output_2 = 0;
        
        uiCtx.current_screen = UI_SCREEN_MAIN_MENU;
        uiCtx.full_redraw = true;
    }
    else if (evt.type == EVT_ENC_CW) {
        manual_target_temp += 5;
        if (manual_target_temp > 260) manual_target_temp = 260;
        
        lv_label_set_text_fmt(lbl_target_val, "%d C", manual_target_temp);
        
        if (heater_enabled) ovenState.target_temp = (float)manual_target_temp;
    }
    else if (evt.type == EVT_ENC_CCW) {
        manual_target_temp -= 5;
        if (manual_target_temp < 20) manual_target_temp = 20;

        lv_label_set_text_fmt(lbl_target_val, "%d C", manual_target_temp);
        
        if (heater_enabled) ovenState.target_temp = (float)manual_target_temp;
    }
    else if (evt.type == EVT_BTN1_PRESS) {
        // Toggle Heater
        heater_enabled = !heater_enabled;
        if (heater_enabled) {
            lv_label_set_text(lbl_status_manual, "HEATER: ON (PID)");
            lv_obj_set_style_text_color(lbl_status_manual, lv_color_hex(0xFF0000), 0); // Red
            
            ovenState.state = STATE_MANUAL; 
            ovenState.target_temp = (float)manual_target_temp;
        } else {
            lv_label_set_text(lbl_status_manual, "HEATER: OFF");
            lv_obj_set_style_text_color(lbl_status_manual, lv_color_hex(0x888888), 0); // Grey
            ovenState.state = STATE_IDLE; // Stop PID
            ovenState.target_temp = 0;
            ovenState.power_output_1 = 0;
            ovenState.power_output_2 = 0;
        }
    }
}

void ui_screen_manual_update(OvenState* state) {
    // Update Actuals
    lv_label_set_text_fmt(lbl_actual_val, "%.1f C", state->current_temp_t1);
    
    // Update Power
    lv_label_set_text_fmt(lbl_p1_val, "SSR1: %.0f%%", state->power_output_1);
    lv_label_set_text_fmt(lbl_p2_val, "SSR2: %.0f%%", state->power_output_2);
    
    if (state->state == STATE_FAULT) {
        heater_enabled = false;
        manual_target_temp = 20;
        lv_label_set_text_fmt(lbl_target_val, "%d C", manual_target_temp);
        lv_label_set_text(lbl_status_manual, "FAULT STOP");
        lv_obj_set_style_text_color(lbl_status_manual, lv_color_hex(0xFF0000), 0);
    }
}
