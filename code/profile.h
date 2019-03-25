
#if GENESIS_DEV
//
// Timing:
//

#define DEBUG_TIMERS_PER_CPU 256

struct Debug_Timer {
    u64 start_time;
    u64 end_time;
    string location;
    usize location_function_offset;
    usize location_line_offset;
};

// Aligning to cache lines.
#pragma pack(push, 1)
struct Debug_Timer_Queue {
    u8 padding[64 - sizeof(u32)];
    u32 count;
    Debug_Timer timers[DEBUG_TIMERS_PER_CPU];
};
#pragma pack(pop)

struct Profiler {
    Debug_Timer_Queue *timer_queues;
    s16 thread_count;
    s16 frame_index;
    
    s16 selected_frame;
    
    bool should_profile;
    bool has_focus;
    
    f32 target_alpha;
    f32 current_alpha;
    f32 dalpha;
};

static Profiler *global_profiler;

struct Block_Timer {
    Debug_Timer timer;
    u32 id;
    u32 thread_index;
    
    Block_Timer(u32 id, string location, usize location_function_offset, usize location_line_offset, u32 thread_index) {
        this->id = id;
        this->thread_index = thread_index;
        timer.location = location;
        timer.end_time = 0;
        timer.location_function_offset = location_function_offset;
        timer.location_line_offset = location_line_offset;
        
        timer.start_time = READ_CPU_CLOCK();
    }
    
    ~Block_Timer() {
        if(global_profiler->should_profile) {
            timer.end_time = READ_CPU_CLOCK();
            
            Debug_Timer_Queue *queue = global_profiler->timer_queues + global_profiler->frame_index * global_profiler->thread_count + thread_index;
            
            u32 add = queue->count;
            
            ASSERT(add < DEBUG_TIMERS_PER_CPU);
            queue->timers[add] = timer;
            queue->count = add + 1;
        }
    }
};

#define __TIME_BLOCK(counter, location, function_offset, line_offset, thread_index) Block_Timer _block_timer##counter(counter, location, function_offset, line_offset, thread_index)
#define _TIME_BLOCK(counter, file, function, line, thread_index) __TIME_BLOCK(counter, STRING(file function line), sizeof(file) - 1, sizeof(file) + sizeof(function) - 2, thread_index)
#define TIME_BLOCK _TIME_BLOCK(__COUNTER__, __FILE__, __FUNCTION__, STRINGISE(__LINE__), thread_index)
#else
#define TIME_BLOCK
#endif
