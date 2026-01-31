#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../project_defs.h"

// Initialize UI (LVGL, Displays, Styles)
void ui_init(void);

// Process input events (Buttons, Encoder)
void ui_process_input(InputEvent evt);

// Update UI based on system state (call periodically, e.g. 10Hz)
void ui_update_state(OvenState* state);

// Timer handler for LVGL (call frequently, e.g. 5ms)
void ui_tick(void);

#ifdef __cplusplus
}
#endif

#endif // UI_MANAGER_H
