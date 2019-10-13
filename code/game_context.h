
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
    GAME_INIT_MEMORY(g_init_mem);
    GAME_RUN_FRAME(g_run_frame);
    s16 *update_audio(Audio_Info *, const s32, const f32);
    void load_chunk(Loaded_Sound *, s16 *, s32, s32);
    inline void reset_temporary_memory();
    void draw_game_single_threaded(Renderer *, Render_Command_Queue *, Game *, Asset_Storage *);
    string tprint(char *, ...);
    void clone_game_state(Game *, Game *);
    void mix_samples(const s16 *, const s32, f32, const f32, f32 *, s32, f32 *, f32 *, f32 **, const u8);
    void flush_rasters_to_atlas(Text_Info *, u8 *, u32 **, v2s16 *, v2 *, s16, s16 *, s16 *, s16 *, v2s16, f32, f32);
    void files_convert_wav_to_facs(string *, s32, Memory_Block *, string *);
    void files_convert_wav_to_uvga(string *, s32, Memory_Block *, string *);
    string files_convert_bmp_to_ta(string *, v2 *, v2 *, u32, Memory_Block *);
    void init_glyph_cache(Text_Info *);
    void draw_string_immediate(Text_Info *, Renderer *, string, v2, f32, s8, bool, v4);
    void text_update(Text_Info *, Renderer *);
    void render_init(Renderer*, Memory_Block*, u8);
    s16* update_synth(Synth*, const s32);
    u16 load_mesh(Renderer *, Render_Command_Queue *, string);
    
    void mesh_update(Mesh_Editor *, Renderer *, Input *, Render_Command_Queue *);
    
    void export_mesh(Edit_Mesh *);
    
    void draw_game_threaded(void *);
    void test_thread_job(void *);
    void entire_sound_update(void *);
    void load_unbatched_first_audio_chunk_job(void *);
    void load_batched_first_audio_chunk_job(void *);
    void load_unbatched_next_audio_chunk_job(void *);
    
#if GENESIS_DEV
    void profile_update(Profiler *, Program_State *, Text_Info *, Renderer *, Input *);
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
