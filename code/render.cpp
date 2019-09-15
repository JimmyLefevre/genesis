
static Texture_ID _find_texture_id(Rendering_Info *render, u32 texture_uid) {
    return render->textures_by_uid[texture_uid];
}
#define FIND_TEXTURE_ID(render_info, tag) _find_texture_id(render_info, TEXTURE_UID_##tag)

static void begin_shader(Shader *shader) {
    glUseProgram(shader->program);
    glUniform1i(shader->texture_sampler, 0);
    
    glEnableVertexAttribArray(shader->p);
    glEnableVertexAttribArray(shader->uv);
    glEnableVertexAttribArray(shader->color);
    glVertexAttribPointer(shader->p, 2, GL_FLOAT, false, sizeof(Render_Vertex), STRUCT_FIELD_OFFSET(p, Render_Vertex));
    glVertexAttribPointer(shader->uv, 2, GL_FLOAT, false, sizeof(Render_Vertex), (GLvoid *)STRUCT_FIELD_OFFSET(uv, Render_Vertex));
    glVertexAttribPointer(shader->color, 4, GL_FLOAT, false, sizeof(Render_Vertex), (GLvoid *)STRUCT_FIELD_OFFSET(color, Render_Vertex));
}

static void end_shader(Shader *shader) {
    glDisableVertexAttribArray(shader->p);
    glDisableVertexAttribArray(shader->uv);
    glDisableVertexAttribArray(shader->color);
}

static void draw_queue(Rendering_Info * const info) {
    Memory_Block * const rq = &info->render_queue;
    f32 *vertTransform = 0;
    u32 runningVertIndex = 0;
    Shader *current_shader = 0;
    
    // Common init:
    glBindBuffer(GL_ARRAY_BUFFER, info->idGeneralVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, info->vert_count * sizeof(Render_Vertex), info->verts, GL_STREAM_DRAW);
    
    for(Render_Entry *this_entry = (Render_Entry *)rq->mem;
        ((usize)this_entry - (usize)rq->mem) < rq->used;
        ++this_entry) {
        Render_Entry_Header header = this_entry->header;
        switch (header.type) {
            case RENDER_ENTRY_TYPE_CLEAR: {
                Render_Entry_Clear clear = this_entry->clear;
                draw_clear(clear.color);
            } break;
            
            case RENDER_ENTRY_TYPE_TRANSFORM: {
                Render_Entry_Transform *transform = &this_entry->transform;
                vertTransform = (f32 *)(info->transforms + transform->index);
                
                glUniformMatrix4fv(current_shader->transform, 1, GL_FALSE, vertTransform);
            } break;
            
            case RENDER_ENTRY_TYPE_SHADER: {
                Render_Entry_Shader *entry = &this_entry->shader;
                // end_shader(current_shader); Leaving this here in case some other GPUs prefer it.
                Shader *s = info->shaders + entry->index;
                
                begin_shader(s);
                current_shader = s;
            } break;
            
            case RENDER_ENTRY_TYPE_DRAW_TO_TEXTURE_BEGIN: {
                Render_Entry_Target *target = &this_entry->target;
                Render_Target tex = info->render_targets[target->index];
                
                glBindFramebuffer(GL_FRAMEBUFFER, tex.target_handle);
                glViewport(0, 0, tex.dim.x, tex.dim.y);
            } break;
            
            case RENDER_ENTRY_TYPE_DRAW_TO_TEXTURE_END: {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                gl_set_render_region(info->draw_rect);
            } break;
            
            case RENDER_ENTRY_TYPE_ATLAS_QUADS: {
                Render_Entry_Atlas_Quads quads = this_entry->atlas_quads;
                u32 vert_count = quads.count * 6;
                
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, quads.atlas);
                glDrawArrays(GL_TRIANGLES, runningVertIndex, vert_count);
                
                runningVertIndex += vert_count;
            } break;
            
            default: UNHANDLED;
        }
    }
    
    rq->used = 0;
    info->vert_count = 0;
    info->quad_count = 0;
    info->last_shader_queued = SHADER_INDEX_COUNT;
}

static u8 render_add_atlas(Rendering_Info *info, u8 *data, u16 atlas_width, u16 atlas_height,
                           s16 texture_count, Texture_Framing *texture_framings,
                           Texture_ID *out_ids) {
    u8 add_atlas = info->atlas_count;
    ASSERT(add_atlas < MAX_ATLAS_COUNT);
    s16 base_texture_count = info->texture_count;
    
    mem_copy(texture_framings, info->texture_framings + info->texture_count, sizeof(Texture_Framing) * texture_count);
    Texture_ID id;
    id.atlas = add_atlas;
    for(s32 i = 0; i < texture_count; ++i) {
        id.texture = base_texture_count + i;
        out_ids[i] = id;
    }
    
    info->atlas_handles[add_atlas] = upload_texture_to_gpu_rgba8(data, atlas_width, atlas_height);
    info->texture_count += texture_count;
    info->atlas_count = add_atlas + 1;
    return add_atlas;
}

