
static s32 index_count_or_zero(s32);
u16 Implicit_Context::load_mesh(Renderer *renderer, Render_Command_Queue *command_queue, string mesh_file) {
    Serialized_Render_Mesh *mesh = CAST(Serialized_Render_Mesh *, mesh_file.data);
    Render_Vertex *verts = CAST(Render_Vertex *, mesh + 1);
    u16 *indices = CAST(u16 *, verts + mesh->vertex_count);
    
    Render_Vertex *vertex_mapped;
    u16 *index_mapped;
    u16 handle = os_platform.make_editable_mesh(sizeof(Render_Vertex), mesh->vertex_count, CAST(u8 **, &vertex_mapped), CAST(u8 **, &index_mapped));
    
    s32 index_count = index_count_or_zero(mesh->vertex_count);
    
    mem_copy(verts, vertex_mapped, sizeof(Render_Vertex) * mesh->vertex_count);
    mem_copy(indices, index_mapped, sizeof(u16) * index_count);
    
    os_platform.update_editable_mesh(command_queue->command_list_handle, handle, index_count, true);
    
    return handle;
}

static inline void put_mesh(Renderer *renderer, u16 put_index, u16 handle) {
    ASSERT(put_index < Mesh_Uid::count);
    ASSERT(!renderer->mesh_handles[put_index]);
    
    renderer->mesh_handles[put_index] = handle;
}

static Mesh_Instance make_mesh_instance(rect2 rect, v4 color) {
    Mesh_Instance result;
    
    result.offset = rect2_center(rect);
    result.scale = rect2_dim(rect);
    result.rot = V2(1.0f, 0.0f);
    result.color = color;
    
    return result;
}

