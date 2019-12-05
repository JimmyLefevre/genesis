
static void mesh_init(Memory_Block *block, Mesh_Editor *editor) {
    sub_block(&editor->undo_buffer.block, block, KiB(1));
    
    FORI(0, MAX_EDIT_MESH_COUNT) {
        Edit_Mesh *mesh = &editor->meshes[i];
        
        mesh->layers = push_array(block, Edit_Mesh_Layer, MAX_LAYERS_PER_MESH);
        FORI_NAMED(layer_index, 0, MAX_LAYERS_PER_MESH) {
            Edit_Mesh_Layer *layer = &mesh->layers[layer_index];
            
            layer->verts = push_array(block, v2, MAX_VERTS_PER_LAYER);
            layer->indices = push_array(block, u16, MAX_INDICES_PER_LAYER);
        }
    }
}

static void edit_mesh_init(Mesh_Editor *editor, Edit_Mesh *mesh) {
    *mesh = {};
    mesh->layer_count = 1;
    mesh->output = editor->meshes[0].output; // @Hack!
}

static u16 add_mesh(Mesh_Editor *editor, Output_Mesh *output, s32 vert_capacity, v4 first_layer_color) {
    s32 add = editor->mesh_count++;
    ASSERT(add < MAX_EDIT_MESH_COUNT);
    
    auto mesh = &editor->meshes[add];
    mesh->layer_count = 1;
    
    auto first_layer = &mesh->layers[0];
    first_layer->vert_count = 0;
    
    mesh->output = *output;
    
    FORI(0, output->vert_count) {
        first_layer->verts[i] = output->vertex_mapped[i].p;
    }
    
    first_layer->vert_count = output->vert_count;
    first_layer->color = first_layer_color;
    
    return CAST(u16, add);
}

static inline v2 do_snap_to_grid(v2 p) {
    v2 result;
    
    result.x = f_round_to_f(p.x, DRAG_SNAP_GRANULARITY);
    result.y = f_round_to_f(p.y, DRAG_SNAP_GRANULARITY);
    
    return result;
}

// We have two current_layer values because mesh->current_layer can get updated mid-frame, which we don't want to deal
// with for robustness purposes.
static v2 snap_vertex(Edit_Mesh *mesh, v2 vertex, s32 current_layer_index, bool snap_to_grid, bool snap_to_edge) {
    v2 result = vertex;
    
    if(snap_to_edge) {
        v2 best_closest = V2();
        f32 best_distance_sq = LARGE_FLOAT;
        
        FORI_NAMED(other_layer_index, 0, mesh->layer_count) {
            if(other_layer_index != current_layer_index) {
                auto other_layer = &mesh->layers[other_layer_index];
                
                if(other_layer->vert_count > 1) {
                    v2 other_a = other_layer->verts[other_layer->vert_count - 1];
                    FORI_NAMED(other_b_index, 0, other_layer->vert_count) {
                        v2 other_b = other_layer->verts[other_b_index];
                        
                        v2 direction = other_b - other_a;
                        f32 length = v2_length(direction);
                        direction /= length;
                        f32 dot = v2_inner(result - other_a, direction);
                        dot = f_clamp(dot, 0.0f, length);
                        
                        if(snap_to_grid) {
                            // Snap along the edge.
                            dot = f_round_to_f(dot, DRAG_SNAP_GRANULARITY);
                        }
                        
                        v2 closest = other_a + direction * dot;
                        f32 dist_sq = v2_length_sq(closest - result);
                        if(best_distance_sq > dist_sq) {
                            best_distance_sq = dist_sq;
                            best_closest = closest;
                        }
                        
                        other_a = other_b;
                    }
                }
            }
        }
        
        if(best_distance_sq < (0.15f * 0.15f)) {
            result = best_closest;
            
            return result;
        }
    }
    
    if(snap_to_grid) {
        // If we didn't snap to an edge, snap to the grid.
        result = do_snap_to_grid(result);
    }
    
    return result;
}

// @Speed?
// This is just testing whether the polygon would be degenerate after moving the vert.
// I'm not sure we cover all the cases, but we've had no issues thus far.
// @Robustness: This doesn't work when we cross an interior vert. When that is done, the
// editor can spinlock entirely.
static bool verify_layer_integrity_around_vertex(Edit_Mesh_Layer *layer, s32 vertex_index) {
    if(layer->vert_count >= 3) {
        v2 vertex = layer->verts[vertex_index];
        
        s32 a_index = (layer->vert_count + vertex_index - 1) % layer->vert_count;
        s32 c_index = (vertex_index + 1) % layer->vert_count;
        v2 a = layer->verts[a_index];
        v2 c = layer->verts[c_index];
        
        ssize t0_index = layer->vert_count - 1;
        FORI_NAMED(t1_index, 0, layer->vert_count) {
            v2 t0 = layer->verts[t0_index];
            v2 t1 = layer->verts[t1_index];
            
            bool cancel = false;
            if((t1_index == a_index) && (t0_index != c_index)) {
                cancel = line_line_overlap(t0, t1, vertex, c);
            } else if((t0_index == c_index) && (t1_index != a_index)) {
                cancel = line_line_overlap(t0, t1, a, vertex);
            } else if((t0_index == c_index) && (t1_index == a_index)) {
                cancel = (v2_cross_prod(t1 - t0, vertex - t0) <= 0.0f);
            } else if((t0_index != a_index) && (t0_index != vertex_index)) {
                cancel = line_line_overlap(t0, t1, a, vertex) || line_line_overlap(t0, t1, vertex, c);
            }
            
            if(cancel) {
                return false;
            }
            
            t0_index = t1_index;
        }
    }
    
    return true;
}

static inline s32 index_count_or_zero(s32 vertex_count) {
    return s_max(0, (vertex_count - 2) * 3);
}

