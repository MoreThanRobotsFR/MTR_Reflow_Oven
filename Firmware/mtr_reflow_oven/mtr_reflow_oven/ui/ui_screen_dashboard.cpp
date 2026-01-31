#include "ui_screens.h"
#include "ui_shared.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

lv_obj_t* scr_dashboard;
static lv_obj_t* chart;
static lv_chart_series_t* ser_temp;
static lv_chart_series_t* ser_target;
static lv_obj_t* lbl_current;
static lv_obj_t* lbl_target;
static lv_obj_t* lbl_status;

extern UIContext uiCtx;

void ui_create_dashboard(void) {
    scr_dashboard = lv_obj_create(NULL);
    lv_obj_add_style(scr_dashboard, &style_screen_bg, 0);
    
    // Top Status Bar
    lv_obj_t* top_bar = lv_obj_create(scr_dashboard);
    lv_obj_set_size(top_bar, 480, 50);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);

    lbl_status = lv_label_create(top_bar);
    lv_label_set_text(lbl_status, "IDLE");
    lv_obj_center(lbl_status);
    lv_obj_add_style(lbl_status, &style_title, 0);

    // Main Chart
    chart = lv_chart_create(scr_dashboard);
    lv_obj_set_size(chart, 440, 200);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 20);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 300);
    lv_chart_set_point_count(chart, 100);
    lv_chart_set_div_line_count(chart, 5, 7);
    
    ser_temp = lv_chart_add_series(chart, lv_color_hex(0xFF4444), LV_CHART_AXIS_PRIMARY_Y); // Red
    ser_target = lv_chart_add_series(chart, lv_color_hex(0x44FF44), LV_CHART_AXIS_PRIMARY_Y); // Green

    // Bottom Info
    lbl_current = lv_label_create(scr_dashboard);
    lv_label_set_text(lbl_current, "Temp: ---");
    lv_obj_align(lbl_current, LV_ALIGN_BOTTOM_LEFT, 20, -10);
    lv_obj_add_style(lbl_current, &style_text_normal, 0);
    lv_obj_set_style_text_font(lbl_current, &lv_font_montserrat_20, 0);

    lbl_target = lv_label_create(scr_dashboard);
    lv_label_set_text(lbl_target, "Target: ---");
    lv_obj_align(lbl_target, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_add_style(lbl_target, &style_text_normal, 0);
    lv_obj_set_style_text_font(lbl_target, &lv_font_montserrat_20, 0);
}

extern ReflowProfile currentProfile;

// Helper: Calculate target temperature at a specific time (seconds)
static float get_target_at_time(uint32_t t) {
    if (currentProfile.segment_count == 0) return 20.0f; // Default
    
    uint32_t elapsed = 0;
    float current_temp = 20.0f; // Start assumption
    
    for (int i=0; i<currentProfile.segment_count; i++) {
        ProfileSegment* seg = &currentProfile.segments[i];
        
        // Check if t is within this segment
        if (t <= elapsed + seg->duration) {
            uint32_t t_in_seg = t - elapsed;
            
            if (seg->type == SEG_RAMP) {
                // Ramp: Start + (Slope * t) OR Interpolate from Start to Target
                // Slope is stored. 
                // Alternatively: Start + (Target - Start) * (t / Duration)
                // We know Target, but we need Start of this segment (which is end of prev).
                // current_temp is Start.
                return current_temp + ( (seg->target_temp - current_temp) * ((float)t_in_seg / seg->duration) );
            } 
            else if (seg->type == SEG_HOLD) {
                return seg->target_temp;
            }
            else { // STEP
                return seg->target_temp;
            }
        }
        
        // Advance
        elapsed += seg->duration;
        current_temp = seg->target_temp;
    }
    
    // If t is beyong total duration, return last temp
    return current_temp;
}

static uint32_t get_total_duration() {
    uint32_t d = 0;
    for (int i=0; i<currentProfile.segment_count; i++) {
        d += currentProfile.segments[i].duration;
    }
    return (d > 0) ? d : 300; // Default 300s
}

