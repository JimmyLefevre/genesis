
#define MAX_EVENT_COUNT 256

enum System_Event_Type {
    SYSTEM_EVENT_NONE,
    SYSTEM_EVENT_KEY,
    SYSTEM_EVENT_MOUSE_MOVE,
    SYSTEM_EVENT_WINDOW_RESIZE,
    SYSTEM_EVENT_FLUSH_INPUT,
};
struct Key_Event {
    u32 key;
    bool down;
};
struct Mouse_Move_Event {
    s32 move[2];
};
struct Window_Resize_Event {
    s32 new_dim[2];
};
struct System_Event {
    u32 type;
    union {
        Key_Event key;
        Mouse_Move_Event mouse_move;
        Window_Resize_Event window_resize;
    };
};

struct Event_Queue {
    System_Event events[MAX_EVENT_COUNT];
    u32 event_count;
};
