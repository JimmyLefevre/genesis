
// Basic functions(GL1-2) that we are expecting to get with GetProcAddress:
static const char *opengl_base_functions[] = {
    "glBindTexture",
    "glBlendFunc",
    "glClear",
    "glClearColor",
    "glDrawArrays",
    "glEnable",
    "glGenTextures",
    "glTexImage2D",
    "glTexParameteri",
    "glViewport",
    "glGetString",
    "glBegin",
    "glEnd",
    "glColor4f",
    "glVertex2f",
    "glDisable",
    "glTexSubImage2D",
};

// GL extension functions that we try to get with wglGetProcAddress:
static const char * const opengl_extended_functions[] = {
    "glEnableVertexAttribArray",
    "glDisableVertexAttribArray",
    "glGetAttribLocation",
    "glVertexAttribPointer",
    "glShaderSource",
    "glCompileShader",
    "glCreateShader",
    "glCreateProgram",
    "glAttachShader",
    "glLinkProgram",
    "glValidateProgram",
    "glGetProgramiv",
    "glGetShaderInfoLog",
    "glGetProgramInfoLog",
    "glUseProgram",
    "glUniformMatrix2fv",
    "glUniformMatrix4fv",
    "glGetUniformLocation",
    "glUniform1i",
    "glDebugMessageCallback",
    "glDebugMessageControl",
    "glGenVertexArrays",
    "glBindVertexArray",
    "glGenBuffers",
    "glBindBuffer",
    "glBufferData",
    "glActiveTexture",
    "glGenFramebuffers",
    "glBindFramebuffer",
    "glFramebufferTexture2D",
    "glDeleteShader",
};

#define OS_GET_SECS(name) f64 name()
typedef OS_GET_SECS(os_get_secs);
#define OS_MEM_ALLOC(name) void *name(usize size)
typedef OS_MEM_ALLOC(os_mem_alloc);
#define OS_OPEN_FILE(name) void *name(string file_name)
typedef OS_OPEN_FILE(os_open_file);
#define OS_READ_FILE(name) bool name(void *handle, void *out, usize offset, usize length)
typedef OS_READ_FILE(os_read_file);
#define OS_FILE_SIZE(name) usize name(void *handle)
typedef OS_FILE_SIZE(os_file_size);
#define OS_READ_ENTIRE_FILE(name) string name(string file_name)
typedef OS_READ_ENTIRE_FILE(os_read_entire_file);
#define OS_WRITE_ENTIRE_FILE(name) bool name(string file_name, string file_data)
typedef OS_WRITE_ENTIRE_FILE(os_write_entire_file);
#define OS_BEGIN_SOUND_UPDATE(name) u32 name()
typedef OS_BEGIN_SOUND_UPDATE(os_begin_sound_update);
#define OS_END_SOUND_UPDATE(name) void name(s16 *input_buffer, u32 samples_to_update)
typedef OS_END_SOUND_UPDATE(os_end_sound_update);
#define OS_CAPTURE_CURSOR(name) void name()
typedef OS_CAPTURE_CURSOR(os_capture_cursor);
#define OS_RELEASE_CURSOR(name) void name()
typedef OS_RELEASE_CURSOR(os_release_cursor);
#define OS_GET_CURSOR_POSITION_IN_PIXELS(name) void name(s32 *out_x, s32 *out_y)
typedef OS_GET_CURSOR_POSITION_IN_PIXELS(os_get_cursor_position_in_pixels);
#define OS_PRINT(name) void name(string text)
typedef OS_PRINT(os_print);
#define OS_CLOSE_FILE(name) void name(void *handle)
typedef OS_CLOSE_FILE(os_close_file);

struct os_function_interface {
    os_get_secs *get_seconds;
    os_mem_alloc *mem_alloc;
    os_open_file *open_file;
    os_close_file *close_file;
    os_read_file *read_file;
    os_file_size *file_size;
    os_read_entire_file *read_entire_file;
    os_write_entire_file *write_entire_file;
    os_begin_sound_update *begin_sound_update;
    os_end_sound_update *end_sound_update;
    os_capture_cursor *capture_cursor;
    os_release_cursor *release_cursor;
    os_print *print;
    os_add_thread_job *add_thread_job;
    
    
#if GENESIS_BUILD_ASSETS_ON_STARTUP
    string debug_asset_directory;
#endif
    
    
};

struct opengl_function_interface {
    union {
        struct {
            void *base_functions[ARRAY_LENGTH(opengl_base_functions)];
            void *extended_functions[ARRAY_LENGTH(opengl_extended_functions)];
        };
        void *all_functions[ARRAY_LENGTH(opengl_base_functions) + ARRAY_LENGTH(opengl_extended_functions)];
    };
};

struct OS_Export {
    os_function_interface os;
    opengl_function_interface opengl;
};

struct Program_State {                // Updated by...
    s32 core_count;                   // Platform
    
    s32 audio_output_rate;            // Platform
    s32 audio_output_channels;        // Platform
    
    Input input;                      // Platform
    bool should_be_fullscreen;        // Either ;Settings
    s32 window_size[2];               // Either ;Settings
    s32 draw_rect[4];                 // Game
    s32 cursor_position_in_pixels[2]; // Platform
    bool should_render;               // Platform
    
    Input_Settings input_settings;    // Game ;Settings
    
    u32 last_key_pressed;             // Platform
    
    bool should_close;                // Either
};
