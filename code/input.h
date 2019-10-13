
#define BUTTON_FLAG(name) (1 << GAME_BUTTON_INDEX_##name)
enum Game_Button_Index {
    // ;Settings If the control layout is changed, then USER_CONFIG_VERSION
    // needs to be incremented!
    GAME_BUTTON_INDEX_slot1    = 0,
    GAME_BUTTON_INDEX_slot2    = 1,
    GAME_BUTTON_INDEX_slot3    = 2,
    GAME_BUTTON_INDEX_slot4    = 3,
    GAME_BUTTON_INDEX_attack   = 4,
    GAME_BUTTON_INDEX_attack2  = 5,
    GAME_BUTTON_INDEX_up       = 6,
    GAME_BUTTON_INDEX_down     = 7,
    GAME_BUTTON_INDEX_left     = 8,
    GAME_BUTTON_INDEX_right    = 9,
    GAME_BUTTON_INDEX_menu     = 10,
    GAME_BUTTON_INDEX_jump     = 11,
    GAME_BUTTON_INDEX_run      = 12,
    GAME_BUTTON_INDEX_crouch   = 13,
    
    GAME_BUTTON_INDEX_editor   = 14,
    GAME_BUTTON_INDEX_profiler = 15,
    
    // When adding bindings, check that the integer type holds enough bits to support it!
    GAME_BUTTON_COUNT,
};

struct Input_Settings {
    f32 mouse_sensitivity;
    u32 bindings[GAME_BUTTON_COUNT];
};

struct Input {
    v2 xhairp;
    
    u16 old_button_state;
    u16 new_button_state;
};

#define BUTTON_RELEASED(in, button) button_released(in, BUTTON_FLAG(button))
static inline bool button_released(Input *in, u32 button) {
    return !(in->new_button_state & button) && (in->old_button_state & button);
}
static inline bool button_released(Input in, u32 button) {
    return button_released(&in, button);
}

#define BUTTON_HELD(in, button) button_held(in, BUTTON_FLAG(button))
static inline bool button_held(Input *in, u32 button) {
    return in->new_button_state & in->old_button_state & button;
}
static inline bool button_held(Input in, u32 button) {
    return button_held(&in, button);
}

#define BUTTON_DOWN(in, button) button_down(in, BUTTON_FLAG(button))
static inline bool button_down(Input *in, u32 button) {
    return in->new_button_state & button;
}
static inline bool button_down(Input in, u32 button) {
    return button_down(&in, button);
}

#define BUTTON_PRESSED(in, button) button_pressed(in, BUTTON_FLAG(button))
static inline bool button_pressed(Input *in, u32 button) {
    return (in->new_button_state & button) && !(in->old_button_state & button);
}
static inline bool button_pressed(Input in, u32 button) {
    return button_pressed(&in, button);
}

#define BIND_BUTTON(bindings, button, key) bindings[GAME_BUTTON_INDEX_##button] = GK_##key
