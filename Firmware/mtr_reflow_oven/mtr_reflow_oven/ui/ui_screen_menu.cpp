#include "ui_screens.h"
#include "ui_shared.h"
#include <stdio.h>

lv_obj_t* scr_menu;
static lv_obj_t* menu_btns[4];
static int menu_index = 0;

extern UIContext uiCtx; // Used to change screen

void ui_update_menu_focus() {
    for (int i=0; i<4; i++) {
        lv_obj_remove_style(menu_btns[i], &style_btn_selected, 0);
    }
    lv_obj_add_style(menu_btns[menu_index], &style_btn_selected, 0);
}

void ui_create_menu(void) {
    scr_menu = lv_obj_create(NULL);
    lv_obj_add_style(scr_menu, &style_screen_bg, 0);
    
    ui_create_header(scr_menu, "MTR REFLOW CONTROLLER");

    // Container
    lv_obj_t* cont = lv_obj_create(scr_menu);
    lv_obj_set_size(cont, 420, 240);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 15);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_pad_row(cont, 15, 0);

    const char* titles[] = {"AUTO REFLOW", "MANUAL MODE", "PROFILES", "SETTINGS"};

    for (int i=0; i<4; i++) {
        menu_btns[i] = lv_btn_create(cont);
        lv_obj_set_width(menu_btns[i], 320);
        lv_obj_add_style(menu_btns[i], &style_btn_default, 0);
        
        lv_obj_t* lbl = lv_label_create(menu_btns[i]);
        lv_label_set_text(lbl, titles[i]);
        lv_obj_center(lbl);
    }

    ui_update_menu_focus();
}

void ui_screen_menu_input(InputEvent evt) {
    if (evt.type == EVT_ENC_CW || evt.type == EVT_BTN2_PRESS) {
        menu_index++;
        if (menu_index >= 4) menu_index = 0;
        ui_update_menu_focus();
    } 
    else if (evt.type == EVT_ENC_CCW) {
        menu_index--;
        if (menu_index < 0) menu_index = 3;
        ui_update_menu_focus();
    }
    else if (evt.type == EVT_BTN1_PRESS || evt.type == EVT_ENC_BTN_PRESS) {
        // Select
        switch (menu_index) {
            case 0: uiCtx.current_screen = UI_SCREEN_DASHBOARD; break;
            case 1: uiCtx.current_screen = UI_SCREEN_MANUAL; break;
            case 2: uiCtx.current_screen = UI_SCREEN_PROFILE_SELECT; break;
            case 3: uiCtx.current_screen = UI_SCREEN_SETTINGS; break;
        }
        uiCtx.full_redraw = true; // Signal manager to switch screen
    }
}
