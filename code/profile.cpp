
#if GENESIS_DEV

#define PROFILER_STORED_FRAMES PHYSICS_HZ

static inline void unpack_to_location_strings(Debug_Timer *timer, string *file, string *function, string *line) {
    *file     = substring(&timer->location, 0,                              timer->location_function_offset);
    *file = advance_to_after_last_occurrence(*file, '\\');
    *function = substring(&timer->location, timer->location_function_offset, timer->location_line_offset);
    *function = advance_to_after_last_occurrence(*function, ':');
    *line     = substring(&timer->location, timer->location_line_offset,     timer->location.length);
}

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
    
    sub_block(&profiler->log_block, block, MiB(1));
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

static string profile_log_frame(Memory_Block *block, Profiler *profiler, s16 frame) {
    s16 thread_count = profiler->thread_count;
    auto *queues = profiler->timer_queues + (frame * thread_count);
    
    string log_scratch = push_string(block, KiB(4), 1);
    string log_written = make_string(log_scratch.data, 0);
    
    log_written.length += print(log_written.data + log_written.length, log_scratch.length - log_written.length,
                                "F%d\n", frame);
    for(s32 ithread = 0; ithread < thread_count; ++ithread) {
        log_written.length += print(log_written.data + log_written.length, log_scratch.length - log_written.length,
                                    "T%d\n", ithread);
        auto *queue = queues + ithread;
        s32 count = queue->count;
        
        for(s32 itimer = 0; itimer < count; ++itimer) {
            Debug_Timer *timer = queue->timers + itimer;
            s64 elapsed = timer->end_time - timer->start_time;
            
            string file;
            string function;
            string line;
            unpack_to_location_strings(timer, &file, &function, &line);
            
            append_string(&log_written, file, log_scratch.length);
            append_string(&log_written, STRING(" "), log_scratch.length);
            append_string(&log_written, function, log_scratch.length);
            append_string(&log_written, STRING(" "), log_scratch.length);
            append_string(&log_written, line, log_scratch.length);
            append_string(&log_written, STRING(" "), log_scratch.length);
            
            log_written.length += print(log_written.data + log_written.length, log_scratch.length - log_written.length,
                                        "%u->%u %u\n", timer->start_time, timer->end_time, elapsed);
        }
    }
    
    // Collapse our allocation if we didn't use it all
    block->used -= (log_scratch.length - log_written.length);
    
    return log_written;
}
static inline string profile_log_frame(Profiler *profiler, s16 frame) {
    return profile_log_frame(&profiler->log_block, profiler, frame);
}