// Re-draw the Green Line (Target) based on loaded profile
// Should be called when entering dashboard or loading profile
void ui_refresh_dashboard_chart() {
    if (!chart) return;
    
    uint32_t duration = get_total_duration();
    // Pad duration slightly (e.g. +30s) for cooldown visibility? No, strictly profile.
    
    // Set X Range implicitly by point count mapping
    // We use strict 100 points
    lv_chart_set_point_count(chart, 100);
    
    // 1. Plot Green Line (Target)
    for (int i=0; i<100; i++) {
        // Time at this point
        uint32_t t = (duration * i) / 99;
        float temp = get_target_at_time(t);
        
        // Write directly to series buffer
        lv_chart_set_value_by_id(chart, ser_target, i, (lv_coord_t)temp);
        
        // Reset Red Line (Actual) to avoid ghost trails
        lv_chart_set_value_by_id(chart, ser_temp, i, LV_CHART_POINT_NONE); 
    }
    
    lv_chart_refresh(chart);
    
    // Update Header with Name
    char buf[64];
    snprintf(buf, 64, "DASHBOARD - %s", currentProfile.name);
    lv_label_set_text(lbl_status, buf); // Reuse status label for title temporarily or update logic
    // Actually lbl_status is for "IDLE/RUNNING". 
    // Let's create a separate label or append? 
    // User asked "display selected profile on top".
    // I entered "DASHBOARD" in header at init. 
    // I can stick a new label or just use the header function if I can access it.
    // ui_create_header creates a label. I can't access it easily unless I stored it.
    // But I can repurpose lbl_status or add a sub-label.
    // Let's prepend profile name to status string in update.
}


void ui_screen_dashboard_update(OvenState* state) {
    if (!state) return;
    
    // Handle "First Run" or "Profile Loaded" detection? 
    // For now, we might need to call refresh manually or detect change.
    // Ideally ui_manager calls this when switching screen.
    // But we'll do a lazy check or just rely on state.
    
    // 1. Update Labels
    lv_label_set_text_fmt(lbl_current, "T: %.1f C", state->current_temp_t1);
    lv_label_set_text_fmt(lbl_target, "Set: %.0f C", state->target_temp);
    
    // 2. Status Label with Profile Name
    const char* s_str = "UNKNOWN";
    lv_color_t color = lv_color_white();
    
    switch(state->state) {
        case STATE_IDLE:      s_str = "IDLE"; color = lv_color_hex(0x888888); break;
        case STATE_RUNNING:   s_str = "RUNNING"; color = lv_color_hex(0xFFA500); break;
        case STATE_MANUAL:    s_str = "MANUAL"; color = lv_color_hex(0xFF00FF); break; // Magenta
        case STATE_COOLDOWN:  s_str = "COOLING"; color = lv_color_hex(0x0088FF); break;
        case STATE_FAULT:     s_str = "FAULT"; color = lv_color_hex(0xFF0000); break;
        case STATE_PRE_CHECK: s_str = "PRE-CHECK"; color = lv_color_hex(0xFFFF00); break;
        default: break;
    }
    
    lv_label_set_text_fmt(lbl_status, "%s - %s", currentProfile.name, s_str);
    lv_obj_set_style_text_color(lbl_status, color, 0);

    // 3. Plot Red Line (Actual)
    // Only if running
    if (state->state == STATE_RUNNING || state->state == STATE_COOLDOWN) {
        uint32_t duration = get_total_duration();
        uint32_t elapsed = (xTaskGetTickCount() - state->profile_start_time) / 1000; // ms to s
        // (Assuming 1000 ticks/sec, FreeRTOS config dependent, usually 1ms tick)
        // Wait, FreeRTOS tick might be 1ms. pdMS_TO_TICKS(1000)? 
        // Standard rp2040 config is 1ms tick? Yes usually.
        
        if (elapsed > duration) elapsed = duration;
        
        // Map time to index 0..99
        int idx = (elapsed * 99) / duration;
        if (idx < 0) idx = 0;
        if (idx > 99) idx = 99;
        
        // Fill up to current index? Or just set point?
        // To make a continuous line, we should ideally ensure all points up to idx are filled.
        // But for simplicity, just setting current point works if update rate is high enough.
        // Better: Set value at idx.
        lv_chart_set_value_by_id(chart, ser_temp, idx, (lv_coord_t)state->current_temp_t1);
        lv_chart_refresh(chart);
    } else {
        // Idle: Maybe show ambient as a flat line or nothing?
        // Resetting happens in ui_refresh_dashboard_chart which should be called on Start/Load.
    }
}

void ui_screen_dashboard_input(InputEvent evt) {
    // Back to menu logic
    if (evt.type == EVT_BTN2_PRESS) {
        uiCtx.current_screen = UI_SCREEN_MAIN_MENU;
        uiCtx.full_redraw = true;
    }
    // Note: Start/Stop logic is handled in AppLogicTask mainly, 
    // but we could send a command queue item here if we wanted to decoupling.
    // For now, assume AppLogic handles BTN1 global toggles, or we handle it here:
    // Actually, if we want the UI to control the state, we should flip a flag or send a message.
    // But currently MTR Reflow Oven AppLogic handles BTN1 directly.
}