static Edit_Mesh_Undo *save_undo_snapshot(Mesh_Editor *editor, u16 mesh_index) {
    Undo_Ring_Buffer *buffer = &editor->undo_buffer;
    Edit_Mesh *mesh = &editor->meshes[mesh_index];
    Edit_Mesh_Layer *layer = &mesh->layers[mesh->current_layer];
    
    s32 vert_count = layer->vert_count;
    s32 index_count = index_count_or_zero(vert_count);
    
    u16 allocation = editor->undo_buffer.first_free;
    usize allocation_offset = buffer->allocation_offsets[allocation];
    usize allocation_size = ALIGN_POW2(sizeof(Edit_Mesh_Undo) + sizeof(v2) * vert_count + sizeof(u16) * index_count, 4);
    if((buffer->block.used + allocation_size) > buffer->block.size) {
        buffer->block.used = 0;
        buffer->wrap_length = allocation;
        allocation = 0;
        allocation_offset = 0;
    }
    
    usize allocation_end = allocation_offset + allocation_size;
    u16 first_free = (allocation + 1) & UNDO_RING_BUFFER_ALLOCATION_MASK;
    u16 oldest_allocation = buffer->oldest_allocation;
    if(first_free == oldest_allocation) { // @Untested
        FORI(first_free, 1024) {
            if(buffer->allocation_offsets[i] < allocation_end) {
                oldest_allocation += 1;
            } else {
                break;
            }
        }
        
        oldest_allocation &= UNDO_RING_BUFFER_ALLOCATION_MASK;
    }
    
    buffer->first_free = first_free;
    buffer->allocation_offsets[buffer->first_free] = allocation_offset + allocation_size;
    buffer->oldest_allocation = oldest_allocation;
    buffer->all_undos_popped = false;
    buffer->just_saved_snapshot = true;
    buffer->all_redos_popped = false;
    
    {
        Edit_Mesh_Undo *undo = CAST(Edit_Mesh_Undo *, push_size(&buffer->block, allocation_size, 4));
        
        undo->mesh_index = mesh_index;
        undo->layer_index = mesh->current_layer;
        
        undo->layer.color = layer->color;
        undo->layer.vert_count = layer->vert_count;
        
        v2 *serialized_verts = CAST(v2 *, undo + 1);
        u16 *serialized_indices = CAST(u16 *, serialized_verts + vert_count);
        
        mem_copy(layer->verts, serialized_verts, sizeof(v2) * vert_count);
        mem_copy(layer->indices, serialized_indices, sizeof(u16) * index_count);
        
        ASSERT((CAST(sptr, serialized_indices + index_count) - CAST(sptr, undo)) <= CAST(sptr, allocation_size));
        logprint(global_log, "Undo size: %u\n", allocation_size);
        
        return undo;
    }
}

static Edit_Mesh_Undo *maybe_undo(Undo_Ring_Buffer *buffer) {
    s32 wrap_length = buffer->wrap_length;
    s32 first_free = buffer->first_free;
    
    if(buffer->all_undos_popped || (!wrap_length && !first_free)) {
        return 0;
    }
    
    if(buffer->just_saved_snapshot || buffer->all_redos_popped) {
        first_free = (first_free - 1);
        
        if(first_free < 0) {
            first_free = wrap_length;
        }
        
        buffer->start_of_undo_chain = CAST(u16, first_free);
    }
    
    s32 to_pop = first_free - 1;
    if(to_pop < 0) {
        if(wrap_length) {
            to_pop = wrap_length - 1;
            wrap_length = 0;
        } else {
            return 0;
        }
    }
    
    usize pop_offset = buffer->allocation_offsets[to_pop];
    
    // Only overwrite the undo if it's not the base state.
    if(to_pop == buffer->oldest_allocation) {
        buffer->all_undos_popped = true;
    } else {
        buffer->first_free = CAST(u16, to_pop);
        buffer->block.used = pop_offset;
    }
    
    buffer->just_saved_snapshot = false;
    buffer->all_redos_popped = false;
    
    return CAST(Edit_Mesh_Undo *, buffer->block.mem + pop_offset);
}

static Edit_Mesh_Undo *maybe_redo(Undo_Ring_Buffer *buffer) {
    s32 wrap_length = buffer->wrap_length;
    s32 first_free = buffer->first_free;
    
    if(buffer->all_redos_popped || buffer->just_saved_snapshot || (!wrap_length && !first_free)) {
        return 0;
    }
    
    usize redo_offset = buffer->allocation_offsets[first_free];
    
    if(first_free == buffer->start_of_undo_chain) {
        buffer->all_redos_popped = true;
    }
    
    first_free += 1;
    if(wrap_length) {
        first_free %= wrap_length;
    }
    
    buffer->first_free = CAST(u16, first_free);
    buffer->block.used = buffer->allocation_offsets[first_free];
    buffer->just_saved_snapshot = false;
    buffer->all_undos_popped = false;
    
    return CAST(Edit_Mesh_Undo *, buffer->block.mem + redo_offset);
}

static void restore_state(Mesh_Editor *editor, Edit_Mesh_Undo *undo) {
    v2 *undo_verts       = CAST(v2 *, undo + 1);
    u16 *undo_indices    = CAST(u16 *, undo_verts + undo->layer.vert_count);
    
    Edit_Mesh *mesh = &editor->meshes[undo->mesh_index];
    Edit_Mesh_Layer *layer = &mesh->layers[undo->layer_index];
    
    layer->color = undo->layer.color;
    layer->vert_count = undo->layer.vert_count;
    if(undo->layer.vert_count) {
        u16 index_count = (undo->layer.vert_count - 2) * 3;
        
        mem_copy(undo_verts, layer->verts, sizeof(v2) * undo->layer.vert_count);
        mem_copy(undo_indices, layer->indices, sizeof(u16) * index_count);
    }
}

struct Serialized_Edit_Mesh_Layout {
    Serialized_Edit_Mesh *header;
    
    v4 *layer_colors;
    u16 *layer_running_vert_count;
    
    v2 *verts;
    u16 *indices;
};

static usize layout_serialized_mesh(Serialized_Edit_Mesh *mesh, Serialized_Edit_Mesh_Layout *layout,
                                    s32 vert_total) {
    usize length = 0;
    s32 layer_count = mesh->layer_count;
    
    layout->header = mesh;
    length += sizeof(Serialized_Edit_Mesh);
    
    layout->layer_colors = CAST(v4 *, mesh + 1);
    length += sizeof(v4) * layer_count;
    
    layout->layer_running_vert_count = CAST(u16 *, layout->layer_colors + layer_count);
    length += sizeof(u16) * layer_count;
    
    if(vert_total == -1) {
        vert_total = layout->layer_running_vert_count[layer_count - 1];
    }
    
    layout->verts = CAST(v2 *, layout->layer_running_vert_count + layer_count);
    length += sizeof(v2) * vert_total;
    
    layout->indices = CAST(u16 *, layout->verts + vert_total);
    s32 index_total = s_max(0, (vert_total - (layer_count << 1)) * 3);
    length += sizeof(u16) * index_total;
    
    return length;
}

