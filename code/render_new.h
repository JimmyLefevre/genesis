
#define MAX_QUAD_COUNT 4096
#define MAX_VERT_COUNT (MAX_QUAD_COUNT * 6)

// @Hardcoded
#define VERTEX_BUFFER_SIZE 256
#define INSTANCE_BUFFER_SIZE 256

ENUM(RESERVED_MESH_HANDLE) {
    QUAD = 0,
    COLOR_PICKER,
    HUE_PICKER,
};

struct Mesh_Instance;
struct Draw_Command;
struct Render_Command_Queue {
    u16 command_list_handle;
    
    u16 instance_buffer_handle;
    Mesh_Instance *instances;
    s32 instance_count;
    s32 instance_capacity;
    
    Draw_Command *draw_commands; // Draw_Command[instance_capacity]
    s32 draw_command_count;
};

#define MAX_RENDER_COMMAND_QUEUE_COUNT 8
struct Renderer {
    u32 current_frame_index;
    
    v2 render_target_dim;
    u8 core_count;
    
    u16 quad_mesh_handle;
    u16 color_picker_mesh_handle;
    v4 *color_picker_color;
    
    Render_Command_Queue command_queues[8];
    s32 command_queue_count;
    
    // Threading
    u32 render_jobs_issued;
    volatile s32 render_jobs_finished;
    volatile s32 command_batches_queued;
    volatile s32 command_flushes;
};
