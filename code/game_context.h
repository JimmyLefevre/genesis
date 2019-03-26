
#include "common_context.h"

#define GAME_GET_API(name) Game_Interface name(OS_Export *os_import)
typedef GAME_GET_API(game_get_api);

#define GAME_INIT_MEMORY(name) void name(GAME_INIT_MEMORY_ARGS)
typedef GAME_INIT_MEMORY(game_init_memory);
#define GAME_RUN_FRAME(name) void name(GAME_RUN_FRAME_ARGS)
typedef GAME_RUN_FRAME(game_run_frame);

struct Implicit_Context {
    Memory_Block temporary_memory;
    u32 thread_index;
    
    void g_full_update(Game *, Input *, const f64, Audio_Info *, Datapack_Handle *);
    // void text_update(Text_Info *, Rendering_Info *);
    GAME_INIT_MEMORY(g_init_mem);
    GAME_RUN_FRAME(g_run_frame);
    void g_modify_textures(Game *, Rendering_Info *);
    inline void reset_temporary_memory();
    void test_thread_job(void *);
    s16 *update_audio(Audio_Info *, const s32, const f32);
    void draw_game(Rendering_Info *, Game *, Asset_Storage *);
    string tprint(char *, ...);
    void clone_game_state(Game *, Game *);
    void mix_samples(const s16 *, const s32, f32, const f32, f32 *, s32, f32 *, f32 *, f32 **, const u8);
    void flush_rasters_to_atlas(Text_Info *, u8 *, u32 **, v2s16 *, v2 *, s16, s16 *, s16 *, s16 *, v2s16, f32, f32);
    void files_convert_wav_to_facs(string *, s32, Memory_Block *, string *);
    string files_convert_bmp_to_ta(string *, v2 *, v2 *, u32, Memory_Block *);
    void draw_menu(Menu *, Game_Client *, Rendering_Info *, Text_Info *);
    void init_glyph_cache(Text_Info *);
    void draw_string_immediate(Text_Info *, Rendering_Info *, string, v2, f32, s8, bool, v4);
    void text_update(Text_Info *, Rendering_Info *);
    
#if GENESIS_DEV
    void profile_update(Profiler *, Program_State *, Text_Info *, Rendering_Info *, Input *);
#endif
};

#define CONTEXT_GAME_INIT_MEMORY(name) void (Implicit_Context::*name)(GAME_INIT_MEMORY_ARGS)
typedef CONTEXT_GAME_INIT_MEMORY(context_game_init_memory);
#define CONTEXT_GAME_RUN_FRAME(name) void (Implicit_Context::*name)(GAME_RUN_FRAME_ARGS)
typedef CONTEXT_GAME_RUN_FRAME(context_game_run_frame);


struct Game_Interface {
    void *os_handle;
    context_game_init_memory init_mem;
    context_game_run_frame run_frame;
};

#define OS_ADD_THREAD_JOB(name) void name(void (Implicit_Context::* function)(void *), void *data)
typedef OS_ADD_THREAD_JOB(os_add_thread_job);

#define THREAD_JOB_PROC(name) void Implicit_Context::name(void *data)
