
static void post_key_event(s32 key, bool down, Event_Queue *events) {
    
    if(events->event_count < MAX_EVENT_COUNT) {
        
        System_Event *event = events->events + events->event_count++;
        event->type = SYSTEM_EVENT_KEY;
        event->key.key = key;
        event->key.down = down;
    }
}
static void post_mouse_move_event(s32 dm[2], Event_Queue *events) {
    
    if(events->event_count < MAX_EVENT_COUNT) {
        
        System_Event *event = events->events + events->event_count++;
        event->type = SYSTEM_EVENT_MOUSE_MOVE;
        event->mouse_move.move[0] = dm[0];
        event->mouse_move.move[1] = dm[1];
    }
}
static void post_window_resize_event(s32 new_dim[2], Event_Queue *events) {
    
    if(events->event_count < MAX_EVENT_COUNT) {
        
        System_Event *event = events->events + events->event_count++;
        event->type = SYSTEM_EVENT_WINDOW_RESIZE;
        event->window_resize.new_dim[0] = new_dim[0];
        event->window_resize.new_dim[1] = new_dim[1];
    }
}
