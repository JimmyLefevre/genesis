
enum Game_Key {
    GK_UNHANDLED,
    GK_ESCAPE,
    GK_1,
    GK_2,
    GK_3,
    GK_4,
    GK_5,
    GK_6,
    GK_7,
    GK_8,
    GK_9,
    GK_0,
    GK_MINUS,
    GK_EQUALS,
    GK_BACKSPACE,
    GK_TAB,
    GK_Q,
    GK_W,
    GK_E,
    GK_R,
    GK_T,
    GK_Y,
    GK_U,
    GK_I,
    GK_O,
    GK_P,
    GK_LEFTBRACKET,
    GK_RIGHTBRACKET,
    GK_ENTER,
    GK_LCTRL,
    GK_A,
    GK_S,
    GK_D,
    GK_F,
    GK_G,
    GK_H,
    GK_J,
    GK_K,
    GK_L,
    GK_SEMICOLON,
    GK_APOSTROPHE,
    GK_GRAVE,
    GK_LSHIFT,
    GK_BACKSLASH,
    GK_Z,
    GK_X,
    GK_C,
    GK_V,
    GK_B,
    GK_N,
    GK_M,
    GK_COMMA,
    GK_PERIOD,
    GK_SLASH,
    GK_RSHIFT,
    GK_NUMERICASTERISK,
    GK_LALT,
    GK_SPACEBAR,
    GK_CAPSLOCK,
    GK_F1,
    GK_F2,
    GK_F3,
    GK_F4,
    GK_F5,
    GK_F6,
    GK_F7,
    GK_F8,
    GK_F9,
    GK_F10,
    GK_NUMLOCK,
    GK_SCROLLLOCK,
    GK_NUMERIC7,
    GK_NUMERIC8,
    GK_NUMERIC9,
    GK_NUMERICHYPHEN,
    GK_NUMERIC4,
    GK_NUMERIC5,
    GK_NUMERIC6,
    GK_NUMERICPLUS,
    GK_NUMERIC1,
    GK_NUMERIC2,
    GK_NUMERIC3,
    GK_NUMERIC0,
    GK_NUMERICPERIOD,
    GK_ANGLEBRACKET,
    GK_F11,
    GK_F12,
    GK_F13,
    GK_F14,
    GK_F15,
    GK_F16,
    GK_F17,
    GK_F18,
    GK_F19,
    GK_F20,
    GK_F21,
    GK_F22,
    GK_F23,
    GK_INTERNATIONAL1,
    GK_INTERNATIONAL2,
    GK_INTERNATIONAL3,
    GK_INTERNATIONAL4,
    GK_INTERNATIONAL5,
    GK_INTERNATIONAL6,
    GK_NUMERICENTER,
    GK_RCTRL,
    GK_PRINTSCREEN,
    GK_RALT,
    GK_HOME,
    GK_UPARROW,
    GK_PAGEUP,
    GK_LEFTARROW,
    GK_RIGHTARROW,
    GK_END,
    GK_DOWNARROW,
    GK_PAGEDOWN,
    GK_INSERT,
    GK_DELETE,
    GK_LSUPER,
    GK_RSUPER,
    GK_APPLICATION,
    GK_LEFTCLICK,
    GK_RIGHTCLICK,
    GK_MIDDLECLICK,
    GK_MOUSE4,
    GK_MOUSE5,
    GK_MOUSEWHEELUP,
    GK_MOUSEWHEELDOWN,
    GK_COUNT,
};

#if OS_WINDOWS
// See http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/scancode.doc for details.
enum Windows_Scancode {
    
