
static void mesh_init(Mesh_Editor *editor) {
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
        result.x = f_round_to_f(result.x, DRAG_SNAP_GRANULARITY);
        result.y = f_round_to_f(result.y, DRAG_SNAP_GRANULARITY);
    }
    
    return result;
}

static v2 unit_scale_to_world_space_offset(v2, f32);
void Implicit_Context::mesh_update(Mesh_Editor *editor, Renderer *renderer, Input *in) {
    using namespace MESH_EDITOR_UI_SELECTION;
    SCOPE_MEMORY(&temporary_memory);
    
    Edit_Mesh *mesh = &editor->meshes[editor->current_mesh];
    
    // @Incomplete: Need to be able to change default scale and rotation.
    
    // @Cleanup: Debugger doesn't like this.
    MIRROR_VARIABLE(s32, selected_vert, &editor->selected_vert);
    MIRROR_VARIABLE(s32, selection, &editor->selection);
    v2 cursor_p = unit_scale_to_world_space_offset(in->xhairp, 1.0f);
    
    if(!BUTTON_DOWN(in, attack)) {
        selection = MESH_EDITOR_UI_SELECTION::NONE;
    }
    
    if(BUTTON_PRESSED(in, up)) {
        editor->highlight_all_vertices = !editor->highlight_all_vertices;
    }
    if(BUTTON_PRESSED(in, left)) {
        editor->snap_to_grid = !editor->snap_to_grid;
    }
    if(BUTTON_PRESSED(in, down)) {
        editor->snap_to_edge = !editor->snap_to_edge;
    }
    
    s32 current_layer_index = mesh->current_layer;
    auto * const current_layer = &mesh->layers[current_layer_index];
    s32 vert_count_at_start_of_update = current_layer->vert_count;
    
    if(!BUTTON_HELD(in, attack)) {
        selection = NONE;
        selected_vert = -1;
    }
    
    s32 hovered_vert = -1;
    if(selection == NONE) {
        FORI_TYPED(s32, 0, current_layer->vert_count) {
            v2 vert = current_layer->verts[i];
            
            if(v2_length_sq(cursor_p - vert) < (0.1f * 0.1f)) {
                
                if(BUTTON_PRESSED(in, attack)) {
                    selection = VERTEX;
                    selected_vert = i;
                    editor->vert_drag_start = vert;
                    editor->cursor_drag_start = cursor_p;
                } else {
                    hovered_vert = i;
                }
                
                break;
            }
        }
    }
    
    bool triangulate_polygon = false;
    
    // @Hack :NeedToGenerateMultipleOutputMeshes
    if(editor->last_mesh != editor->current_mesh) {
        triangulate_polygon = true;
        editor->last_mesh = editor->current_mesh;
    }
    
    //
    // Layout
    //
    
#define MESH_PICKER_UNEXPANDED_BOTTOM 3.5f
#define MESH_PICKER_EXPANDED_BOTTOM 4.5f - ((4.5f - MESH_PICKER_UNEXPANDED_BOTTOM) * MESHES_IN_PICKER)
    enum Layout {
        color_picker,
        layer_picker,
        mesh_picker,
        
        count,
    };
    rect2 layout[Layout::count];
    layout[color_picker] = rect2_4f(-6.8f, -4.0f, -5.8f, -3.0f);
    layout[layer_picker] = rect2_4f(-8.0f, -4.0f, -7.0f, 4.0f);
    {
        rect2 mesh_picker_aabb = rect2_4f(-5.5f, MESH_PICKER_UNEXPANDED_BOTTOM, 5.5f, 4.5f);
        if(editor->expanded_mesh_picker) {
            mesh_picker_aabb.bottom = MESH_PICKER_EXPANDED_BOTTOM;
        }
        layout[mesh_picker] = mesh_picker_aabb;
    }
    
    { // Batched-hittest immediate UI update.
        s32 hit = v2_rect2_first_hit(cursor_p, layout, Layout::count);
        
        // @Duplication: This would also work:
        // selection = hit + COLOR_PICKER;
        // ... and to make it more explicit, we could define FIRST_UI_WIDGET = COLOR_PICKER;
        // or something.
        switch(hit) {
            case color_picker: {
                if(BUTTON_PRESSED(in, attack)) {
                    selection = COLOR_PICKER;
                }
            } break;
            
            case layer_picker: {
                if(BUTTON_PRESSED(in, attack)) {
                    selection = LAYER_PICKER;
                }
            } break;
            
            case mesh_picker: {
                if(BUTTON_PRESSED(in, attack)) {
                    selection = MESH_PICKER;
                }
            } break;
        }
    }
    
    //
    // At this point, UI selection should be figured out.
    //
    
    switch(selection) {
        case VERTEX: {
            // Drag the selected vert.
            // @Speed?
            // This is just testing whether the polygon would be degenerate after moving the vert.
            // I'm not sure we cover all the cases, but we've had no issues thus far.
            // @Robustness: This doesn't work when we cross an interior vert. When that is done, the
            // editor can spinlock entirely.
            
            v2 target = editor->vert_drag_start + (cursor_p - editor->cursor_drag_start);
            target = snap_vertex(mesh, target, current_layer_index, editor->snap_to_grid, editor->snap_to_edge);
            
            bool cancel = false;
            
            s32 a_index = (current_layer->vert_count + selected_vert - 1) % current_layer->vert_count;
            s32 c_index = (selected_vert + 1) % current_layer->vert_count;
            v2 a = current_layer->verts[a_index];
            v2 c = current_layer->verts[c_index];
            
            ssize t0_index = current_layer->vert_count - 1;
            FORI_NAMED(t1_index, 0, current_layer->vert_count) {
                v2 t0 = current_layer->verts[t0_index];
                v2 t1 = current_layer->verts[t1_index];
                
                if((t1_index == a_index) && (t0_index != c_index)) {
                    cancel = line_line_overlap(t0, t1, target, c);
                } else if((t0_index == c_index) && (t1_index != a_index)) {
                    cancel = line_line_overlap(t0, t1, a, target);
                } else if((t0_index == c_index) && (t1_index == a_index)) {
                    cancel = (v2_cross_prod(t1 - t0, target - t0) <= 0.0f);
                } else if((t0_index != a_index) && (t0_index != selected_vert)) {
                    cancel = line_line_overlap(t0, t1, a, target) || line_line_overlap(t0, t1, target, c);
                }
                
                if(cancel) {
                    break;
                }
                
                t0_index = t1_index;
            }
            
            if(!cancel) {
                current_layer->verts[selected_vert] = target;
                triangulate_polygon = true;
            }
        } break;
        
        case COLOR_PICKER: {
            if(BUTTON_DOWN(in, attack)) {
                v2 color_picker_uv = v2_reverse_lerp(layout[color_picker].min, layout[color_picker].max, cursor_p);
                
                color_picker_uv.x = f_clamp(color_picker_uv.x, 0.0f, 1.0f);
                color_picker_uv.y = f_clamp(color_picker_uv.y, 0.0f, 1.0f);
                
                v3 white = V3(1.0f);
                v3 black = V3(0.0f);
                v3 hue = V3(1.0f, 0.0f, 0.0f);
                
                v3 color = v3_hadamard_prod(v3_lerp(white, hue, color_picker_uv.x), v3_lerp(black, white, color_picker_uv.y));
                current_layer->color = V4(color, 1.0f);
                triangulate_polygon = true;
            }
        } break;
        
        case LAYER_PICKER: {
            if(BUTTON_PRESSED(in, attack)) {
                f32 picker_bottom = layout[layer_picker].bottom;
                f32 picker_height = layout[layer_picker].top - picker_bottom;
                f32 relative_cursor_height = cursor_p.y - picker_bottom;
                f32 item_height = picker_height / MAX_LAYERS_PER_MESH;
                
                s32 picked_index = CAST(s32, relative_cursor_height / item_height);
                
                if(picked_index >= mesh->layer_count) {
                    mesh->layer_count = picked_index + 1;
                } else {
                    mesh->current_layer = s_min(mesh->layer_count - 1, picked_index);
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
            
            s32 picked_x = CAST(s32, relative_cursor_p.x / item_dim.x);
            s32 picked_y = picker_height - 1 - CAST(s32, relative_cursor_p.y / item_dim.y);
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
        } break;
        
        default: {
            if(BUTTON_PRESSED(in, attack2)) {
                // Add or remove a vert.
                bool ok = false;
                
                if(hovered_vert == -1) {
                    // No vert hovered; add one.
                    v2 new_vertex = snap_vertex(mesh, cursor_p, current_layer_index, editor->snap_to_grid, editor->snap_to_edge);
                    
                    if(current_layer->vert_count < 64) {
                        if(current_layer->vert_count >= 3) {
                            // Find the proper location in which to insert the new vertex.
                            ssize candidate_insert_index = -1;
                            f32 candidate_distance = LARGE_FLOAT;
                            
                            v2 a = current_layer->verts[current_layer->vert_count - 1];
                            FORI(0, current_layer->vert_count) {
                                v2 b = current_layer->verts[i];
                                
                                if((v2_inner(a - b, a - new_vertex) > 0.0f) && (v2_inner(b - a, b - new_vertex) > 0.0f) && (v2_cross_prod(b - a, new_vertex - a) < 0.0f)) {
                                    // We found an adequate edge.
                                    ok = true;
                                    
                                    v2 normal = v2_normalize(-v2_perp(b - a));
                                    f32 dist = v2_inner(new_vertex - b, normal);
                                    if(dist < candidate_distance) {
                                        candidate_insert_index = i;
                                        candidate_distance = dist;
                                    }
                                }
                                
                                a = b;
                            }
                            
                            if(ok) {
                                current_layer->vert_count += 1;
                                FORI_REVERSE_NAMED(shift_index, current_layer->vert_count - 1, candidate_insert_index + 1) {
                                    current_layer->verts[shift_index] = current_layer->verts[shift_index - 1];
                                }
                                current_layer->verts[candidate_insert_index] = new_vertex;
                            }
                        } else {
                            current_layer->verts[current_layer->vert_count++] = new_vertex;
                            ok = (current_layer->vert_count >= 3);
                        }
                    }
                } else {
                    // Remove the hovered vert.
                    ok = true;
                    current_layer->vert_count -= 1;
                    FORI(selected_vert, current_layer->vert_count) {
                        current_layer->verts[i] = current_layer->verts[i + 1];
                    }
                    hovered_vert = -1;
                    
                    ASSERT(current_layer->vert_count >= 0);
                }
                
                triangulate_polygon = ok && (current_layer->vert_count >= 3);
            }
        }
    }
    
    if(triangulate_polygon && current_layer->vert_count) {
        { // Triangulating the modified layer.
            SCOPE_MEMORY(&temporary_memory);
            v2 *scratch_verts = push_array(&temporary_memory, v2, current_layer->vert_count);
            u16 *scratch_indices = push_array(&temporary_memory, u16, (current_layer->vert_count - 2) * 3);
            
            FORI(0, current_layer->vert_count) {
                scratch_verts[i] = current_layer->verts[i];
                scratch_indices[i] = CAST(u16, i);
            }
            
            s32 vert_count = current_layer->vert_count;
            s32 index_count = 0;
            
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
                        current_layer->indices[index_count + 0] = scratch_indices[a_index];
                        current_layer->indices[index_count + 1] = scratch_indices[b_index];
                        current_layer->indices[index_count + 2] = scratch_indices[i];
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
            
            current_layer->indices[index_count + 0] = scratch_indices[0];
            current_layer->indices[index_count + 1] = scratch_indices[1];
            current_layer->indices[index_count + 2] = scratch_indices[2];
            index_count += 3;
        }
        
        { // Merging the layers into the mesh.
            s32 running_vert_index = 0;
            s32 running_index_index = 0;
            FORI_NAMED(layer_index, 0, mesh->layer_count) {
                auto layer = &mesh->layers[layer_index];
                
                s32 layer_index_count = s_max(0, (layer->vert_count - 2) * 3);
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
            
            mesh->output.vert_count = running_vert_index;
            mesh->output.index_count = (running_vert_index - 2) * 3;
        }
        
        os_platform.update_editable_mesh(renderer->command_queue.command_list_handle, mesh->output.handle, mesh->output.index_count, false);
    }
    
    //
    // At this point, we should be done updating.
    // The rest of this procedure is draw calls.
    //
    
    //
    // Mesh layer
    //
    
    {
        Mesh_Instance instance = {mesh->output.default_offset, mesh->output.default_scale, mesh->output.default_rot, V4(1.0f)};
        render_mesh(&renderer->command_queue, mesh->output.handle, &instance);
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
                    
                    render_quad(&renderer->command_queue, &highlight);
                }
            }
        }
    }
    
    if(hovered_vert != -1) {
        Mesh_Instance hover_highlight = {};
        
        hover_highlight.offset = current_layer->verts[hovered_vert];
        hover_highlight.scale = V2(0.3f);
        hover_highlight.rot = V2(1.0f, 0.0f);
        hover_highlight.color = V4(0.0f, 1.0f, 0.0f, 1.0f);
        
        render_quad(&renderer->command_queue, &hover_highlight);
    }
    
    //
    // UI layer
    //
    
    { // Color picker
        Mesh_Instance instance = {};
        instance.offset = rect2_center(layout[color_picker]);
        instance.scale = rect2_dim(layout[color_picker]);
        instance.rot = V2(1.0f, 0.0f);
        instance.color = V4(1.0f, 1.0f, 1.0f, 1.0f);
        render_mesh(&renderer->command_queue, RESERVED_MESH_HANDLE::COLOR_PICKER, &instance);
    }
    
    v4 colors[] = {
        V4(1.0f, 0.5f, 1.0f, 1.0f),
        V4(1.0f, 1.0f, 0.5f, 1.0f),
        
        V4(1.0f, 0.3f, 0.3f, 1.0f), // Toggle expand
        
        V4(0.5f, 0.7f, 0.3f, 1.0f), // Add 0
        V4(0.3f, 1.0f, 0.3f, 1.0f), // Add 1
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
            if(i >= mesh->layer_count) {
                color_index += 3;
            }
            instance.color = colors[color_index];
            
            render_quad(&renderer->command_queue, &instance);
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
                } else if(mesh_index >= editor->mesh_count) {
                    color_index += 3;
                }
                instance.color = colors[color_index];
                
                render_quad(&renderer->command_queue, &instance);
            }
        }
    }
    
    //
    // Cursor
    //
    
    {
        Mesh_Instance cursor = {};
        
        cursor.offset = cursor_p;
        cursor.scale = V2(0.1f);
        cursor.rot = v2_angle_to_complex(TAU * 0.125f);
        cursor.color = (mesh->current_layer == 1) ? V4(0.0f, 1.0f, 1.0f, 1.0f) : V4(0.0f, 0.0f, 1.0f, 1.0f);
        
        render_quad(&renderer->command_queue, &cursor);
    }
    
    advance_input(in);
    maybe_flush_draw_commands(&renderer->command_queue);
}

// @Incomplete
void Implicit_Context::export_mesh(Mesh_Editor *editor) {
    SCOPE_MEMORY(&temporary_memory);
    auto mesh = &editor->meshes[editor->current_mesh];
    
    Serialized_Mesh *serialized = push_struct(&temporary_memory, Serialized_Mesh, 1);
    
    // @Hardcoded
    serialized->offset = V2();
    serialized->scale = V2();
    serialized->rot = V2(1.0f, 0.0f);
    serialized->layer_count = 2;
}