static inline u16 get_mesh_handle(Renderer *renderer, u16 mesh_index) {
    ASSERT(mesh_index < Mesh_Uid::count);
    return renderer->mesh_handles[mesh_index];
}
#define GET_MESH_HANDLE(renderer, name) get_mesh_handle(renderer, Mesh_Uid::##name)

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
        // @Incomplete: We either need to ExecuteCommandLists() and wait, or swap to another instance buffer.
        UNHANDLED;
    }
    
    MIRROR_VARIABLE(s32, command_count, &queue->draw_command_count);
    {
        if(command_count) {
            auto last_command = &queue->draw_commands[command_count - 1];
            
            if((last_command->type == Draw_Command_Type::INSTANCES) && (last_command->mesh_handle == mesh_handle)) {
                last_command->instance_count += instance_count;
            } else {
                auto new_command = &queue->draw_commands[command_count++];
                
                new_command->type = Draw_Command_Type::INSTANCES;
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

static void render_set_texture(Render_Command_Queue *queue, u16 id) {
    ASSERT(queue->draw_command_count < queue->instance_capacity);
    Draw_Command *command = &queue->draw_commands[queue->draw_command_count++];
    
    command->type = Draw_Command_Type::TEXTURE;
    command->texture_id = id;
}
static void render_set_vertex_shader(Render_Command_Queue *queue, u16 id) {
    ASSERT(queue->draw_command_count < queue->instance_capacity);
    Draw_Command *command = &queue->draw_commands[queue->draw_command_count++];
    
    command->type = Draw_Command_Type::VERTEX_SHADER;
    command->vertex_shader_id = id;
}
static void render_set_pixel_shader(Render_Command_Queue *queue, u16 id) {
    ASSERT(queue->draw_command_count < queue->instance_capacity);
    Draw_Command *command = &queue->draw_commands[queue->draw_command_count++];
    
    command->type = Draw_Command_Type::PIXEL_SHADER;
    command->pixel_shader_id = id;
}

static inline void render_meshes(Render_Command_Queue *queue, u16 mesh_handle, Mesh_Instance *instances, s32 instance_count) {
    Mesh_Instance *queue_instances = reserve_instances(queue, mesh_handle, instance_count);
    mem_copy(instances, queue_instances, sizeof(Mesh_Instance) * instance_count);
}
static inline void render_mesh(Render_Command_Queue *queue, u16 mesh_handle, Mesh_Instance *instance) {
    render_meshes(queue, mesh_handle, instance, 1);
}
static inline void render_quad(Renderer *renderer, Render_Command_Queue *queue, Mesh_Instance *instance) {
    render_meshes(queue, GET_MESH_HANDLE(renderer, quad), instance, 1);
}

static void render_quad_outline(Renderer *renderer, Render_Command_Queue *queue, Mesh_Instance *instance, f32 thickness) {
    Mesh_Instance outline[4];
    
    v2 center = instance->offset;
    v2 x_axis = v2_complex_prod(V2(instance->scale.x * 0.5f, 0.0f), instance->rot);
    v2 y_axis = v2_complex_prod(V2(0.0f, instance->scale.y * 0.5f), instance->rot);
    
    outline[0].offset = center - x_axis;
    outline[0].scale = V2(thickness, instance->scale.y);
    outline[0].rot = instance->rot;
    outline[0].color = instance->color;
    
    outline[1].offset = center - y_axis;
    outline[1].scale = V2(instance->scale.x, thickness);
    outline[1].rot = instance->rot;
    outline[1].color = instance->color;
    
    outline[2].offset = center + x_axis;
    outline[2].scale = V2(thickness, instance->scale.y);
    outline[2].rot = instance->rot;
    outline[2].color = instance->color;
    
    outline[3].offset = center + y_axis;
    outline[3].scale = V2(instance->scale.x, thickness);
    outline[3].rot = instance->rot;
    outline[3].color = instance->color;
    
    render_meshes(queue, GET_MESH_HANDLE(renderer, quad), outline, 4);
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
    os_platform.update_editable_mesh(command_list_handle, GET_MESH_HANDLE(renderer, color_picker), 6, false);
}

static void render_cursor(Renderer *renderer, Render_Command_Queue *command_queue, v2 p, v4 inner_color, bool render_outer) {
    Mesh_Instance cursor = {};
    
    cursor.offset = p;
    cursor.scale = V2(0.1f);
    cursor.rot = v2_angle_to_complex(TAU * 0.125f);
    cursor.color = V4(1.0f - inner_color.r, 1.0f - inner_color.g, 1.0f - inner_color.b, inner_color.a);
    
    if(render_outer) {
        render_quad(renderer, command_queue, &cursor);
        
        cursor.color = inner_color;
    }
    
    cursor.scale *= 0.35f;
    render_quad(renderer, command_queue, &cursor);
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
    
    {
        render_begin_frame_and_clear(renderer, V4(0.00f, 0.003f, 0.00f, 1.0f));
        
        Render_Command_Queue *command_queue = get_current_command_queue(renderer);
        render_set_transform_game_camera(command_queue->command_list_handle, V2(), V2(1.0f, 0.0f), 1.0f);
        
        
        { // Basic quad.
            s32 index_count = 6;
            Render_Vertex *vertex_mapped;
            u16 *index_mapped;
            u16 handle = os_platform.make_editable_mesh(sizeof(Render_Vertex), 4, CAST(u8 **, &vertex_mapped), CAST(u8 **, &index_mapped));
            ASSERT(handle == Mesh_Uid::quad);
            
            vertex_mapped[0] = make_render_vertex(V2(0.5f, 0.5f), V2(1.0f, 1.0f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[1] = make_render_vertex(V2(-0.5f, 0.5f), V2(0.0f, 1.0f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[2] = make_render_vertex(V2(-0.5f, -0.5f), V2(0.0f, 0.0f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[3] = make_render_vertex(V2(0.5f, -0.5f), V2(1.0f, 0.0f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            
            index_mapped[0] = 0;
            index_mapped[1] = 1;
            index_mapped[2] = 2;
            index_mapped[3] = 0;
            index_mapped[4] = 2;
            index_mapped[5] = 3;
            
            os_platform.update_editable_mesh(command_queue->command_list_handle, handle, index_count, true);
            
            put_mesh(renderer, Mesh_Uid::quad, handle);
        }
        
        { // Color picker.
            // @Hack: Not sure this is even remotely approximating HSV, but it's enough for now.
            s32 index_count = 6;
            Render_Vertex *vertex_mapped;
            u16 *index_mapped;
            u16 handle = os_platform.make_editable_mesh(sizeof(Render_Vertex), 4, CAST(u8 **, &vertex_mapped), CAST(u8 **, &index_mapped));
            
            vertex_mapped[0] = make_render_vertex(V2(0.5f, 0.5f), V2(1.0f, 1.0f), V4(1.0f, 0.0f, 0.0f, 1.0f));
            vertex_mapped[1] = make_render_vertex(V2(-0.5f, 0.5f), V2(0.0f, 1.0f), V4(1.0f, 1.0f, 1.0f, 1.0f));
            vertex_mapped[2] = make_render_vertex(V2(-0.5f, -0.5f), V2(0.0f, 0.0f), V4(0.0f, 0.0f, 0.0f, 1.0f));
            vertex_mapped[3] = make_render_vertex(V2(0.5f, -0.5f), V2(1.0f, 0.0f), V4(0.0f, 0.0f, 0.0f, 1.0f));
            
            index_mapped[0] = 0;
            index_mapped[1] = 1;
            index_mapped[2] = 2;
            index_mapped[3] = 0;
            index_mapped[4] = 2;
            index_mapped[5] = 3;
            
            renderer->color_picker_color = &vertex_mapped[0].color;
            
            os_platform.update_editable_mesh(command_queue->command_list_handle, handle, index_count, false);
            
            put_mesh(renderer, Mesh_Uid::color_picker, handle);
        }
        
        { // Hue picker.
            // We could make an HSV shader instead of lerping through all the hues, but this works fine.
            v4 hues[] = {
                V4(1.0f, 0.0f, 0.0f, 1.0f),
                V4(1.0f, 1.0f, 0.0f, 1.0f),
                V4(0.0f, 1.0f, 0.0f, 1.0f),
                V4(0.0f, 1.0f, 1.0f, 1.0f),
                V4(0.0f, 0.0f, 1.0f, 1.0f),
                V4(1.0f, 0.0f, 1.0f, 1.0f),
                V4(1.0f, 0.0f, 0.0f, 1.0f),
            };
            
            Render_Vertex *vertex_mapped;
            u16 *index_mapped;
            u16 handle = os_platform.make_editable_mesh(sizeof(Render_Vertex), 2 * ARRAY_LENGTH(hues), CAST(u8 **, &vertex_mapped), CAST(u8 **, &index_mapped));
            
            s32 index_count = 0;
            {
                f32 at_y = -0.5f;
                f32 y_step = 1.0f / (ARRAY_LENGTH(hues) - 1);
                vertex_mapped[0] = make_render_vertex(V2(-0.5f, at_y), V2(0.0f, (at_y + 0.5f)), hues[0]);
                vertex_mapped[1] = make_render_vertex(V2(0.5f, at_y), V2(1.0f, (at_y + 0.5f)), hues[0]);
                
                u16 running_vertex_index = 2;
                s32 running_index_index = 0;
                FORI_NAMED(hue_index, 1, ARRAY_LENGTH(hues)) {
                    at_y += y_step;
                    
                    vertex_mapped[running_vertex_index + 0] = make_render_vertex(V2(-0.5f, at_y), V2(0.0f, (at_y + 0.5f)), hues[hue_index]);
                    vertex_mapped[running_vertex_index + 1] = make_render_vertex(V2(0.5f, at_y), V2(1.0f, (at_y + 0.5f)), hues[hue_index]);
                    
                    index_mapped[running_index_index + 0] = running_vertex_index - 2;
                    index_mapped[running_index_index + 1] = running_vertex_index - 1;
                    index_mapped[running_index_index + 2] = running_vertex_index + 1;
                    index_mapped[running_index_index + 3] = running_vertex_index - 2;
                    index_mapped[running_index_index + 4] = running_vertex_index + 1;
                    index_mapped[running_index_index + 5] = running_vertex_index + 0;
                    
                    running_vertex_index += 2;
                    running_index_index += 6;
                }
                
                index_count = running_index_index;
            }
            
            os_platform.update_editable_mesh(command_queue->command_list_handle, handle, index_count, true);
            
            put_mesh(renderer, Mesh_Uid::hue_picker, handle);
        }
    }
}
