
static void clear_input_state(Input *in) {
    in->xhairp = V2(0.5f);
    in->old_button_state = 0;
    in->new_button_state = 0;
}

static void move_mouse(f32 sensitivity, Input *in, s32 dm[2]) {
    v2 dm_f = V2((f32)dm[0], (f32)dm[1]);
    // @Hack: Correcting for aspect ratio.
    dm_f.y *= 16.0f/9.0f;
    in->xhairp = in->xhairp + dm_f * sensitivity;
    in->xhairp.x = f_clamp(in->xhairp.x, 0.0f, 1.0f);
    in->xhairp.y = f_clamp(in->xhairp.y, 0.0f, 1.0f);
}

static void inline unpress_button(Input *in, u32 button) {
    ASSERT(button < GAME_BUTTON_COUNT);
    in->new_button_state &= ~(1 << button);
}

static void inline press_button(Input *in, u32 button) {
    ASSERT(button < GAME_BUTTON_COUNT);
    in->new_button_state |=  (1 << button);
}

static void process_key(u32 *bindings, Input *in, u32 key_value, bool down) {
    for(u32 i = 0; i < GAME_BUTTON_COUNT; ++i) {
        if(bindings[i] == key_value) {
            if(down) {
                press_button(in, i);
            } else {
                unpress_button(in, i);
            }
        }
    }
}

static inline void advance_input(Input *in) {
    in->old_button_state = in->new_button_state;
}