static Render_Entry *queue_shader(Rendering_Info *info, u32 index) {
    if(info->last_shader_queued == index) return 0;
    
    Render_Entry *result = push_struct(&info->render_queue, Render_Entry);
    result->header.type = RENDER_ENTRY_TYPE_SHADER;
    result->shader.index = index;
    info->last_shader_queued = index;
    return result;
}

/* GL matrices:
 0   1  2  3
  4   5  6  7
   8   9 10 11
  12 13 14 15
*/
inline Render_Entry *queue_transform(Rendering_Info * const info, const u32 index) {
    Render_Entry *result = push_struct(&info->render_queue, Render_Entry);
    result->header.type = RENDER_ENTRY_TYPE_TRANSFORM;
    result->transform.index = index;
    
    f32 *mat = (f32 *)info->transforms[index];
    info->one_pixel = v2_element_wise_div(V2(2.0f / mat[0], 2.0f / mat[5]), info->render_target_dim);
    
    return result;
}
inline Render_Entry *queue_transform_game_camera(Rendering_Info *info, v2 p, v2 rot, f32 zoom) {
    f32 *mat = (f32 *)(info->transforms[TRANSFORM_INDEX_GAME]);
    
    mat[0] = (1.0f / 8.0f) * rot.x;
    mat[1] = (1.0f / 4.5f) * -rot.y;
    mat[4] = (1.0f / 8.0f) * rot.y;
    mat[5] = (1.0f / 4.5f) * rot.x;
    mat[12] = -(p.x * mat[0] + p.y * mat[4]);
    mat[13] = -(p.x * mat[1] + p.y * mat[5]);
    mat[15] = 1.0f / zoom;
    
    return queue_transform(info, TRANSFORM_INDEX_GAME);
}

static Render_Entry *queue_clear(Rendering_Info *info, v4 color = V4(0.0f)) {
    Render_Entry *result = push_struct(&info->render_queue, Render_Entry);
    result->header.type = RENDER_ENTRY_TYPE_CLEAR;
    result->clear.color = color;
    return result;
}

static Render_Entry *get_current_or_new_entry(Rendering_Info *info, u8 type) {
    if(!info->next_entry_should_be_new) {
        Render_Entry *last_entry = (Render_Entry *)(info->render_queue.mem + info->render_queue.used - sizeof(Render_Entry));
        if(last_entry->header.type == type) return last_entry;
    }
    Render_Entry *result = push_struct(&info->render_queue, Render_Entry);
    result->header.type = type;
    result->clear.color = V4();
    info->next_entry_should_be_new = false;
    return result;
}

inline bool can_allocate_verts(Rendering_Info *info, u32 vert_count) {
    bool result = (info->vert_count + vert_count) < MAX_VERT_COUNT ;
    return result;
}

inline Render_Vertex *push_verts(Rendering_Info *info, u32 vertCount) {
    ASSERT(can_allocate_verts(info, vertCount));
    Render_Vertex *result = info->verts + info->vert_count;
    info->vert_count += vertCount;
    return result;
}

static Render_Entry *queue_quad(Rendering_Info *info, u32 atlas, v2 uvmin, v2 uvmax,
                                v2 p, v2 halfdim, v2 rotation, v2 offset, v4 color) {
    if(can_allocate_verts(info, 6)) {
        Render_Entry *result = get_current_or_new_entry(info, RENDER_ENTRY_TYPE_ATLAS_QUADS);
        Render_Vertex *verts = push_verts(info, 6);
        
        const v2 halfdim_flipy = V2(halfdim.x, -halfdim.y);
        if(result->atlas_quads.count && result->atlas_quads.atlas != atlas) {
            result = push_struct(&info->render_queue, Render_Entry);
            result->header.type = RENDER_ENTRY_TYPE_ATLAS_QUADS;
            result->atlas_quads.count = 0;
        }
        result->atlas_quads.atlas = atlas;
        result->atlas_quads.count += 1;
        
        p += offset;
        const v2 bottom_left  = p + v2_complex_prod(-halfdim      , rotation);
        const v2 bottom_right = p + v2_complex_prod( halfdim_flipy, rotation);
        const v2 top_right    = p + v2_complex_prod( halfdim      , rotation);
        const v2 top_left     = p + v2_complex_prod(-halfdim_flipy, rotation);
        
        Render_Vertex vert0, vert1, vert2, vert5;
        vert0.p = bottom_left;
        vert0.uv = uvmin;
        vert0.color = color;
        vert1.p = bottom_right;
        vert1.uv = V2(uvmax.x, uvmin.y);
        vert1.color = color;
        vert2.p = top_right;
        vert2.uv = uvmax;
        vert2.color = color;
        vert5.p = top_left;
        vert5.uv = V2(uvmin.x, uvmax.y);
        vert5.color = color;
        
        verts[0] = vert0;
        verts[1] = vert1;
        verts[2] = vert2;
        verts[3] = vert0;
        verts[4] = vert2;
        verts[5] = vert5;
        return result;
    }
    return 0;
}
inline Render_Entry *queue_quad(Rendering_Info *info, Texture_ID id, v2 p, v2 halfdim, v2 rotation, v4 color) {
    const Texture_Framing framing = info->texture_framings[id.texture];
    u32 atlas = info->atlas_handles[id.atlas];
    halfdim = v2_hadamard_prod(framing.halfdim, halfdim);
    return queue_quad(info, atlas, framing.uvs.min, framing.uvs.max, p, halfdim, rotation, framing.offset, color);
}

