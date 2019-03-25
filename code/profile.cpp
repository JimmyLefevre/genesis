
#if GENESIS_DEV

#define PROFILER_STORED_FRAMES PHYSICS_HZ

static void profile_init(Profiler *profiler, Memory_Block *block, s32 thread_count) {
    profiler->thread_count = (s16)thread_count;
    
    // NOTE: Make sure we're aligning each CPU's queue to cache lines.
    push_size(block, 1, 64);
    profiler->timer_queues = push_array(block, Debug_Timer_Queue, thread_count * PROFILER_STORED_FRAMES, 64);
    zero_mem(profiler->timer_queues, ALIGN_POW2(sizeof(Debug_Timer_Queue) * thread_count * PROFILER_STORED_FRAMES, 64));
    
    // NOTE: Make super duper sure
    for(s32 i = 0; i < thread_count; ++i) {
        ASSERT(!((u64)(profiler->timer_queues + i) & 63));
    }
    
    profiler->should_profile = true;
    profiler->selected_frame = -1;
    
    profiler->target_alpha = 0.3f;
    profiler->current_alpha = 0.3f;
    profiler->dalpha = -0.05f;
}

#define THREAD_COLUMN_CHARACTERS 6
#define FILE_COLUMN_CHARACTERS 50
#define FUNCTION_COLUMN_CHARACTERS 35
#define LINE_COLUMN_CHARACTERS 5
#define CYCLE_COLUMN_CHARACTERS 15
#define THREAD_COLUMN_WIDTH (THREAD_COLUMN_CHARACTERS + 1)
#define FILE_COLUMN_WIDTH (FILE_COLUMN_CHARACTERS + 1)
#define FUNCTION_COLUMN_WIDTH (FUNCTION_COLUMN_CHARACTERS + 1)
#define LINE_COLUMN_WIDTH (LINE_COLUMN_CHARACTERS + 1)
#define CYCLE_COLUMN_WIDTH (CYCLE_COLUMN_CHARACTERS + 1)

