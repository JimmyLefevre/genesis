
static Mesh_Instance make_mesh_instance(rect2 rect, v4 color) {
    Mesh_Instance result;
    
    result.offset = rect2_center(rect);
    result.scale = rect2_dim(rect);
    result.rot = V2(1.0f, 0.0f);
    result.color = color;
    
    return result;
}

static inline void flush_draw_commands(Render_Command_Queue *queue) {
    ASSERT(queue->instance_count > 0);
    
    os_platform.draw_mesh_instances(queue->command_list_handle,
                                    queue->instance_buffer_handle,
                                    queue->draw_commands,
                                    queue->draw_command_count);
    
    queue->draw_command_count = 0;
    queue->instance_count = 0;
}
static inline void maybe_flush_draw_commands(Render_Command_Queue *queue) {
    if(queue->instance_count > 0) {
        flush_draw_commands(queue);
    }
}

static Mesh_Instance *reserve_instances(Render_Command_Queue *queue, u16 mesh_handle, s32 instance_count) {
    MIRROR_VARIABLE(s32, current_instance_count, &queue->instance_count);
    
    s32 new_instance_count = current_instance_count + instance_count;
    if(new_instance_count >= queue->instance_capacity) {
        flush_draw_commands(queue);
        new_instance_count = 0;
        queue->draw_command_count = 0;
        // @Incomplete: We either need to ExecuteCommandLists() and wait, or to swap to another instance buffer.
        UNHANDLED;
    }
    
    MIRROR_VARIABLE(s32, command_count, &queue->draw_command_count);
    {
        if(command_count) {
            auto last_command = &queue->draw_commands[command_count - 1];
            if(last_command->mesh_handle == mesh_handle) {
                last_command->instance_count += instance_count;
            } else {
                auto new_command = &queue->draw_commands[command_count++];
                
                new_command->mesh_handle = mesh_handle;
                new_command->instance_count = instance_count;
            }
        } else {
            queue->draw_commands[0].mesh_handle = mesh_handle;
            queue->draw_commands[0].instance_count = instance_count;
            command_count = 1;
        }
    }
    
    Mesh_Instance *result = &queue->instances[current_instance_count];
    current_instance_count = new_instance_count;
    return result;
}

static inline void render_meshes(Render_Command_Queue *queue, u16 mesh_handle, Mesh_Instance *instances, s32 instance_count) {
    Mesh_Instance *queue_instances = reserve_instances(queue, mesh_handle, instance_count);
    mem_copy(instances, queue_instances, sizeof(Mesh_Instance) * instance_count);
}
static inline void render_mesh(Render_Command_Queue *queue, u16 mesh_handle, Mesh_Instance *instance) {
    render_meshes(queue, mesh_handle, instance, 1);
}
static inline void render_quad(Render_Command_Queue *queue, Mesh_Instance *instance) {
    render_meshes(queue, RESERVED_MESH_HANDLE::QUAD, instance, 1);
}
static inline void render_quad(Render_Command_Queue *queue, Mesh_Instance instance) {
    render_meshes(queue, RESERVED_MESH_HANDLE::QUAD, &instance, 1);
}
static inline void render_quads(Render_Command_Queue *queue, Mesh_Instance *instance, s32 quad_count) {
    render_meshes(queue, RESERVED_MESH_HANDLE::QUAD, instance, quad_count);
}

static void render_set_transform_game_camera(u16 handle, v2 p, v2 rot, f32 zoom) {
    f32 x_aspect_scale = (1.0f / (GAME_ASPECT_RATIO_X * 0.5f));
    f32 y_aspect_scale = (1.0f / (GAME_ASPECT_RATIO_Y * 0.5f));
    f32 transform[16] = {
        x_aspect_scale * rot.x, y_aspect_scale * rot.y, 0.0f, 0.0f,
        x_aspect_scale * -rot.y, y_aspect_scale * rot.x, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -(p.x * transform[0] + p.y * transform[1]), -(p.x * transform[4] + p.y * transform[5]), 0.0f, (zoom) ? (1.0f / zoom) : 0.0f,
    };
    
    os_platform.set_transform(handle, transform);
}

static void render_set_transform_right_handed_unit_scale(u16 handle) {
    f32 transform[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
    };
    
    os_platform.set_transform(handle, transform);
}

void Implicit_Context::render_init(Renderer* renderer, Memory_Block* block, u8 core_count) {
    SCOPE_MEMORY(&temporary_memory);
    renderer->core_count = core_count;
    
    s32 command_batch_count = 2; // :OneCommandListPerFrame
    
    auto command_list_handles     = push_array(&temporary_memory, u16, command_batch_count);
    
    auto vertex_buffers           = push_array(&temporary_memory, Make_Vertex_Buffer_Output, command_batch_count);
    auto vertex_buffer_vert_sizes = push_array(&temporary_memory, s32, command_batch_count);
    auto vertex_buffer_lengths    = push_array(&temporary_memory, s32, command_batch_count);
    
    FORI(0, command_batch_count) {
        vertex_buffer_vert_sizes[i] = sizeof(Render_Vertex);
        vertex_buffer_lengths[i]    = VERTEX_BUFFER_SIZE;
    }
    
    
    auto instance_buffers           = push_array(&temporary_memory, Make_Vertex_Buffer_Output, command_batch_count);
    auto instance_buffer_vert_sizes = push_array(&temporary_memory, s32, command_batch_count);
    auto instance_buffer_lengths    = push_array(&temporary_memory, s32, command_batch_count);
    
    FORI(0, command_batch_count) {
        instance_buffer_vert_sizes[i] = sizeof(Mesh_Instance);
        instance_buffer_lengths[i]    = INSTANCE_BUFFER_SIZE;
    }
    
    
    os_platform.make_command_lists(command_list_handles, command_batch_count);
    os_platform.make_vertex_buffers(vertex_buffers, vertex_buffer_vert_sizes, vertex_buffer_lengths, command_batch_count);
    os_platform.make_vertex_buffers(instance_buffers, instance_buffer_vert_sizes, instance_buffer_lengths, command_batch_count);
    
    // Frame state:
    FORI(0, command_batch_count) {
        auto queue = &renderer->command_queues[i];
        auto instance_buffer = &instance_buffers[i];
        
        queue->command_list_handle = command_list_handles[i];
        
        queue->instance_buffer_handle = instance_buffer->handle;
        queue->instances = CAST(Mesh_Instance *, instance_buffer->mapped_buffer);
        queue->instance_capacity = instance_buffer_lengths[i];
        queue->draw_commands = push_array(block, Draw_Command, queue->instance_capacity);
    }
    
    renderer->command_queue_count = command_batch_count;
}

static inline Render_Command_Queue *get_current_command_queue(Renderer *renderer) {
    return &renderer->command_queues[renderer->current_frame_index & 1]; // :OneCommandListPerFrame
}

static inline void render_begin_frame_and_clear(Renderer *renderer, v4 clear_color) {
    os_platform.begin_frame_and_clear(get_current_command_queue(renderer)->command_list_handle, CAST(u16, renderer->current_frame_index), clear_color);
}

static void render_end_frame(Renderer* renderer) {
    renderer->current_frame_index += 1;
    os_platform.end_frame();
}

static void set_color_picker_hue(Renderer *renderer, u16 command_list_handle, v4 hue) {
    *renderer->color_picker_color = hue;
    os_platform.update_editable_mesh(command_list_handle, RESERVED_MESH_HANDLE::COLOR_PICKER, 6, false);
}
