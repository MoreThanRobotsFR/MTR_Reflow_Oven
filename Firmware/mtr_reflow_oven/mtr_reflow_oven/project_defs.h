#ifndef PROJECT_DEFS_H
#define PROJECT_DEFS_H

#include <stdint.h>
#include <stdbool.h>

// --- Constants ---
#define MAX_PROFILE_SEGMENTS 20

// --- Enums ---
typedef enum {
    STATE_INIT,
    STATE_IDLE,
    STATE_PRE_CHECK,
    STATE_RUNNING,
    STATE_MANUAL,
    STATE_COOLDOWN,
    STATE_FAULT
} OvenStateEnum;

typedef enum {
    SEG_RAMP,
    SEG_STEP,
    SEG_HOLD
} SegmentType;

typedef enum {
    UI_SCREEN_DASHBOARD,
    UI_SCREEN_MAIN_MENU,
    UI_SCREEN_PROFILE_SELECT,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_PARAM,
    UI_SCREEN_MANUAL,
    UI_SCREEN_SYS_INFO
} UIScreenEnum;

typedef enum {
    EVT_BTN1_PRESS,
    EVT_BTN2_PRESS,

    EVT_ENC_BTN_PRESS,
    EVT_ENC_CW,
    EVT_ENC_CCW,
    EVT_NONE
} InputEventType;

// --- Structs ---

typedef struct {
    SegmentType type;
    float target_temp;
    float slope;      // degC/sec
    uint32_t duration; // seconds
    char note[16];
} ProfileSegment;

typedef struct {
    char name[32];
    ProfileSegment segments[MAX_PROFILE_SEGMENTS];
    uint8_t segment_count;
} ReflowProfile;

typedef struct {
    OvenStateEnum state;
    float current_temp_t1;
    float current_temp_t2;
    float current_temp_amb;
    float target_temp;
    float power_output_1; // 0-100%
    float power_output_2; // 0-100%
    uint32_t profile_start_time;
    uint8_t current_segment_index;
    bool t2_connected;
    bool fault_active;
} OvenState;

typedef struct {
    float t1;
    float t2;
    float amb;
    uint32_t flags;
} SensorData;

typedef struct {
    InputEventType type;
    uint32_t timestamp;
} InputEvent;

typedef struct {
    UIScreenEnum current_screen;
    int cursor_index;
    int scroll_offset;
    bool edit_mode; // If true, rotary changes value instead of cursor
    
    // Selection Data
    int selected_profile_index;
    
    // Dirty Flags for Redraw
    bool full_redraw;
} UIContext;

extern UIContext uiCtx;

typedef struct {
    bool enable_sensor2_check;
    int screen_orientation;
    int ssr2_is_present; // Added as per request
    int buzzer_volume;
    float pid_ssr1_kp, pid_ssr1_ki, pid_ssr1_kd;
    float pid_ssr2_kp, pid_ssr2_ki, pid_ssr2_kd;
    float t1_offset;
    float t2_offset;
} SystemConfig;

#endif // PROJECT_DEFS_H
