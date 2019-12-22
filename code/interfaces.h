
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

struct Make_Vertex_Buffer_Output {
    u16 handle;
    u8* mapped_buffer;
};

struct Mesh_Instance {
    v2 offset;
    v2 scale;
    v2 rot;
    v4 color;
};

ENUM(Draw_Command_Type) {
    NONE = 0,
    
    INSTANCES,
    TEXTURE,
    VERTEX_SHADER,
    PIXEL_SHADER,
    
    COUNT,
};

struct Draw_Command {
    u8 type;
    union {
        struct {
            u16 mesh_handle;
            s32 instance_count;
        };
        
        struct {
            u16 texture_id;
        };
        
        struct {
            u16 vertex_shader_id;
        };
        
        struct {
            u16 pixel_shader_id;
        };
    };
};

struct Render_Vertex {
    v2 p;
    v2 uv;
    v4 color;
};
static inline Render_Vertex make_render_vertex(v2 p, v2 uv, v4 color) {
    Render_Vertex result;
    result.p = p;
    result.uv = uv;
    result.color = color;
    return result;
}

#define GPU_BEGIN_FRAME_AND_CLEAR(name) void name(u16 list_handle, u16 frame_index, v4 clear_color)
typedef GPU_BEGIN_FRAME_AND_CLEAR(gpu_begin_frame_and_clear);
#define GPU_END_FRAME(name) void name()
typedef GPU_END_FRAME(gpu_end_frame);
#define GPU_MAKE_COMMAND_LISTS(name) void name(u16 *out, s32 count)
typedef GPU_MAKE_COMMAND_LISTS(gpu_make_command_lists);
#define GPU_MAKE_VERTEX_BUFFERS(name) void name(Make_Vertex_Buffer_Output *out, s32 *vert_sizes, s32 *lengths, s32 count)
typedef GPU_MAKE_VERTEX_BUFFERS(gpu_make_vertex_buffers);
#define GPU_SET_TRANSFORM(name) void name(u16 list_handle, f32 *transform)
typedef GPU_SET_TRANSFORM(gpu_set_transform);
#define GPU_SUBMIT_COMMANDS(name) void name(u16 *list_handles, s32 list_count, bool end_frame)
typedef GPU_SUBMIT_COMMANDS(gpu_submit_commands);
#define GPU_DRAW_MESH_INSTANCES(name) void name(u16 list_handle, u16 instance_buffer_handle, Draw_Command *commands, s32 command_count)
typedef GPU_DRAW_MESH_INSTANCES(gpu_draw_mesh_instances);
#define GPU_MAKE_EDITABLE_MESH(name) u16 name(s32 vertex_size, s32 vertex_capacity, u8 **vertex_mapped, u8 **index_mapped)
typedef GPU_MAKE_EDITABLE_MESH(gpu_make_editable_mesh);
#define GPU_UPDATE_EDITABLE_MESH(name) void name(u16 list_handle, u16 editable_handle, s32 index_count, bool make_read_only)
typedef GPU_UPDATE_EDITABLE_MESH(gpu_update_editable_mesh);
#define GPU_UPDATE_TEXTURE_RGBA(name) void name(u16 id, u8 *data, v2s dim, v2s bottom_left)
typedef GPU_UPDATE_TEXTURE_RGBA(gpu_update_texture_rgba);
#define GPU_UPLOAD_TEXTURE_TO_GPU_RGBA8(name) void name(u16 list_handle, u16 index, u8 *data, v2s dim)
typedef GPU_UPLOAD_TEXTURE_TO_GPU_RGBA8(gpu_upload_texture_to_gpu_rgba8);

struct os_function_interface {
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
    
    gpu_begin_frame_and_clear *begin_frame_and_clear;
    gpu_end_frame *end_frame;
    gpu_make_command_lists *make_command_lists;
    gpu_make_vertex_buffers *make_vertex_buffers;
    gpu_set_transform *set_transform;
    gpu_submit_commands *submit_commands;
    gpu_draw_mesh_instances *draw_mesh_instances;
    gpu_make_editable_mesh *make_editable_mesh;
    gpu_update_editable_mesh *update_editable_mesh;
    gpu_update_texture_rgba *update_texture_rgba;
    gpu_upload_texture_to_gpu_rgba8 *upload_texture_to_gpu_rgba8;
};

ENUM(Texture_Id) {
    BLANK = 0,
    UNTEXTURED,
    FONT_ATLAS,
    
    COUNT,
};

ENUM(Shader_Id) {
    STANDARD_VERTEX = 0,
    STANDARD_PIXEL,
    
    COUNT,
};

ENUM(Vertex_Buffer_Id) {
    P_COLOR_INDEX,
    
    COUNT,
};

ENUM(Constant_Buffer_Id) {
    VERTEX_POSITION,
    VERTEX_COLOR,
    
    COUNT,
};

struct OS_Export {
    os_function_interface os;
};

struct Program_State {                // Updated by...
    s32 core_count;                   // Platform
    f64 time;
    
    s32 audio_output_rate;            // Platform
    s32 audio_output_channels;        // Platform
    
    Input input;                      // Platform
    bool should_be_fullscreen;        // Either ;Settings
    v2s window_size;                  // Either ;Settings
    rect2s draw_rect;                 // Platform
    v2 cursor_position_barycentric;   // Platform
    bool should_render;               // Platform
    
    Input_Settings input_settings;    // Game ;Settings
    
    u32 last_key_pressed;             // Platform
    
    bool should_close;                // Either
};