void Implicit_Context::profile_update(Profiler *profiler, Program_State *program_state, Text_Info *text_info, Rendering_Info *render_info, Input *in) {
    TIME_BLOCK;
    
    profiler->current_alpha = f_move_toward(profiler->current_alpha, profiler->target_alpha, profiler->dalpha);
    
    // NOTE: Variables for text rendering
    const f32 text_height = 0.02f;
    const f32 cw = character_width_for_mono_font(text_info, render_info, FONT_MONO, text_height);
    const f32 lh = line_height_for_font(text_info, render_info, FONT_MONO, text_height);
    f32 alpha = profiler->current_alpha;
    
    v4 red = V4(1.0f, 0.0f, 0.0f, alpha);
    v4 yellow = V4(1.0f, 1.0f, 0.0f, alpha);
    v4 green = V4(0.0f, 1.0f, 0.0f, alpha);
    v4 cyan = V4(0.0f, 1.0f, 1.0f, alpha);
    v4 white = V4(1.0f, 1.0f, 1.0f, alpha);
    v4 black = V4(0.0f, 0.0f, 0.0f, alpha);
    
    f32 at_x = 0.0f;
    f32 at_y = 0.8f;
    
    f32 thread_x = at_x;
    f32 file_x = thread_x + cw * THREAD_COLUMN_WIDTH;
    f32 function_x = file_x + cw * FILE_COLUMN_WIDTH;
    f32 line_x = function_x + cw * FUNCTION_COLUMN_WIDTH;
    f32 cycle_x = line_x + cw * LINE_COLUMN_WIDTH;
    
    // @Duplication from the menu.
    v2 pixel_to_unit_scale = get_pixel_to_unit_scale(render_info);
    v2 cursor_p = v2_hadamard_prod(V2(program_state->cursor_position_in_pixels), pixel_to_unit_scale);
    
    if(BUTTON_PRESSED(in, attack2)) {
        profiler->should_profile = !profiler->should_profile;
    }
    
    s16 current_frame_index = profiler->frame_index;
    
    {
        f32 frame_bar_left = 0.0f;
        f32 frame_bar_top = 0.90f;
        f32 frame_bar_bottom = 0.82f;
        f32 frame_bar_right = 0.5f;
        
        f32 last_right = 0.0f;
        for(s16 i = 0; i < PROFILER_STORED_FRAMES; ++i) {
            f32 left = last_right;
            f32 right = f_lerp(frame_bar_left, frame_bar_right, (f32)(i + 1) / PROFILER_STORED_FRAMES);
            
            rect2 rect = rect2_4f(left, frame_bar_bottom, right, frame_bar_top);
            v4 color = V4(1.0f, 1.0f, 1.0f, alpha);
            bool hovered = point_in_rect2(cursor_p, rect);
            
            if(hovered) {
                if(i == current_frame_index) {
                    color = green;
                } else {
                    color = yellow;
                }
                
                if(BUTTON_PRESSED(in, attack)) {
                    
                    if(profiler->selected_frame == i) {
                        profiler->selected_frame = -1;
                    } else {
                        profiler->selected_frame = i;
                    }
                }
            } else if(i == current_frame_index) {
                color = red;
            }
            
            if(i == profiler->selected_frame) {
                color = cyan;
            }
            
            queue_quad_minmax(render_info, render_info->blank_texture_id, rect.min, rect.max, color);
            last_right = right;
        }
    }
    
    if(program_state->should_render) {
        text_add_string(text_info, render_info, STRING("Thread"), V2(thread_x, at_y), text_height, FONT_MONO, false, white);
        text_add_string(text_info, render_info, STRING("File"), V2(file_x, at_y), text_height, FONT_MONO, false, white);
        text_add_string(text_info, render_info, STRING("Function"), V2(function_x, at_y), text_height, FONT_MONO, false, white);
        text_add_string(text_info, render_info, STRING("Line"), V2(line_x, at_y), text_height, FONT_MONO, false, white);
        text_add_string(text_info, render_info, STRING("Cycles"), V2(cycle_x, at_y), text_height, FONT_MONO, false, white);
        at_y -= lh;
    }
    
    for(s16 ithread = 0; ithread < profiler->thread_count; ++ithread) {
        Memory_Block_Frame _mbf = Memory_Block_Frame(&temporary_memory);
        
        s64 queue_to_display;
        if(profiler->selected_frame == -1) {
            queue_to_display = profiler->frame_index;
        } else {
            queue_to_display = profiler->selected_frame;
        }
        queue_to_display *= profiler->thread_count;
        
        Debug_Timer_Queue *queue = profiler->timer_queues + queue_to_display + ithread;
        u32 count = queue->count;
        
        string thread = tprint("%d", ithread);
        
        logprint(global_log, "///\n///Debug timer queue: Thread %d\n///\n", ithread);
        for(u32 itimer = 0; itimer < count; ++itimer) {
            // @Incomplete: Sort these timers by cycles elapsed.
            Debug_Timer timer = queue->timers[itimer];
            u64 elapsed = timer.end_time - timer.start_time;
            
            string file     = substring(&timer.location, 0,                              timer.location_function_offset);
            file = advance_to_after_last_occurrence(file, '\\');
            string function = substring(&timer.location, timer.location_function_offset, timer.location_line_offset);
            function = advance_to_after_last_occurrence(function, ':');
            string line     = substring(&timer.location, timer.location_line_offset,     timer.location.length);
            
            if(program_state->should_render) {
                v2 thread_p   = V2(thread_x, at_y);
                v2 file_p     = V2(file_x, at_y);
                v2 function_p = V2(function_x, at_y);
                v2 line_p     = V2(line_x, at_y);
                v2 cycle_p    = V2(cycle_x, at_y);
                
                if(file.length > FILE_COLUMN_CHARACTERS) {
                    file.length = FILE_COLUMN_CHARACTERS;
                }
                if(function.length > FUNCTION_COLUMN_CHARACTERS) {
                    function.length = FUNCTION_COLUMN_CHARACTERS;
                }
                if(line.length > LINE_COLUMN_CHARACTERS) {
                    line.length = LINE_COLUMN_CHARACTERS;
                }
                
                string cycles = tprint("%dcy", timer.end_time - timer.start_time);
                
                text_add_string(text_info, render_info, thread, thread_p, text_height, FONT_MONO, false, white);
                text_add_string(text_info, render_info, file, file_p, text_height, FONT_MONO, false, white);
                text_add_string(text_info, render_info, function, function_p, text_height, FONT_MONO, false, white);
                text_add_string(text_info, render_info, line, line_p, text_height, FONT_MONO, false, white);
                text_add_string(text_info, render_info, cycles, cycle_p, text_height, FONT_MONO, false, white);
                
                at_y -= lh;
            }
            logprint(global_log, "Debug timer: %s \n    %u -> %u\n    %d elapsed\n",
                     timer.location.data, timer.start_time, timer.end_time, timer.end_time - timer.start_time);
        }
    }
    os_platform.print(global_log->temporary_buffer);
    
    
    if(profiler->should_profile) {
        profiler->frame_index = (profiler->frame_index + 1) % PROFILER_STORED_FRAMES;
        
        for(s16 i = 0; i < profiler->thread_count; ++i) {
            profiler->timer_queues[profiler->frame_index * profiler->thread_count + i].count = 0;
        }
    }
    
    // This second render pass acts as an overlay.
    text_update(text_info, render_info);
    draw_queue(render_info);
}

#else
#define TIME_BLOCK
#define profile_init(...)
#define profile_update(...)
#endif
