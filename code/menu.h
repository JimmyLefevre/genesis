
// @Cleanup: Rename this to Interaction_Type?
enum Widget_Type {
    WIDGET_TYPE_NONE = 0,
    WIDGET_TYPE_BUTTON,
    WIDGET_TYPE_TOGGLE,
    WIDGET_TYPE_DROPDOWN,
    WIDGET_TYPE_SLIDER,
    WIDGET_TYPE_KEYBIND,
    WIDGET_TYPE_COUNT,
};

struct Slider {
    f32     min;
    f32     max;
    f32 *actual;
};

struct Button {
    any32  set_to;
    any32 *actual;
};

struct Toggle {
    bool *actual;
};

struct Dropdown_Option {
    any32  value;
    string label;
};

struct Dropdown {
    Dropdown_Option *options;
    any32 *actual;
    s8 option_count;
    s8 hover;
    s8 active;
};

struct Keybind {
    u32 *actual;
};

struct Interaction {
    u8 type;
    u8 index;
};
inline Interaction make_interaction() {
    return {WIDGET_TYPE_NONE, 0};
}
inline Interaction make_interaction(s32 type, s32 index) {
    ASSERT((u8)type == type);
    ASSERT((u8)index == index);
    return {(u8)type, (u8)index};
}

struct Menu_Label {
    string s;
    v2 p;
    f32 pixel_height;
    s8 font;
    bool centered;
};

struct Menu_Page {
    union {
        struct {
            rect2 *  button_aabbs;
            rect2 *  toggle_aabbs;
            rect2 *dropdown_aabbs;
            rect2 *  slider_aabbs;
            rect2 * keybind_aabbs;
        };
        rect2 *aabbs_by_type[WIDGET_TYPE_COUNT - 1];
    };
    
    union {
        struct {
            s32   button_count;
            s32   toggle_count;
            s32 dropdown_count;
            s32   slider_count;
            s32  keybind_count;
        };
        s32 counts_by_type[WIDGET_TYPE_COUNT - 1];
    };
    
    Slider     *sliders;
    Button     *buttons;
    Toggle     *toggles;
    Dropdown *dropdowns;
    Keybind   *keybinds;
    
    Menu_Label *labels;
    s32    label_count;
};

struct Menu {
    Interaction interaction;
    Interaction hover;
    
    Menu_Page *pages;
    s32 current_page;
    
    s32 something;
    bool should_display_text;
    s32 sliders_to_display;
};