inline Render_Entry *queue_quad_minmax(Rendering_Info *info, Texture_ID id, v2 min, v2 max, v4 color) {
    const Texture_Framing framing = info->texture_framings[id.texture];
    u32 atlas = info->atlas_handles[id.atlas];
    v2 p = v2_lerp(min, max, 0.5f);
    v2 halfdim = (max - min) * 0.5f;
    halfdim = v2_hadamard_prod(framing.halfdim, halfdim);
    return queue_quad(info, atlas, framing.uvs.min, framing.uvs.max, p, halfdim, V2(1.0f, 0.0f), framing.offset, color);
}

// @Duplication
inline Render_Entry *queue_quad_minmax(Rendering_Info *info, u32 atlas, v2 uvmin, v2 uvmax, v2 min, v2 max, v2 offset, v4 color) {
    if(can_allocate_verts(info, 6)) {
        Render_Entry *result = get_current_or_new_entry(info, RENDER_ENTRY_TYPE_ATLAS_QUADS);
        Render_Vertex *verts = push_verts(info, 6);
        
        if(result->atlas_quads.count && result->atlas_quads.atlas != atlas) {
            result = push_struct(&info->render_queue, Render_Entry);
            result->header.type = RENDER_ENTRY_TYPE_ATLAS_QUADS;
            result->atlas_quads.count = 0;
        }
        result->atlas_quads.atlas = atlas;
        result->atlas_quads.count += 1;
        
        Render_Vertex vert0, vert1, vert2, vert5;
        vert0.p     = min;
        vert0.uv    = uvmin;
        vert0.color = color;
        vert1.p     = V2(  max.x,   min.y);
        vert1.uv    = V2(uvmax.x, uvmin.y);
        vert1.color = color;
        vert2.p     =   max;
        vert2.uv    = uvmax;
        vert2.color = color;
        vert5.p     = V2(  min.x,   max.y);
        vert5.uv    = V2(uvmin.x, uvmax.y);
        vert5.color = color;
        
        verts[0] = vert0;
        verts[1] = vert1;
        verts[2] = vert2;
        verts[3] = vert0;
        verts[4] = vert2;
        verts[5] = vert5;
        return result;
    }
    return 0;
}

inline Render_Entry *queue_circle_pradius(Rendering_Info *info, v2 p, f32 radius, v4 color = V4(1.0f)) {
    return queue_quad(info, info->circle_id, p, V2(radius), V2(1.0f, 0.0f), color);
}

static void queue_line_strip(Rendering_Info *info, v2 *ps, u32 count, f32 thickness, v4 color = V4(1.0f)) {
    if(can_allocate_verts(info, (count - 1) * 6)) {
        for(u32 i = 1; i < count; ++i) {
            v2 a = ps[i-1];
            v2 b = ps[i];
            v2 midpoint = v2_lerp(a, b, 0.5f);
            v2 midpointb = b - midpoint;
            f32 midpointb_length = v2_length(b - midpoint);
            v2 halfdim = V2(thickness + midpointb_length, thickness);
            v2 rot = (b - midpoint) / midpointb_length;
            
            queue_quad(info, info->blank_texture_id, midpoint, halfdim, rot, color);
        }
    }
}

inline Render_Entry *queue_draw_to_texture_begin(Rendering_Info *info, u32 index) {
    Render_Entry *result = push_struct(&info->render_queue, Render_Entry);
    result->header.type = RENDER_ENTRY_TYPE_DRAW_TO_TEXTURE_BEGIN;
    result->target.index = index;
    return result;
}

inline Render_Entry *queue_draw_to_texture_end(Rendering_Info *info) {
    Render_Entry *result = push_struct(&info->render_queue, Render_Entry);
    result->header.type = RENDER_ENTRY_TYPE_DRAW_TO_TEXTURE_END;
    return result;
}

static void maybe_update_render_size(Rendering_Info *ri, s32 new_dim[2]) {
    rect2s new_draw_rect = rect2s_fit((s32)GAME_ASPECT_RATIO_X, (s32)GAME_ASPECT_RATIO_Y, new_dim[0], new_dim[1]);
    
    if(mem_compare(&ri->draw_rect, &new_draw_rect, sizeof(rect2s))) {
        ri->render_target_dim = V2((f32)(new_draw_rect.right - new_draw_rect.left),
                                   (f32)(new_draw_rect.top - new_draw_rect.bottom));
        ri->draw_rect = new_draw_rect;
        
        gl_set_render_region(new_draw_rect);
    }
}
