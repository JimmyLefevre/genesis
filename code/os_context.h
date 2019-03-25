
#include "common_context.h"

struct Implicit_Context {
    Memory_Block temporary_memory;
    u32 thread_index;
};

typedef Game_Interface (*game_get_api)(OS_Export *os_import);
typedef void (*context_game_init_memory)(Implicit_Context *_this, GAME_INIT_MEMORY_ARGS);
typedef void (*context_game_run_frame)(Implicit_Context *_this, GAME_RUN_FRAME_ARGS);
struct Game_Interface {
    void *os_handle;
    context_game_init_memory init_mem;
    context_game_run_frame run_frame;
};

#define OS_ADD_THREAD_JOB(name) void name(void (*function)(Implicit_Context *_this, void *), void *data)
typedef OS_ADD_THREAD_JOB(os_add_thread_job);

#define THREAD_JOB_PROC(name) void name(Implicit_Context *_this, void *data)