    WINSC_IGNORED0,
    WINSC_ESC,
    WINSC_1,
    WINSC_2,
    WINSC_3,
    WINSC_4,
    WINSC_5,
    WINSC_6,
    WINSC_7,
    WINSC_8,
    WINSC_9,
    WINSC_0,
    WINSC_MINUS,
    WINSC_EQUALS,
    WINSC_BACKSPACE,
    WINSC_TAB,
    WINSC_Q,
    WINSC_W,
    WINSC_E,
    WINSC_R,
    WINSC_T,
    WINSC_Y,
    WINSC_U,
    WINSC_I,
    WINSC_O,
    WINSC_P,
    WINSC_LEFTBRACKET,
    WINSC_RIGHTBRACKET,
    WINSC_ENTER, // Extended: NUMERICENTER
    WINSC_LCTRL, // Extended: RCTRL
    WINSC_A,
    WINSC_S,
    WINSC_D,
    WINSC_F,
    WINSC_G,
    WINSC_H,
    WINSC_J,
    WINSC_K,
    WINSC_L,
    WINSC_SEMICOLON,
    WINSC_APOSTROPHE,
    WINSC_GRAVE,
    WINSC_LSHIFT,
    WINSC_BACKSLASH,
    WINSC_Z,
    WINSC_X,
    WINSC_C,
    WINSC_V,
    WINSC_B,
    WINSC_N,
    WINSC_M,
    WINSC_COMMA,
    WINSC_PERIOD,
    WINSC_SLASH,
    WINSC_RSHIFT,
    WINSC_NUMERICASTERISK, // Extended: PRINTSCREEN(E02A E037)
    WINSC_LALT, // Extended: RALT
    WINSC_SPACEBAR,
    WINSC_CAPSLOCK,
    WINSC_F1,
    WINSC_F2,
    WINSC_F3,
    WINSC_F4,
    WINSC_F5,
    WINSC_F6,
    WINSC_F7,
    WINSC_F8,
    WINSC_F9,
    WINSC_F10,
    WINSC_NUMLOCK, // Not extended in the docs, but extended in practice.
    WINSC_SCROLLLOCK,
    WINSC_NUMERIC7, // Extended: HOME
    WINSC_NUMERIC8, // Extended: UPARROW
    WINSC_NUMERIC9, // Extended: PAGEUP
    WINSC_NUMERICHYPHEN,
    WINSC_NUMERIC4, // Extended: LEFTARROW
    WINSC_NUMERIC5,
    WINSC_NUMERIC6, // Extended: RIGHTARROW
    WINSC_NUMERICPLUS,
    WINSC_NUMERIC1, // Extended: END
    WINSC_NUMERIC2, // Extended: DOWNARROW
    WINSC_NUMERIC3, // Extended: PAGEDOWN
    WINSC_NUMERIC0, // Extended: INSERT
    WINSC_NUMERICPERIOD, // Extended: DELETE
    WINSC_IGNORED1,
    WINSC_IGNORED2,
    WINSC_ANGLEBRACKET,
    WINSC_F11,
    WINSC_F12,
    WINSC_IGNORED3,
    WINSC_IGNORED4,
    WINSC_IGNORED5, // Extended: LWIN
    WINSC_IGNORED6, // Extended: RWIN
    WINSC_IGNORED7, // Extended: APPLICATION
    WINSC_IGNORED8, // Extended: ACPI Power
    WINSC_IGNORED9, // Extended: ACPI Sleep
    WINSC_IGNORED10,
    WINSC_IGNORED11,
    WINSC_IGNORED12,
    WINSC_IGNORED13, // Extended: ACPI Wake
    WINSC_IGNORED14,
    WINSC_IGNORED15,
    WINSC_IGNORED16,
    WINSC_F13,
    WINSC_F14,
    WINSC_F15,
    WINSC_F16,
    WINSC_F17,
    WINSC_F18,
    WINSC_F19,
    WINSC_F20,
    WINSC_F21,
    WINSC_F22,
    WINSC_F23,
    WINSC_IGNORED17,
    WINSC_INTERNATIONAL1,
    WINSC_IGNORED18,
    WINSC_IGNORED19,
    WINSC_INTERNATIONAL2,
    WINSC_IGNORED20,
    WINSC_IGNORED21,
    WINSC_IGNORED22,
    WINSC_INTERNATIONAL3,
    WINSC_IGNORED24,
    WINSC_INTERNATIONAL4,
    WINSC_IGNORED25,
    WINSC_INTERNATIONAL5,
    WINSC_IGNORED26,
    WINSC_IGNORED27,
    WINSC_INTERNATIONAL6,
    WINSC_IGNORED28,
};