void Implicit_Context::export_mesh(Edit_Mesh *mesh) {
    { // Serialized_Edit_Mesh
        Scoped_Memory scoped_memory = Scoped_Memory(&temporary_memory);
        auto layer_count = mesh->layer_count;
        
        Serialized_Edit_Mesh *serialized = CAST(Serialized_Edit_Mesh *, temporary_memory.mem + temporary_memory.used);
        
        serialized->center_point = mesh->center_point;
        // @Hardcoded
        serialized->offset = V2();
        serialized->scale = V2();
        serialized->rot = V2(1.0f, 0.0f);
        
        ASSERT(layer_count <= 255);
        serialized->layer_count = CAST(u8, layer_count);
        
        s32 vert_total = 0;
        FORI(0, layer_count) {
            vert_total += mesh->layers[i].vert_count;
        }
        
        Serialized_Edit_Mesh_Layout layout;
        
        usize length = layout_serialized_mesh(serialized, &layout, vert_total);
        
        if(length <= (temporary_memory.size - temporary_memory.used)) {
            u16 running_vert_count = 0;
            u16 running_index_count = 0;
            FORI(0, layer_count) {
                u16 vert_count = mesh->layers[i].vert_count;
                u16 index_count = CAST(u16, index_count_or_zero(vert_count));
                
                if(vert_count) {
                    mem_copy(mesh->layers[i].verts, layout.verts + running_vert_count, sizeof(v2) * vert_count);
                    mem_copy(mesh->layers[i].indices, layout.indices + running_index_count, sizeof(u16) * index_count);
                    
                    running_vert_count += vert_count;
                    running_index_count += index_count;
                }
                
                layout.layer_colors[i] = mesh->layers[i].color;
                layout.layer_running_vert_count[i] = running_vert_count;
            }
            
            string file;
            file.data = CAST(u8 *, serialized);
            file.length = length;
            
            os_platform.write_entire_file(STRING("latest.edit_mesh"), file);
        } else UNHANDLED;
    }
    
    { // Serialized_Render_Mesh @Duplication
        Scoped_Memory scoped_memory = Scoped_Memory(&temporary_memory);
        auto layer_count = mesh->layer_count;
        
        Serialized_Render_Mesh *serialized = CAST(Serialized_Render_Mesh *, temporary_memory.mem + temporary_memory.used);
        
        serialized->version = SERIALIZED_RENDER_MESH_VERSION;
        // @Hardcoded
        // In hindsight, we shouldn't need these until we implement some sort of vertex compression.
        serialized->default_offset = V2();
        serialized->default_scale = V2();
        serialized->default_rotation = V2(1.0f, 0.0f);
        
        s32 vert_total = 0;
        FORI(0, layer_count) {
            vert_total += mesh->layers[i].vert_count;
        }
        
        s32 index_total = 0;
        if(vert_total) {
            index_total = (vert_total - 2) * 3;
        }
        
        serialized->vertex_count = CAST(u16, vert_total);
        
        usize length = sizeof(Serialized_Render_Mesh) + sizeof(Render_Vertex) * vert_total + sizeof(u16) * index_total;
        
        Render_Vertex *verts = CAST(Render_Vertex *, serialized + 1);
        u16 *indices = CAST(u16 *, verts + vert_total);
        
        if(length <= (temporary_memory.size - temporary_memory.used)) {
            s32 running_vert_count = 0;
            s32 running_index_count = 0;
            
            f32 max_distance_from_center_point = 0.0f;
            FORI(0, layer_count) {
                Edit_Mesh_Layer *layer = &mesh->layers[i];
                u16 vert_count = layer->vert_count;
                u16 index_count = CAST(u16, index_count_or_zero(vert_count));
                
                if(vert_count) {
                    FORI_NAMED(vert_index, 0, vert_count) {
                        Render_Vertex *vert = &verts[running_vert_count + vert_index];
                        
                        v2 p = layer->verts[vert_index] - mesh->center_point;
                        
                        f32 dist_sq = v2_length_sq(p);
                        if(dist_sq > max_distance_from_center_point) {
                            max_distance_from_center_point = dist_sq;
                        }
                        
                        vert->p = p;
                        vert->color = layer->color;
                    }
                    
                    FORI_NAMED(index_index, 0, index_count) {
                        indices[running_index_count + index_index] = CAST(u16, layer->indices[index_index] + running_vert_count);
                    }
                    
                    running_vert_count += vert_count;
                    running_index_count += index_count;
                }
            }
            
            // Normalize verts to a -0.5 to 0.5 box.
            if(max_distance_from_center_point) {
                max_distance_from_center_point = f_sqrt(max_distance_from_center_point);
                
                const f32 normalize_factor = 1.0f / (max_distance_from_center_point * 2.0f);
                
                FORI_NAMED(vert_index, 0, vert_total) {
                    verts[vert_index].p *= normalize_factor;
                }
            }
            
            string file;
            file.data = CAST(u8 *, serialized);
            file.length = length;
            
            os_platform.write_entire_file(STRING("latest.mesh"), file);
        } else UNHANDLED;
    }
}

static void import_mesh(Edit_Mesh *mesh, string name) {
    string file = os_platform.read_entire_file(name);
    
    if(file.data) {
        Serialized_Edit_Mesh *serialized = CAST(Serialized_Edit_Mesh *, file.data);
        
        if(serialized->version == SERIALIZED_EDIT_MESH_VERSION) {
            Serialized_Edit_Mesh_Layout layout;
            layout_serialized_mesh(serialized, &layout, -1);
            
            ASSERT(serialized->layer_count);
            u16 layer_count = serialized->layer_count;
            mesh->layer_count = layer_count;
            mesh->current_layer = 0;
            
            u16 running_vert_count = 0;
            u16 running_index_count = 0;
            FORI(0, layer_count) {
                Edit_Mesh_Layer *layer = &mesh->layers[i];
                
                u16 vert_count = layout.layer_running_vert_count[i] - running_vert_count;
                u16 index_count = CAST(u16, index_count_or_zero(vert_count));
                
                if(vert_count) { //ASSERT(vert_count);
                    
                    mem_copy(layout.verts + running_vert_count,    layer->verts,   sizeof(v2)  *  vert_count);
                    mem_copy(layout.indices + running_index_count, layer->indices, sizeof(u16) * index_count);
                    
                    running_vert_count += vert_count;
                    running_index_count += index_count;
                }
                
                layer->vert_count = vert_count;
                layer->color = layout.layer_colors[i];
            }
        }
    }
}

static v3 recompute_color_picker_selection(v3 hue, v2 uv) {
    const v3 white = V3(1.0f);
    const v3 black = V3(0.0f);
    
    v3 color = v3_hadamard_prod(v3_lerp(white, hue, uv.x), v3_lerp(black, white, uv.y));
    
    return color;
}

static inline void scale_verts_from_center_point(v2 *verts, s32 vert_count, v2 center_point, f32 scale) {
    FORI(0, vert_count) {
        v2 dp = verts[i] - center_point;
        
        verts[i] += dp * scale;
    }
}

static inline void rotate_verts_around_center_point(v2 *verts, s32 vert_count, v2 center_point, v2 rot) {
    FORI(0, vert_count) {
        v2 dp = verts[i] - center_point;
        
        v2 rotated = v2_complex_prod(dp, rot);
        
        verts[i] = center_point + rotated;
    }
}

static inline void scale_rot_drag_interact(Input *in, v2 *cursor_p, bool *should_draw_cursor, bool *triangulate_layer,
                                           f32 *dscale, v2 *rot, v2 *drag) {
    const bool jump_down = BUTTON_DOWN(in, jump);
    const bool run_down = BUTTON_DOWN(in, run);
    const bool crouch_down = BUTTON_DOWN(in, crouch);
    
    if(!BUTTON_PRESSED(in, jump) && !BUTTON_PRESSED(in, run) && !BUTTON_PRESSED(in, crouch)) {
        if(jump_down) {
            // Scale the layer.
            *dscale = cursor_p->y * 0.1f;
            
        } else if(run_down) {
            // Rotate the layer.
            f32 drot = cursor_p->y * 0.1f;
            *rot = v2_angle_to_complex(drot);
        } else if(crouch_down) {
            *drag = *cursor_p;
        }
    }
    
    if(jump_down || run_down || crouch_down) {
        *should_draw_cursor = false;
        *cursor_p = V2();
        *triangulate_layer = true;
    }
}