static string profile_log_all_frames(Memory_Block *block, Profiler *profiler) {
    string result;
    result.data = block->mem + block->used;
    usize used_before = block->used;
    
    for(s16 i = 0; i < PROFILER_STORED_FRAMES; ++i) {
        profile_log_frame(block, profiler, (profiler->frame_index + 1 + i) % PROFILER_STORED_FRAMES);
    }
    
    result.length = block->used - used_before;
    
    return result;
}
static inline string profile_log_all_frames(Profiler *profiler) {
    return profile_log_all_frames(&profiler->log_block, profiler);
}

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
    
    f32 scroll = profiler->scroll;
    {
        if(BUTTON_DOWN(in, up)) {
            scroll += 0.007f;
        }
        if(BUTTON_DOWN(in, down)) {
            scroll -= 0.007f;
        }
        
        if(scroll < 0.0f) {
            scroll = 0.0f;
        }
        
        at_y += scroll;
    }
    
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
    
    // This is only used for snapping the scroll to the bottom of the timer list.
    f32 bottommost_timer = 1.0f;
    
    {
        f32 frame_bar_left = 0.0f;
        f32 frame_bar_top = at_y + 0.1f;
        f32 frame_bar_bottom = at_y + 0.02f;
        f32 frame_bar_right = 0.5f;
        
        // Culling:
        if(frame_bar_bottom <= 1.0f) {
            
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
                        // Toggle selection.
                        if(profiler->selected_frame == i) {
                            profiler->selected_frame = -1;
                        } else {
                            profiler->selected_frame = i;
                        }
                    } else if(BUTTON_DOWN(in, attack) && (profiler->selected_frame != -1)) {
                        // Slide our selection with the cursor.
                        profiler->selected_frame = i;
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
    }
    
    if(BUTTON_PRESSED(in, slot1)) {
        headsup(global_log, STRING("Logging frame..."));
        string log = profile_log_frame(profiler, profiler->selected_frame);
        // @Cleanup: Put this in a file or something to be able to save different logs.
        os_platform.write_entire_file(STRING("0000.log"), log);
        
        profiler->log_block.used = 0;
    }
    if(BUTTON_PRESSED(in, slot2)) {
        headsup(global_log, STRING("Logging all frames..."));
        string log = profile_log_all_frames(profiler);
        // @Cleanup: See above.
        os_platform.write_entire_file(STRING("all0000.log"), log);
        
        profiler->log_block.used = 0;
    }
    
    // At this point, input should be completely processed.
    
    
    if(program_state->should_render) {
        text_add_string(text_info, render_info, STRING("Thread"), V2(thread_x, at_y), text_height, FONT_MONO, false, white);
        text_add_string(text_info, render_info, STRING("File"), V2(file_x, at_y), text_height, FONT_MONO, false, white);
        text_add_string(text_info, render_info, STRING("Function"), V2(function_x, at_y), text_height, FONT_MONO, false, white);
        text_add_string(text_info, render_info, STRING("Line"), V2(line_x, at_y), text_height, FONT_MONO, false, white);
        text_add_string(text_info, render_info, STRING("Cycles"), V2(cycle_x, at_y), text_height, FONT_MONO, false, white);
        at_y -= lh;
    }
    
    s16 *sort_indices = push_array(&temporary_memory, s16, DEBUG_TIMERS_PER_CPU);
    
    for(s16 ithread = 0; ithread < profiler->thread_count; ++ithread) {
        SCOPE_MEMORY(&temporary_memory);
        
        s64 queue_to_display;
        if(profiler->selected_frame == -1) {
            queue_to_display = profiler->frame_index;
        } else {
            queue_to_display = profiler->selected_frame;
        }
        queue_to_display *= profiler->thread_count;
        
        Debug_Timer_Queue *queue = profiler->timer_queues + queue_to_display + ithread;
        s32 count = queue->count;
        
        string thread = tprint("%d", ithread);
        
        { // Sorting timers: @Speed Nsquared sort!
            // Sort init:
            for(s32 i = 0; i < count; ++i) {
                sort_indices[i] = (s16)i;
            }
            
            {
                for(s32 i = 0; i < count; ++i) {
                    s32 best_timer = i;
                    Debug_Timer *timer = queue->timers + i;
                    s64 elapsed = timer->end_time - timer->start_time;
                    
                    for(s32 j = i + 1; j < count; ++j) {
                        Debug_Timer *other = queue->timers + j;
                        s64 other_elapsed = other->end_time - other->start_time;
                        
                        if(other_elapsed > elapsed) {
                            best_timer = j;
                            timer = other;
                            elapsed = other_elapsed;
                        }
                    }
                    
                    sort_indices[i] = (s16)best_timer;
                }
            }
            
            // Sort end:
            for(s32 i = 0; i < count; ++i) {
                SWAP(queue->timers[i], queue->timers[sort_indices[i]]);
            }
        }
        
        for(s32 itimer = 0; itimer < count; ++itimer) {
            Debug_Timer timer = queue->timers[itimer];
            u64 elapsed = timer.end_time - timer.start_time;
            
            string file;
            string function;
            string line;
            unpack_to_location_strings(&timer, &file, &function, &line);
            
            bool is_visible = ((at_y - lh) <= 1.0f) && ((at_y + lh) >= 0.0f);
            
            if(program_state->should_render && is_visible) {
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
                
                bottommost_timer = at_y;
            }
            
            at_y -= lh;
        }
    }
    os_platform.print(global_log->temporary_buffer);
    
    // One frame lag, but ehh, it works well enough.
    if(bottommost_timer > 0.01f) {
        scroll = profiler->scroll;
    }
    profiler->scroll = scroll;
    
    if(profiler->should_profile) {
        profiler->frame_index = (profiler->frame_index + 1) % PROFILER_STORED_FRAMES;
        
        for(s16 i = 0; i < profiler->thread_count; ++i) {
            profiler->timer_queues[profiler->frame_index * profiler->thread_count + i].count = 0;
        }
    }
    
    // This second render pass acts as an overlay.
    text_update(text_info, render_info);
    draw_queue(render_info);
    
    advance_input(in);
}

#else
#define TIME_BLOCK
#define profile_init(...)
#define profile_update(...)
#endif