static Game_Key winsc_to_gk[] = {
    
    GK_UNHANDLED,
    GK_ESCAPE,
    GK_1,
    GK_2,
    GK_3,
    GK_4,
    GK_5,
    GK_6,
    GK_7,
    GK_8,
    GK_9,
    GK_0,
    GK_MINUS,
    GK_EQUALS,
    GK_BACKSPACE,
    GK_TAB,
    GK_Q,
    GK_W,
    GK_E,
    GK_R,
    GK_T,
    GK_Y,
    GK_U,
    GK_I,
    GK_O,
    GK_P,
    GK_LEFTBRACKET,
    GK_RIGHTBRACKET,
    GK_ENTER,
    GK_LCTRL,
    GK_A,
    GK_S,
    GK_D,
    GK_F,
    GK_G,
    GK_H,
    GK_J,
    GK_K,
    GK_L,
    GK_SEMICOLON,
    GK_APOSTROPHE,
    GK_GRAVE,
    GK_LSHIFT,
    GK_BACKSLASH,
    GK_Z,
    GK_X,
    GK_C,
    GK_V,
    GK_B,
    GK_N,
    GK_M,
    GK_COMMA,
    GK_PERIOD,
    GK_SLASH,
    GK_RSHIFT,
    GK_NUMERICASTERISK,
    GK_LALT,
    GK_SPACEBAR,
    GK_CAPSLOCK,
    GK_F1,
    GK_F2,
    GK_F3,
    GK_F4,
    GK_F5,
    GK_F6,
    GK_F7,
    GK_F8,
    GK_F9,
    GK_F10,
    GK_NUMLOCK,
    GK_SCROLLLOCK,
    GK_NUMERIC7,
    GK_NUMERIC8,
    GK_NUMERIC9,
    GK_NUMERICHYPHEN,
    GK_NUMERIC4,
    GK_NUMERIC5,
    GK_NUMERIC6,
    GK_NUMERICPLUS,
    GK_NUMERIC1,
    GK_NUMERIC2,
    GK_NUMERIC3,
    GK_NUMERIC0,
    GK_NUMERICPERIOD,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_ANGLEBRACKET,
    GK_F11,
    GK_F12,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_F13,
    GK_F14,
    GK_F15,
    GK_F16,
    GK_F17,
    GK_F18,
    GK_F19,
    GK_F20,
    GK_F21,
    GK_F22,
    GK_F23,
    GK_UNHANDLED,
    GK_INTERNATIONAL1,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_INTERNATIONAL2,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_INTERNATIONAL3,
    GK_UNHANDLED,
    GK_INTERNATIONAL4,
    GK_UNHANDLED,
    GK_INTERNATIONAL5,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_INTERNATIONAL6,
    GK_UNHANDLED,
};

#elif OS_LINUX

enum Evdev_Keycode {
    EVDEV_MINIMUM = 8,
    EVDEV_1 = 10,
    EVDEV_2,
    EVDEV_3,
    EVDEV_4,
    EVDEV_5,
    EVDEV_6,
    EVDEV_7,
    EVDEV_8,
    EVDEV_9,
    EVDEV_0,
    EVDEV_MINUS,
    EVDEV_EQUALS,
    EVDEV_BACKSPACE,
    EVDEV_TAB,
    EVDEV_Q,
    EVDEV_W,
    EVDEV_E,
    EVDEV_R,
    EVDEV_T,
    EVDEV_Y,
    EVDEV_U,
    EVDEV_I,
    EVDEV_O,
    EVDEV_P,
    EVDEV_LEFTBRACKET,
    EVDEV_RIGHTBRACKET,
    EVDEV_RETURN,
    EVDEV_LCONTROL,
    EVDEV_A,
    EVDEV_S,
    EVDEV_D,
    EVDEV_F,
    EVDEV_G,
    EVDEV_H,
    EVDEV_J,
    EVDEV_K,
    EVDEV_L,
    EVDEV_SEMICOLON,
    EVDEV_APOSTROPHE,
    EVDEV_GRAVE,   
    EVDEV_LSHIFT,
    EVDEV_BACKSLASH,
    EVDEV_Z,
    EVDEV_X,
    EVDEV_C,
    EVDEV_V,
    EVDEV_B,
    EVDEV_N,
    EVDEV_M,
    EVDEV_COMMA,
    EVDEV_PERIOD,
    EVDEV_SLASH,
    EVDEV_RSHIFT,
    EVDEV_KEYPAD_MULTIPLY,
    EVDEV_LALT,
    EVDEV_SPACE,
    EVDEV_CAPSLOCK,
    EVDEV_F1,
    EVDEV_F2,
    EVDEV_F3,
    EVDEV_F4,
    EVDEV_F5,
    EVDEV_F6,
    EVDEV_F7,
    EVDEV_F8,
    EVDEV_F9,
    EVDEV_F10,
    EVDEV_NUMLOCK,
    EVDEV_SCROLLLOCK,
    EVDEV_KEYPAD_7,
    EVDEV_KEYPAD_8,
    EVDEV_KEYPAD_9,
    EVDEV_KEYPAD_MINUS,
    EVDEV_KEYPAD_4,
    EVDEV_KEYPAD_5,
    EVDEV_KEYPAD_6,
    EVDEV_KEYPAD_PLUS,
    EVDEV_KEYPAD_1,
    EVDEV_KEYPAD_2,
    EVDEV_KEYPAD_3,
    EVDEV_KEYPAD_0,
    EVDEV_KEYPAD_PERIOD,
    EVDEV_IGNORED1,
    EVDEV_IGNORED2,
    EVDEV_ANGLEBRACKET,
    EVDEV_F11,
    EVDEV_F12,
    EVDEV_IGNORED3,
    EVDEV_KATAKANA,
    EVDEV_HIRAGANA,
    EVDEV_HENKAN,
    EVDEV_KANA_TOGGLE,
    EVDEV_MUHENKAN,
    EVDEV_KPJPComma,
    EVDEV_KEYPAD_ENTER,
    EVDEV_RCONTROL,
    EVDEV_KEYPAD_DIVIDE,
    EVDEV_PRINTSCREEN,
    EVDEV_RALT,
    EVDEV_IGNORED4,
    EVDEV_HOME,
    EVDEV_UP,
    EVDEV_PAGEUP,
    EVDEV_LEFT,
    EVDEV_RIGHT,
    EVDEV_END,
    EVDEV_DOWN,
    EVDEV_PAGEDOWN,
    EVDEV_INSERT,
    EVDEV_DELETE,
    EVDEV_IGNORED5,
    EVDEV_MUTE,
    EVDEV_LOWER_VOLUME,
    EVDEV_INCREASE_VOLUME,
    EVDEV_POWER,
    EVDEV_KEYPAD_EQUALS,
    EVDEV_IGNORED6,
    EVDEV_PAUSE,
    EVDEV_IGNORED7,
    EVDEV_IGNORED8,
    EVDEV_HANGUL_LATIN_TOGGLE,
    EVDEV_HANGUL_HANJA_CONVERT,
    EVDEV_YEN,
    EVDEV_LWIN,
    EVDEV_RWIN,
    EVDEV_MENU, // 135
    EVDEV_MAXIMUM = 255,
};