static v2 unit_scale_to_world_space_offset(v2, f32);
static v2 world_space_offset_to_unit_scale(v2, f32);
void Implicit_Context::mesh_update(Mesh_Editor *editor, Renderer *renderer, Input *in, Render_Command_Queue *command_queue) {
    using namespace MESH_EDITOR_UI_SELECTION;
    SCOPE_MEMORY(&temporary_memory);
    
    render_set_transform_game_camera(command_queue->command_list_handle, V2(), V2(1.0f, 0.0f), 1.0f);
    
    u16 current_mesh_index = editor->current_mesh;
    
    Edit_Mesh *mesh = &editor->meshes[current_mesh_index];
    
    // @Incomplete: Need to be able to change default scale and rotation.
    
    Mesh_Interaction *interaction = &editor->interaction;
    Mesh_Hover hover = {};
    v2 cursor_p = unit_scale_to_world_space_offset(in->xhairp, 1.0f);
    
    bool should_draw_cursor = true;
    bool triangulate_layer = false;
    bool triangulate_the_whole_mesh = false;
    
    { // @Hack
        static bool first_pass = true;
        if(first_pass) {
            first_pass = false;
            import_mesh(mesh, STRING("latest.edit_mesh"));
            triangulate_layer = true;
        }
    }
    
    if(BUTTON_PRESSED(in, slot1)) {
        // Undo.
        Edit_Mesh_Undo *undo = maybe_undo(&editor->undo_buffer);
        if(undo) {
            restore_state(editor, undo);
            
            triangulate_layer = true;
        }
    } else if(BUTTON_PRESSED(in, slot2)) {
        // Redo.
        Edit_Mesh_Undo *redo = maybe_redo(&editor->undo_buffer);
        if(redo) {
            restore_state(editor, redo);
            
            triangulate_layer = true;
        }
    }
    
    if(!BUTTON_DOWN(in, attack)) {
        // This is the only place where selection is set to NONE.
        // That means we can record every end of interaction here.
        // @Cleanup: Recording ends of interactions makes the undo system kind of unwieldy, but
        // we'll work with it for now. We basically just need a few more checks to make it work
        // exactly as expected, and the current system would also work with redo.
        if(interaction->selection != NONE) {
            save_undo_snapshot(editor, current_mesh_index);
        }
        
        interaction->selection = NONE;
    }
    
    if(!BUTTON_DOWN(in, crouch)) {
        if(BUTTON_PRESSED(in, up)) {
            editor->highlight_all_vertices = !editor->highlight_all_vertices;
        }
        if(BUTTON_PRESSED(in, left)) {
            editor->snap_to_grid = !editor->snap_to_grid;
        }
        if(BUTTON_PRESSED(in, down)) {
            editor->snap_to_edge = !editor->snap_to_edge;
        }
        if(BUTTON_PRESSED(in, right)) {
            export_mesh(mesh);
        }
    } else {
        // Changing layer order.
        if(BUTTON_PRESSED(in, up)) {
            s32 swap0 = mesh->current_layer;
            s32 swap1 = swap0 + 1;
            
            if(swap1 < MAX_LAYERS_PER_MESH) {
                SWAP(mesh->layers[swap0], mesh->layers[swap1]);
                mesh->current_layer = CAST(u16, swap1);
            }
        } else if(BUTTON_PRESSED(in, down)) { // @Duplication
            s32 swap0 = mesh->current_layer;
            s32 swap1 = swap0 - 1;
            
            if(swap1 < MAX_LAYERS_PER_MESH) {
                SWAP(mesh->layers[swap0], mesh->layers[swap1]);
                mesh->current_layer = CAST(u16, swap1);
            }
        }
    }
    
    s32 current_layer_index = mesh->current_layer;
    auto * const current_layer = &mesh->layers[current_layer_index];
    s32 vert_count_at_start_of_update = current_layer->vert_count;
    
    if(interaction->selection == NONE) {
        if(v2_length_sq(cursor_p - mesh->center_point) < (0.1f * 0.1f)) {
            if(BUTTON_PRESSED(in, attack)) {
                interaction->selection = CENTER_POINT;
            } else {
                hover.selection = CENTER_POINT;
            }
        }
    }
    
    if((hover.selection == NONE) && (interaction->selection == NONE)) {
        FORI_TYPED_NAMED(s32, b_index, 0, current_layer->vert_count) {
            v2 b = current_layer->verts[b_index];
            
            if(v2_length_sq(cursor_p - b) < (0.1f * 0.1f)) {
                
                if(BUTTON_PRESSED(in, attack)) {
                    interaction->selection = VERTEX;
                    interaction->vert.index = b_index;
                    interaction->vert.vert_drag_start = b;
                    interaction->vert.cursor_drag_start = cursor_p;
                } else {
                    hover.selection = VERTEX;
                    hover.index = b_index;
                }
                
                break;
            }
        }
    }
    
    // @Hack :NeedToGenerateMultipleOutputMeshes
    if(editor->last_mesh != current_mesh_index) {
        triangulate_layer = true;
        editor->last_mesh = current_mesh_index;
    }
    
    //
    // Layout
    //
    
#define MESH_PICKER_UNEXPANDED_BOTTOM 3.5f
#define MESH_PICKER_EXPANDED_BOTTOM 4.5f - ((4.5f - MESH_PICKER_UNEXPANDED_BOTTOM) * MESHES_IN_PICKER)
    enum Layout {
        color_picker,
        hue_picker,
        layer_picker,
        mesh_picker,
        
        count,
    };
    rect2 layout[Layout::count];
    layout[color_picker] = rect2_4f(-6.8f, -4.0f, -5.8f, -3.0f);
    layout[hue_picker]   = rect2_4f(-5.7f, -4.0f, -5.4f, -3.0f);
    layout[layer_picker] = rect2_4f(-8.0f, -4.0f, -7.0f, 4.0f);
    {
        rect2 mesh_picker_aabb = rect2_4f(-5.5f, MESH_PICKER_UNEXPANDED_BOTTOM, 5.5f, 4.5f);
        if(editor->expanded_mesh_picker) {
            mesh_picker_aabb.bottom = MESH_PICKER_EXPANDED_BOTTOM;
        }
        layout[mesh_picker] = mesh_picker_aabb;
    }
    
    s32 hit = v2_rect2_first_hit(cursor_p, layout, Layout::count);
    { // Batched-hittest immediate UI update.
        // @Duplication: This would also work:
        // selection = hit + COLOR_PICKER;
        // ... and to make it more explicit, we could define FIRST_UI_WIDGET = COLOR_PICKER;
        // or something.
        switch(hit) {
            case color_picker: {
                if(BUTTON_PRESSED(in, attack)) {
                    interaction->selection = COLOR_PICKER;
                }
            } break;
            
            case hue_picker: {
                if(BUTTON_PRESSED(in, attack)) {
                    interaction->selection = HUE_PICKER;
                }
            } break;
            
            case layer_picker: {
                if(BUTTON_PRESSED(in, attack)) {
                    interaction->selection = LAYER_PICKER;
                }
            } break;
            
            case mesh_picker: {
                if(BUTTON_PRESSED(in, attack)) {
                    interaction->selection = MESH_PICKER;
                }
            } break;
        }
    }
    
    //
    // At this point, UI selection should be figured out.
    //
    
    s32 extrude_edge = -1;
    s32 maybe_new_vertex_insert_index = -1;
    v2 maybe_new_vertex = snap_vertex(mesh, cursor_p, current_layer_index, editor->snap_to_grid, editor->snap_to_edge);
    if(hover.selection == NONE) {
        if((current_layer->vert_count < 64) && (current_layer->vert_count >= 2)) { // Find the best edge to extrude if we had to add a new vertex.
            // This is done on every frame for UI purposes.
            f32 candidate_distance = LARGE_FLOAT;
            
            s32 a_index = current_layer->vert_count - 1;
            v2 a = current_layer->verts[a_index];
            FORI_TYPED_NAMED(s32, b_index, 0, current_layer->vert_count) {
                v2 b = current_layer->verts[b_index];
                
                if((v2_inner(a - b, a - maybe_new_vertex) > 0.0f) && (v2_inner(b - a, b - maybe_new_vertex) > 0.0f) && (v2_cross_prod(b - a, maybe_new_vertex - a) < 0.0f)) {
                    // We found an adequate edge.
                    
                    f32 dist = v2_line_distance(maybe_new_vertex, a, b);
                    if(dist < candidate_distance) {
                        maybe_new_vertex_insert_index = b_index;
                        candidate_distance = dist;
                        extrude_edge = a_index;
                    }
                }
                
                a_index = b_index;
                a = b;
            }
            
            if(candidate_distance < 0.1f) {
                v2 edge_b = current_layer->verts[(extrude_edge + 1) % current_layer->vert_count];
                v2 edge_a = current_layer->verts[extrude_edge];
                
                if((interaction->selection == NONE) && BUTTON_PRESSED(in, attack)) {
                    interaction->selection = EDGE;
                    interaction->edge.index = extrude_edge;
                    interaction->edge.vert_starts[0] = current_layer->verts[interaction->edge.index];
                    interaction->edge.vert_starts[1] = current_layer->verts[(interaction->edge.index + 1) % current_layer->vert_count];
                    interaction->edge.cursor_start = cursor_p;
                    interaction->edge.last_scale = 1.0f;
                    interaction->edge.last_drag = V2();
                    interaction->edge.last_drag_cursor_p = cursor_p;
                    interaction->edge.last_rotation = V2(1.0f, 0.0f);
                    
                    v2 adjacent_right = current_layer->verts[(interaction->edge.index + current_layer->vert_count - 1) % current_layer->vert_count];
                    v2 adjacent_left = current_layer->verts[(interaction->edge.index + 2) % current_layer->vert_count];
                    
                    interaction->edge.left_adjacent_dir = v2_normalize(edge_b - adjacent_left);
                    interaction->edge.right_adjacent_dir = v2_normalize(edge_a - adjacent_right);
                } else {
                    hover.selection = EDGE;
                    hover.index = extrude_edge;
                    
                    f32 dot = v2_inner(v2_normalize(edge_b - edge_a), maybe_new_vertex - edge_a);
                    maybe_new_vertex = edge_a + v2_normalize(edge_b - edge_a) * dot;
                }
            }
        } else if(current_layer->vert_count <= 1) {
            maybe_new_vertex_insert_index = current_layer->vert_count;
        }
    }
    
    
    switch(interaction->selection) {
        case VERTEX: {
            // Drag the selected vert.
            
            s32 selected_vert = interaction->vert.index;
            
            v2 target = interaction->vert.vert_drag_start + (cursor_p - interaction->vert.cursor_drag_start);
            target = snap_vertex(mesh, target, current_layer_index, editor->snap_to_grid, editor->snap_to_edge);
            
            v2 old_vert = current_layer->verts[selected_vert];
            current_layer->verts[selected_vert] = target;
            
            if(verify_layer_integrity_around_vertex(current_layer, selected_vert)) {
                triangulate_layer = true;
            } else {
                current_layer->verts[selected_vert] = old_vert;
            }
        } break;
        
        case EDGE: {
            s32 b_index = interaction->edge.index;
            s32 c_index = (interaction->edge.index + 1) % current_layer->vert_count;
            
            const v2 b = current_layer->verts[b_index];
            const v2 c = current_layer->verts[c_index];
            
            v2 drag = interaction->edge.last_drag;
            
            f32 scale_factor = interaction->edge.last_scale;
            v2 center = (interaction->edge.vert_starts[0] + interaction->edge.vert_starts[1]) * 0.5f;
            v2 half_edge = (interaction->edge.vert_starts[0] - interaction->edge.vert_starts[1]) * 0.5f;
            
            v2 rotation = interaction->edge.last_rotation;
            
            if(BUTTON_DOWN(in, jump)) {
                if(BUTTON_PRESSED(in, jump)) {
                    cursor_p = V2();
                }
                // Scale the edge.
                scale_factor += (cursor_p.y / 9.0f) * 2.0f;
                cursor_p = V2();
                should_draw_cursor = false;
                
                interaction->edge.last_scale = scale_factor;
            } else if(BUTTON_DOWN(in, run)) {
                if(BUTTON_PRESSED(in, run)) {
                    cursor_p = V2();
                }
                // Rotate the edge.
                f32 angle = PI * cursor_p.y / GAME_ASPECT_RATIO_Y;
                cursor_p = V2();
                rotation = v2_complex_prod_n(rotation, v2_angle_to_complex(angle));
                should_draw_cursor = false;
                
                interaction->edge.last_rotation = rotation;
            } else {
                if(BUTTON_RELEASED(in, jump) || BUTTON_RELEASED(in, run)) {
                    cursor_p = interaction->edge.last_drag_cursor_p;
                }
                
                // Drag the edge.
                scale_factor = interaction->edge.last_scale;
                
                v2 target = cursor_p;
                if(editor->snap_to_grid) {
                    target = do_snap_to_grid(target);
                }
                
                drag = target - interaction->edge.cursor_start;
                
                if(editor->snap_to_edge) {
                    v2 drag_n = v2_normalize(drag);
                    
                    v2 best_dot_dir = interaction->edge.left_adjacent_dir;
                    f32 best_dot = v2_inner(drag_n, best_dot_dir);
                    f32 right_dot = v2_inner(drag_n, interaction->edge.right_adjacent_dir);
                    
                    if(f_abs(best_dot) < f_abs(right_dot)) {
                        best_dot = right_dot;
                        best_dot_dir = interaction->edge.right_adjacent_dir;
                    }
                    
                    drag = best_dot_dir * v2_inner(best_dot_dir, drag);
                }
                
                interaction->edge.last_drag = drag;
                interaction->edge.last_drag_cursor_p = cursor_p;
            }
            
            v2 vert_offset = v2_complex_prod(half_edge * scale_factor, rotation);
            
            current_layer->verts[b_index] = center + vert_offset + drag;
            current_layer->verts[c_index] = center - vert_offset + drag;
            
            if(verify_layer_integrity_around_vertex(current_layer, b_index) && 
               verify_layer_integrity_around_vertex(current_layer, c_index)) {
                triangulate_layer = true;
            } else {
                current_layer->verts[b_index] = b;
                current_layer->verts[c_index] = c;
            }
        } break;
        
        case CENTER_POINT: {
            if(BUTTON_PRESSED(in, attack)) {
                cursor_p = V2();
            }
            
            v2 dp = cursor_p;
            mesh->center_point += dp;
            
            cursor_p = V2();
            should_draw_cursor = false;
        } break;
        
        case COLOR_PICKER: {
            if(BUTTON_PRESSED(in, attack)) {
                interaction->color_picker.layer_color_before = current_layer->color;
            } else ASSERT(BUTTON_DOWN(in, attack));
            
            if(BUTTON_DOWN(in, attack)) {
                if(hit == layer_picker) {
                    // @Duplication
                    f32 picker_bottom = layout[layer_picker].bottom;
                    f32 picker_height = layout[layer_picker].top - picker_bottom;
                    f32 relative_cursor_height = cursor_p.y - picker_bottom;
                    f32 item_height = picker_height / MAX_LAYERS_PER_MESH;
                    
                    s32 picked_index = f_floor_s(relative_cursor_height / item_height);
                    if(picked_index == current_layer_index) {
                        current_layer->color = interaction->color_picker.layer_color_before;
                    } else if(picked_index < mesh->layer_count) {
                        current_layer->color = mesh->layers[picked_index].color;
                    }
                } else {
                    v2 color_picker_uv = v2_reverse_lerp(layout[color_picker].min, layout[color_picker].max, cursor_p);
                    
                    color_picker_uv.x = f_clamp(color_picker_uv.x, 0.0f, 1.0f);
                    color_picker_uv.y = f_clamp(color_picker_uv.y, 0.0f, 1.0f);
                    
                    v3 color = recompute_color_picker_selection(renderer->color_picker_color->xyz, color_picker_uv);
                    
                    current_layer->color = V4(color, 1.0f);
                    editor->color_picker_uv = color_picker_uv;
                }
                
                triangulate_layer = true;
            }
        } break;
        
        case HUE_PICKER: {
            const v4 hues[] = {
                V4(1.0f, 0.0f, 0.0f, 1.0f),
                V4(1.0f, 1.0f, 0.0f, 1.0f),
                V4(0.0f, 1.0f, 0.0f, 1.0f),
                V4(0.0f, 1.0f, 1.0f, 1.0f),
                V4(0.0f, 0.0f, 1.0f, 1.0f),
                V4(1.0f, 0.0f, 1.0f, 1.0f),
                V4(1.0f, 0.0f, 0.0f, 1.0f),
            };
            
            rect2 picker = layout[hue_picker];
            
            if(cursor_p.y <= picker.bottom) {
                set_color_picker_hue(renderer, command_queue->command_list_handle, hues[0]);
            } else if(cursor_p.y >= picker.top) {
                set_color_picker_hue(renderer, command_queue->command_list_handle, hues[ARRAY_LENGTH(hues)-1]);
            } else {
                f32 picker_height = picker.top - picker.bottom;
                f32 item_height = picker_height / (ARRAY_LENGTH(hues) - 1);
                s32 item_index = f_floor_s((cursor_p.y - picker.bottom) / item_height);
                
                f32 item_bottom = picker.bottom + item_height * item_index;
                f32 item_top = item_bottom + item_height;
                f32 lerp_factor = f_reverse_lerp(item_bottom, item_top, cursor_p.y);
                
                v4 color = v4_lerp(hues[item_index], hues[item_index + 1], lerp_factor);
                set_color_picker_hue(renderer, command_queue->command_list_handle, color);
                
                v3 selected = recompute_color_picker_selection(renderer->color_picker_color->xyz, editor->color_picker_uv);
                current_layer->color = V4(selected, 1.0f);
                
                triangulate_layer = true;
            }
        } break;
        
        case LAYER_PICKER: {
            if(BUTTON_PRESSED(in, attack)) {
                const f32 picker_bottom = layout[layer_picker].bottom;
                const f32 picker_height = layout[layer_picker].top - picker_bottom;
                const f32 relative_cursor_height = cursor_p.y - picker_bottom;
                const f32 item_height = picker_height / MAX_LAYERS_PER_MESH;
                
                const s32 picked_index = f_floor_s(relative_cursor_height / item_height);
                
                if(picked_index >= mesh->layer_count) {
                    mesh->layer_count = CAST(u16, picked_index + 1);
                }
                
                if(picked_index <= mesh->layer_count) {
                    mesh->current_layer = CAST(u16, s_min(mesh->layer_count - 1, picked_index));
                }
            }
            
            f32 dscale = 0.0f;
            v2 rot = V2();
            v2 drag = V2();
            scale_rot_drag_interact(in, &cursor_p, &should_draw_cursor, &triangulate_layer, &dscale, &rot, &drag);
            
            if(dscale) {
                scale_verts_from_center_point(current_layer->verts, current_layer->vert_count, mesh->center_point, dscale);
            } else if(rot.x || rot.y) {
                rotate_verts_around_center_point(current_layer->verts, current_layer->vert_count, mesh->center_point, rot);
            } else if(drag.x || drag.y) {
                FORI(0, current_layer->vert_count) {
                    current_layer->verts[i] += drag;
                }
            }
        } break;
        
        case MESH_PICKER: {
            s32 picker_height = 1;
            if(editor->expanded_mesh_picker) {
                picker_height = MESHES_IN_PICKER;
            }
            
            v2 picker_min = layout[mesh_picker].min;
            v2 picker_dim = layout[mesh_picker].max - picker_min;
            v2 relative_cursor_p = cursor_p - picker_min;
            v2 item_dim = V2(picker_dim.x / MESHES_IN_PICKER, picker_dim.y / CAST(f32, picker_height));
            
            s32 picked_x = f_floor_s(relative_cursor_p.x / item_dim.x);
            s32 picked_y = picker_height - 1 - f_floor_s(relative_cursor_p.y / item_dim.y);
            s32 picked_index = picked_y * picker_height + picked_x;
            if(picked_y > 0) {
                // We ignore the expand button to find the mesh index.
                picked_index -= 1;
            }
            
            if(BUTTON_PRESSED(in, attack)) {
                if((picked_x < (MESHES_IN_PICKER - 1)) || (picked_y > 0)) {
                    if(picked_index >= editor->mesh_count) {
                        FORI(editor->mesh_count, picked_index + 1) {
                            edit_mesh_init(editor, &editor->meshes[i]);
                        }
                        
                        editor->mesh_count = picked_index + 1;
                    } else {
                        editor->current_mesh = CAST(u16, s_min(editor->mesh_count - 1, picked_y * picker_height + picked_x));
                    }
                } else {
                    editor->expanded_mesh_picker = !editor->expanded_mesh_picker;
                    if(editor->expanded_mesh_picker) {
                        layout[mesh_picker].bottom = MESH_PICKER_EXPANDED_BOTTOM;
                    } else {
                        layout[mesh_picker].bottom = MESH_PICKER_UNEXPANDED_BOTTOM;
                    }
                }
            }
            
            f32 dscale = 0.0f;
            v2 rot = V2();
            v2 drag = V2();
            scale_rot_drag_interact(in, &cursor_p, &should_draw_cursor, &triangulate_layer, &dscale, &rot, &drag);
            
            if(dscale) {
                FORI_NAMED(layer_index, 0, mesh->layer_count) {
                    Edit_Mesh_Layer *layer = &mesh->layers[layer_index];
                    
                    scale_verts_from_center_point(layer->verts, layer->vert_count, mesh->center_point, dscale);
                }
                
                triangulate_layer = true;
                triangulate_the_whole_mesh = true;
            } else if(rot.x || rot.y) {
                FORI_NAMED(layer_index, 0, mesh->layer_count) {
                    Edit_Mesh_Layer *layer = &mesh->layers[layer_index];
                    
                    rotate_verts_around_center_point(layer->verts, layer->vert_count, mesh->center_point, rot);
                }
                
                triangulate_layer = true;
                triangulate_the_whole_mesh = true;
            } else if(drag.x || drag.y) {
                FORI_NAMED(layer_index, 0, mesh->layer_count) {
                    Edit_Mesh_Layer *layer = &mesh->layers[layer_index];
                    FORI_NAMED(vert_index, 0, layer->vert_count) {
                        layer->verts[vert_index] += drag;
                    }
                }
                triangulate_layer = true;
                triangulate_the_whole_mesh = true;
            }
        } break;
        
        default: {
            if(BUTTON_PRESSED(in, attack2)) {
                // Add or remove a vert.
                bool ok = false;
                
                if(maybe_new_vertex_insert_index != -1) {
                    current_layer->vert_count += 1;
                    FORI_REVERSE_NAMED(shift_index, current_layer->vert_count - 1, maybe_new_vertex_insert_index + 1) {
                        current_layer->verts[shift_index] = current_layer->verts[shift_index - 1];
                    }
                    current_layer->verts[maybe_new_vertex_insert_index] = maybe_new_vertex;
                    ok = true;
                } else if(hover.selection == VERTEX) {
                    ASSERT(hover.index != -1);
                    // Remove the hovered vert.
                    current_layer->vert_count -= 1;
                    FORI(hover.index, current_layer->vert_count) {
                        current_layer->verts[i] = current_layer->verts[i + 1];
                    }
                    hover.selection = NONE;
                    
                    ASSERT(current_layer->vert_count >= 0);
                    ok = true;
                }
                
                triangulate_layer = ok;
            }
        }
    }
    
    if(triangulate_layer) {
        s32 vert_count = current_layer->vert_count;
        s32 index_count = 0;
        
        if(vert_count >= 3) {
            s32 start_layer_index = mesh->current_layer;
            s32 end_layer_index = mesh->current_layer + 1;
            if(triangulate_the_whole_mesh) {
                start_layer_index = 0;
                end_layer_index = mesh->layer_count;
            }
            
            FORI_NAMED(layer_index, start_layer_index, end_layer_index) {
                // Triangulating the modified layer.
                SCOPE_MEMORY(&temporary_memory);
                Edit_Mesh_Layer *layer = &mesh->layers[layer_index];
                
                v2 *scratch_verts = push_array(&temporary_memory, v2, layer->vert_count);
                u16 *scratch_indices = push_array(&temporary_memory, u16, index_count_or_zero(layer->vert_count));
                
                FORI(0, layer->vert_count) {
                    scratch_verts[i] = layer->verts[i];
                    scratch_indices[i] = CAST(u16, i);
                }
                
                
                while(vert_count > 3) {
                    u16 a_index = CAST(u16, vert_count - 2);
                    u16 b_index = CAST(u16, vert_count - 1);
                    FORI(0, vert_count) {
                        v2 a = scratch_verts[a_index];
                        v2 b = scratch_verts[b_index];
                        v2 c = scratch_verts[i];
                        
                        bool ear = true;
                        
                        // The ear tip needs to be a convex vertex:
                        if(v2_cross_prod(b - a, c - a) > 0.0f) {
                            // We can't have any other vertex be in the ear.
                            FORI_TYPED_NAMED(u16, test_index, 0, vert_count) {
                                v2 test = scratch_verts[test_index];
                                
                                if((test_index != a_index) && (test_index != b_index) && (test_index != i)) {
                                    if((v2_cross_prod(b - a, test - a) < 0.0f) ||
                                       (v2_cross_prod(c - b, test - b) < 0.0f) ||
                                       (v2_cross_prod(a - c, test - c) < 0.0f)) {
                                        // The point is outside the triangle.
                                        continue;
                                    }
                                    
                                    ear = false;
                                    break;
                                }
                            }
                        } else {
                            ear = false;
                        }
                        
                        if(ear) {
                            layer->indices[index_count + 0] = scratch_indices[a_index];
                            layer->indices[index_count + 1] = scratch_indices[b_index];
                            layer->indices[index_count + 2] = scratch_indices[i];
                            index_count += 3;
                            
                            FORI_NAMED(shift_index, b_index, vert_count - 1) {
                                scratch_verts[shift_index] = scratch_verts[shift_index + 1];
                                scratch_indices[shift_index] = scratch_indices[shift_index + 1];
                            }
                            vert_count -= 1;
                            
                            break;
                        }
                        
                        a_index = b_index;
                        b_index = CAST(u16, i);
                    }
                }
                
                layer->indices[index_count + 0] = scratch_indices[0];
                layer->indices[index_count + 1] = scratch_indices[1];
                layer->indices[index_count + 2] = scratch_indices[2];
                index_count += 3;
            }
        }
        
        { // Merging the layers into the mesh.
            s32 running_vert_index = 0;
            s32 running_index_index = 0;
            FORI_NAMED(layer_index, 0, mesh->layer_count) {
                auto layer = &mesh->layers[layer_index];
                
                if(layer->vert_count >= 3) {
                    s32 layer_index_count = index_count_or_zero(layer->vert_count);
                    FORI(0, layer_index_count) {
                        mesh->output.index_mapped[running_index_index] = CAST(u16, layer->indices[i] + running_vert_index);
                        
                        running_index_index += 1;
                    }
                    
                    FORI(0, layer->vert_count) {
                        mesh->output.vertex_mapped[running_vert_index].p = layer->verts[i];
                        mesh->output.vertex_mapped[running_vert_index].color = layer->color;
                        
                        running_vert_index += 1;
                    }
                }
            }
            
            mesh->output.vert_count = CAST(u16, running_vert_index);
            mesh->output.index_count = CAST(u16, running_index_index);
        }
        
        os_platform.update_editable_mesh(command_queue->command_list_handle, mesh->output.handle, mesh->output.index_count, false);
    }
    
    //
    // At this point, we should be done updating.
    // The rest of this procedure is draw calls.
    //
    
    s32 hovered_vert = -1;
    if(hover.selection == VERTEX) {
        hovered_vert = hover.index;
    }
    
    //
    // Mesh layer
    //
    
    {
        Mesh_Instance instance;
        instance.offset = mesh->output.default_offset;
        instance.scale = mesh->output.default_scale;
        instance.rot = mesh->output.default_rot;
        instance.color = V4(1.0f);
        
        render_mesh(command_queue, mesh->output.handle, &instance);
    }
    
    if(editor->highlight_all_vertices) {
        FORI_NAMED(layer_index, 0, mesh->layer_count) {
            auto layer = &mesh->layers[layer_index];
            
            FORI_NAMED(vert_index, 0, layer->vert_count) {
                if((layer_index != current_layer_index) || (vert_index != hovered_vert)) {
                    Mesh_Instance highlight = {};
                    highlight.offset = layer->verts[vert_index];
                    highlight.scale = V2(0.25f);
                    highlight.rot = V2(1.0f, 0.0f);
                    highlight.color = (layer_index == 0) ? V4(1.0f, 1.0f, 0.0f, 1.0f) : V4(0.5f, 0.5f, 0.5f, 1.0f);
                    
                    render_quad(renderer, command_queue, &highlight);
                }
            }
        }
    }
    
    { // Center point.
        Mesh_Instance instance = {};
        instance.offset = mesh->center_point;
        instance.color = V4(0.0f, 0.0f, 0.0f, 1.0f);
        instance.scale = V2(0.15f);
        instance.rot = V2(1.0f, 0.0f);
        
        render_quad(renderer, command_queue, &instance);
        
        instance.scale *= 0.5f;
        instance.color = V4(1.0f);
        
        render_quad(renderer, command_queue, &instance);
    }
    
    if(hovered_vert != -1) {
        Mesh_Instance hover_highlight = {};
        
        hover_highlight.offset = current_layer->verts[hovered_vert];
        hover_highlight.scale = V2(0.2f);
        hover_highlight.rot = V2(1.0f, 0.0f);
        hover_highlight.color = V4(0.0f, 1.0f, 0.0f, 1.0f);
        
        render_quad(renderer, command_queue, &hover_highlight);
    } else if((hover.selection == EDGE) && (interaction->selection == NONE)) {
        Mesh_Instance hover_highlight = {};
        
        v2 a = current_layer->verts[hover.index];
        v2 b = current_layer->verts[(hover.index + 1) % current_layer->vert_count];
        f32 length = v2_length(a - b);
        
        hover_highlight.offset = (a + b) * 0.5f;
        hover_highlight.scale = V2(length, 0.1f);
        hover_highlight.rot = v2_normalize(b - a);
        hover_highlight.color = V4(0.0f, 1.0f, 0.0f, 1.0f);
        
        render_quad(renderer, command_queue, &hover_highlight);
    }
    
    //
    // UI layer
    //
    
    { // Snap LEDs.
        const f32 margin = 0.05f;
        Mesh_Instance instance = {};
        instance.scale = V2(0.08f);
        instance.offset = V2(layout[color_picker].left + instance.scale.x * 0.5f, layout[color_picker].top + instance.scale.y * 0.5f + margin);
        instance.rot = V2(1.0f, 0.0f);
        
        instance.color = V4(1.0f, 0.0f, 0.0f, 1.0f);
        if(editor->snap_to_grid) {
            instance.color.r = 0.0f;
            instance.color.g = 1.0f;
        }
        
        render_quad(renderer, command_queue, &instance);
        
        instance.offset.x += instance.scale.x + margin;
        if(editor->snap_to_edge) {
            instance.color.r = 0.0f;
            instance.color.g = 1.0f;
        } else {
            instance.color.r = 1.0f;
            instance.color.g = 0.0f;
        }
        
        render_quad(renderer, command_queue, &instance);
    }
    
    { // Color picker
        Mesh_Instance instance = make_mesh_instance(layout[color_picker], V4(1.0f, 1.0f, 1.0f, 1.0f));
        render_mesh(command_queue, GET_MESH_HANDLE(renderer, color_picker), &instance);
    }
    
    { // Hue picker
        Mesh_Instance instance = make_mesh_instance(layout[hue_picker], V4(1.0f));
        render_mesh(command_queue, GET_MESH_HANDLE(renderer, hue_picker), &instance);
    }
    
    v4 colors[] = {
        V4(1.0f, 0.5f, 1.0f, 1.0f),
        V4(1.0f, 1.0f, 0.5f, 1.0f),
        
        V4(1.0f, 0.3f, 0.3f, 1.0f), // Toggle expand
        
        V4(0.5f, 0.7f, 0.3f, 1.0f), // Add 0
        V4(0.3f, 1.0f, 0.3f, 1.0f), // Add 1
        
        V4(0.3f, 0.3f, 0.9f, 1.0f), // Selected
    };
    
    { // Layer picker
        rect2 aabb = layout[layer_picker];
        
        f32 item_height = (aabb.top - aabb.bottom) / MAX_LAYERS_PER_MESH;
        
        Mesh_Instance instance = {};
        instance.offset = V2((aabb.left + aabb.right) * 0.5f, aabb.bottom - item_height * 0.5f);
        instance.scale = V2(aabb.right - aabb.left, item_height);
        instance.rot = V2(1.0f, 0.0f);
        
        FORI(0, MAX_LAYERS_PER_MESH) {
            instance.offset.y += item_height;
            
            s32 color_index = i & 1;
            if(i == current_layer_index) {
                color_index = 5;
            } else if(i >= mesh->layer_count) {
                color_index += 3;
            }
            instance.color = colors[color_index];
            
            render_quad(renderer, command_queue, &instance);
        }
    }
    
    { // Mesh picker
        rect2 aabb = layout[mesh_picker];
        
        s32 picker_height = 1;
        if(editor->expanded_mesh_picker) {
            picker_height = MESHES_IN_PICKER;
        }
        
        Mesh_Instance instance = {};
        instance.rot = V2(1.0f, 0.0f);
        instance.scale = V2((aabb.right - aabb.left) / MESHES_IN_PICKER, (aabb.top - aabb.bottom) / CAST(f32, picker_height));
        instance.offset.x = aabb.left - instance.scale.x * 0.5f;
        
        FORI_NAMED(y, 0, picker_height) {
            FORI_NAMED(x, 0, MESHES_IN_PICKER) {
                instance.offset.x = aabb.left + instance.scale.x * (0.5f + CAST(f32, x));
                instance.offset.y = aabb.top  - instance.scale.y * (0.5f + CAST(f32, y));
                
                ssize mesh_index = x + y * MESHES_IN_PICKER;
                if(y > 0) {
                    // Ignore the expand button.
                    mesh_index -= 1;
                }
                
                s32 color_index = (x + y) & 1;
                if((x == (MESHES_IN_PICKER - 1)) && (y == 0)) {
                    color_index = 2;
                } else if(mesh_index == current_mesh_index) {
                    color_index = 5;
                } else if(mesh_index >= editor->mesh_count) {
                    color_index += 3;
                }
                instance.color = colors[color_index];
                
                render_quad(renderer, command_queue, &instance);
            }
        }
    }
    
    //
    // Cursor
    //
    
    if(should_draw_cursor) {
        render_cursor(renderer, command_queue, cursor_p, mesh->layers[mesh->current_layer].color, interaction->selection != VERTEX);
    }
    
    in->xhairp = world_space_offset_to_unit_scale(cursor_p, 1.0f);
    advance_input(in);
    maybe_flush_draw_commands(command_queue);
}