static Game_Key evdev_to_gk[] = {
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_1,
    GK_2,
    GK_3,
    GK_4,
    GK_5,
    GK_6,
    GK_7,
    GK_8,
    GK_9,
    GK_0,
    GK_MINUS,
    GK_EQUALS,
    GK_BACKSPACE,
    GK_TAB,
    GK_Q,
    GK_W,
    GK_E,
    GK_R,
    GK_T,
    GK_Y,
    GK_U,
    GK_I,
    GK_O,
    GK_P,
    GK_LEFTBRACKET,
    GK_RIGHTBRACKET,
    GK_ENTER,
    GK_LCTRL,
    GK_A,
    GK_S,
    GK_D,
    GK_F,
    GK_G,
    GK_H,
    GK_J,
    GK_K,
    GK_L,
    GK_SEMICOLON,
    GK_APOSTROPHE,
    GK_GRAVE,
    GK_LSHIFT,
    GK_BACKSLASH,
    GK_Z,
    GK_X,
    GK_C,
    GK_V,
    GK_B,
    GK_N,
    GK_M,
    GK_COMMA,
    GK_PERIOD,
    GK_SLASH,
    GK_RSHIFT,
    GK_NUMERICASTERISK,
    GK_LALT,
    GK_SPACEBAR,
    GK_CAPSLOCK,
    GK_F1,
    GK_F2,
    GK_F3,
    GK_F4,
    GK_F5,
    GK_F6,
    GK_F7,
    GK_F8,
    GK_F9,
    GK_F10,
    GK_NUMLOCK,
    GK_SCROLLLOCK,
    GK_NUMERIC7,
    GK_NUMERIC8,
    GK_NUMERIC9,
    GK_NUMERICHYPHEN,
    GK_NUMERIC4,
    GK_NUMERIC5,
    GK_NUMERIC6,
    GK_NUMERICPLUS,
    GK_NUMERIC1,
    GK_NUMERIC2,
    GK_NUMERIC3,
    GK_NUMERIC0,
    GK_NUMERICPERIOD,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_ANGLEBRACKET,
    GK_F11,
    GK_F12,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_NUMERICENTER,
    GK_RCTRL,
    GK_UNHANDLED,
    GK_PRINTSCREEN,
    GK_RALT,
    GK_UNHANDLED,
    GK_HOME,
    GK_UPARROW,
    GK_PAGEUP,
    GK_LEFTARROW,
    GK_RIGHTARROW,
    GK_END,
    GK_DOWNARROW,
    GK_PAGEDOWN,
    GK_INSERT,
    GK_DELETE,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_UNHANDLED,
    GK_LSUPER,
    GK_RSUPER,
    GK_UNHANDLED,
};

#endif
