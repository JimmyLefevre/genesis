
static inline void open_menu(Game_Client *g_cl) {
    os_platform.release_cursor();
    g_cl->in_menu = true;
}

static inline void close_menu(Game_Client *g_cl) {
    os_platform.capture_cursor();
    g_cl->in_menu = false;
}

static void update_slider(rect2 *aabb, Slider *slider, v2 p) {
    f32 lerp_factor = (p.x - aabb->left) / (aabb->right - aabb->left);
    lerp_factor = f_clamp(lerp_factor, 0.0f, 1.0f);
    *slider->actual = f_lerp(slider->min, slider->max, lerp_factor);
}

// The convention in these add_* functions is that we scale positions by the frame,
// but not dimensions.
static Slider *add_slider(rect2 *aabbs, Slider *sliders, s32 *count, rect2 frame,
                          v2 base, f32 length, f32 thickness, f32 min, f32 max, f32 *actual) {
    ASSERT(min != max);
    const f32 half_thickness = thickness * 0.5f;
    const f32 frame_width = frame.right - frame.left;
    const f32 frame_height = frame.top - frame.bottom;
    rect2 aabb;
    aabb.left   = frame.  left + base.x * frame_width;
    aabb.bottom = frame.bottom + base.y * frame_height - half_thickness;
    aabb.right  = aabb.   left +    length;
    aabb.top    = aabb. bottom + thickness;
    aabb = rect2_clip(aabb, frame);
    
    Slider slider;
    slider.   min =    min;
    slider.   max =    max;
    slider.actual = actual;
    
    aabbs  [*count] =   aabb;
    Slider *result = sliders + *count;
    *result = slider;
    
    *count += 1;
    
    return result;
}

static inline v2 framed_p(v2 p, rect2 frame) {
    v2 frame_dim = frame.max - frame.min;
    v2 result = frame.min + v2_hadamard_prod(p, frame_dim);
    return result;
}

static inline rect2 framed_rect_pdim(v2 p, v2 dim, rect2 frame) {
    v2 frame_dim = frame.max - frame.min;
    rect2 result = rect2_pdim(frame.min + v2_hadamard_prod(p, frame_dim), dim);
    result = rect2_clip(result, frame);
    
    return result;
}

static Button *add_button(rect2 *aabbs, Button *buttons, s32 *count, rect2 frame,
                          v2 p, v2 dim, any32 set_to, any32 *actual) {
    rect2 aabb = framed_rect_pdim(p, dim, frame);
    
    Button button;
    button.set_to = set_to;
    button.actual = actual;
    
    aabbs[*count] = aabb;
    Button *result = buttons + *count;
    *result = button;
    
    *count += 1;
    
    return result;
}

static Toggle *add_toggle(rect2 *aabbs, Toggle *toggles, s32 *count, rect2 frame,
                          v2 p, v2 dim, bool *actual) {
    rect2 aabb = framed_rect_pdim(p, dim, frame);
    
    Toggle toggle;
    toggle.actual = actual;
    
    aabbs  [*count] =   aabb;
    Toggle *result = toggles + *count;
    *result = toggle;
    
    *count += 1;
    
    return result;
}

static Dropdown *add_dropdown(rect2 *aabbs, Dropdown *dropdowns, s32 *count, rect2 frame,
                              v2 p, v2 dim, s32 option_count, any32 *actual,
                              s32 default, Memory_Block *block) {
    ASSERT((s8)option_count == option_count);
    ASSERT((s8)     default ==      default);
    ASSERT(default < option_count);
    rect2 aabb = framed_rect_pdim(p, dim, frame);
    
    Dropdown dropdown;
    dropdown.options = push_array(block, Dropdown_Option, option_count);
    dropdown.actual = actual;
    dropdown.option_count = (s8)option_count;
    dropdown.active = (s8)default;
    dropdown.hover = -1;
    
    if(dropdown.actual) {
        *dropdown.actual = dropdown.options[dropdown.active].value;
    }
    
    aabbs[*count] = aabb;
    Dropdown *result = dropdowns + *count;
    *result = dropdown;
    
    *count += 1;
    return result;
}

static Keybind *add_keybind(rect2 *aabbs, Keybind *keybinds, s32 *count, rect2 frame,
                            v2 p, v2 dim, u32 *actual) {
    rect2 aabb = framed_rect_pdim(p, dim, frame);
    
    Keybind keybind;
    keybind.actual = actual;
    
    aabbs[*count] = aabb;
    Keybind *result = keybinds + *count;
    *result = keybind;
    
    *count += 1;
    
    return result;
}

static Menu_Label *add_label(Menu_Label *labels, s32 *count, rect2 frame,
                             v2 p, f32 pixel_height, string s, s8 font, bool centered) {
    Menu_Label label;
    
    label.s = s;
    label.p = framed_p(p, frame);
    label.pixel_height = pixel_height;
    label.font = font;
    label.centered = centered;
    
    Menu_Label *result = labels + *count;
    *result = label;
    *count += 1;
    
    return result;
}

static Dropdown_Option *add_dropdown_options(Dropdown_Option *options, s32 *count,
                                             Dropdown_Option *to_add, s32 add_count) {
    Dropdown_Option *result = options + *count;
    
    FORI(0, add_count) {
        result[i] = to_add[i];
    }
    
    *count += add_count;
    return result;
}

static void update_menu(Menu *menu, Rendering_Info *render, Input *in, Program_State *program_state) {
    v2 pixel_to_unit_scale = get_pixel_to_unit_scale(render);
    v2 cursor_p = v2_hadamard_prod(V2(program_state->cursor_position_in_pixels), pixel_to_unit_scale);
    
    auto          *page = menu->pages + menu->current_page;
    v2 cursor_p_clamped = v2_clamp(cursor_p, 0.0f, 1.0f);
    auto    interaction = menu->interaction;
    Interaction   hover = make_interaction();
    
    bool found_hovered = false;
    
    // These are all the specific input paths that don't fit
    // in the generic handling.
    // They are multi-step interactions (dropdowns) or held
    // interactions (sliders); basically anything that needs
    // more data points than on frames when we click.
    if(interaction.type == WIDGET_TYPE_DROPDOWN) {
        rect2 aabb     = page->dropdown_aabbs[interaction.index];
        auto dropdown = page->dropdowns +    interaction.index ;
        s8 dropdown_hover = -1;
        
        for(s32 i = 0; i < dropdown->option_count; ++i) {
            const rect2 option_aabb = rect2_4f(aabb. left, aabb.bottom - 0.1f * i,
                                               aabb.right, aabb.   top - 0.1f * i);
            if(point_in_rect2(cursor_p_clamped, option_aabb)) {
                dropdown_hover = (s8)i;
                
                break;
            }
        }
        
        // If any option is hovered, then the dropdown is hovered.
        if(dropdown_hover != -1) {
            hover = interaction;
            found_hovered = true;
        }
        dropdown->hover = dropdown_hover;
    } else if(interaction.type == WIDGET_TYPE_SLIDER) {
        rect2 aabb  = page->slider_aabbs[interaction.index];
        auto slider = page->sliders     [interaction.index];
        
        if(BUTTON_DOWN(in, attack)) {
            update_slider(&aabb, &slider, cursor_p_clamped);
        } else {
            // We dropped the interaction.
            interaction = make_interaction();
        }
    } else if(interaction.type == WIDGET_TYPE_KEYBIND) {
        auto keybind = page->keybinds[interaction.index];
        u32 last_key_pressed = program_state->last_key_pressed;
        
        if((last_key_pressed != GK_UNHANDLED) && (last_key_pressed != program_state->input_settings.bindings[GAME_BUTTON_INDEX_menu])) {
            // Bind the key.
            for(s32 i = 0; i < GAME_BUTTON_COUNT; ++i) {
                if(program_state->input_settings.bindings[i] == last_key_pressed) {
                    unpress_button(&program_state->input, i);
                    program_state->input_settings.bindings[i] = GK_UNHANDLED;
                    
                    break;
                }
            }
            
            *keybind.actual = last_key_pressed;
            
            interaction = make_interaction();
        }
    } else {
        interaction = make_interaction();
    }
    
    // Generic hit test.
    if(!found_hovered) {
        for(s32 itype = 0; itype < (WIDGET_TYPE_COUNT - 1); ++itype) {
            rect2 *aabbs      = page-> aabbs_by_type[itype];
            s32    aabb_count = page->counts_by_type[itype];
            
            for(s32 iaabb = 0; iaabb < aabb_count; ++iaabb) {
                rect2 aabb = aabbs[iaabb];
                
                if(point_in_rect2(cursor_p_clamped, aabb)) {
                    hover = make_interaction(itype + 1, iaabb);
                    found_hovered= true;
                    break;
                }
            }
            
            if(found_hovered) {
                break;
            }
        }
    }
    
    // Interaction resolution.
    if(BUTTON_PRESSED(in, attack)) {
        interaction = hover;
        
        switch(interaction.type) {
            case WIDGET_TYPE_BUTTON: {
                auto *button = page->buttons + interaction.index;
                
                *button->actual = button->set_to;
            } break;
            
            case WIDGET_TYPE_TOGGLE: {
                auto *toggle = page->toggles + interaction.index;
                
                *toggle->actual = !*toggle->actual;
            } break;
            
            case WIDGET_TYPE_DROPDOWN: {
                auto *dropdown = page->dropdowns + interaction.index;
                
                if(dropdown->hover == -1) {
                    // First frame; we're opening the dropdown.
                    // We're assuming that the first option will always overlap the original dropdown.
                    dropdown->hover = 0;
                } else {
                    // The dropdown is open; find the right option and apply it.
                    *dropdown->actual = dropdown->options[dropdown->hover].value;
                    dropdown->active  = (s8)dropdown->hover;
                    dropdown->hover   = -1;
                    interaction = make_interaction();
                }
            } break;
            
            case WIDGET_TYPE_SLIDER: {
                auto *slider = page->sliders + interaction.index;
                
                update_slider(page->slider_aabbs + interaction.index, slider, cursor_p_clamped);
            } break;
            
            default: {
            } break;
        }
    }
    
    advance_input(in);
    menu->interaction = interaction;
    menu->hover = hover;
}

void Implicit_Context::draw_menu(Menu *menu, Game_Client *g_cl, Rendering_Info *render_info, Text_Info *text_info) {
    const f32 inv_render_target_height = 1.0f / render_info->render_target_dim.y;
    
    if(menu->should_display_text) {
        f32 line_advance        = text_font_line_advance(text_info, FONT_REGULAR);
        f32 line_advance_scaled = line_advance * 56.0f * inv_render_target_height;
        v2 at = V2(0.5f, 1.0f - line_advance_scaled);
        string title = tprint("Something is %d", menu->something);
        text_add_string(text_info, render_info, title, at, 0.1f, FONT_FANCY, true);
        at.y -= line_advance_scaled;
        text_add_string(text_info, render_info, STRING("Press Escape when you are done."), at, 0.1f, FONT_FANCY, true);
    }
    
    auto              page = menu->pages + menu->current_page;
    const auto interaction = menu->interaction;
    const auto       hover = menu->hover;
    
    const v4   idle_color = V4(1.0f, 0.0f, 0.0f, 1.0f);
    const v4  hover_color = V4(1.0f, 1.0f, 0.0f, 1.0f);
    const v4 active_color = V4(0.0f, 1.0f, 0.0f, 1.0f);
    
    { // Buttons.
        s32 button_count = page->button_count;
        for(s32 i = 0; i < button_count; ++i) {
            rect2 aabb = page->button_aabbs[i];
            v4 color = idle_color;
            
            if((interaction.type == WIDGET_TYPE_BUTTON) && (interaction.index == i)) {
                color = active_color;
            } else if((hover.type == WIDGET_TYPE_BUTTON) && (hover.index == i)) {
                color = hover_color;
            }
            
            queue_quad_minmax(render_info, render_info->blank_texture_id, aabb.min, aabb.max, color);
        }
    }
    
    { // Toggles.
        s32 toggle_count = page->toggle_count;
        for(s32 i = 0; i < toggle_count; ++i) {
            rect2 aabb = page->toggle_aabbs[i];
            auto toggle = page->toggles[i];
            v4 color = idle_color;
            
            if(*toggle.actual) {
                color = active_color;
            }
            
            if((hover.type == WIDGET_TYPE_TOGGLE) && (hover.index == i)) {
                color = color * 0.5f + hover_color * 0.5f;
            }
            
            queue_quad_minmax(render_info, render_info->blank_texture_id, aabb.min, aabb.max, color);
        }
    }
    
    { // Dropdowns.
        s32 dropdown_count = page->dropdown_count;
        for(s32 i = 0; i < dropdown_count; ++i) {
            rect2 aabb = page->dropdown_aabbs[i];
            string label = page->dropdowns[i].options[page->dropdowns[i].active].label;
            v4 color = idle_color;
            
            if((interaction.type == WIDGET_TYPE_DROPDOWN) && (interaction.index == i)) {
                color = active_color;
            } else if((hover.type == WIDGET_TYPE_DROPDOWN) && (hover.index == i)) {
                color = hover_color;
            }
            
            queue_quad_minmax(render_info, render_info->blank_texture_id, aabb.min, aabb.max, color);
            
            // Only draw this label if the dropdown isn't expanded.
            if((interaction.type != WIDGET_TYPE_DROPDOWN) || (interaction.index != i)) {
                // @Hardcoded :DropdownLabels
                text_add_string(text_info, render_info, label, V2(aabb.left + 0.02f / GAME_ASPECT_RATIO, aabb.bottom + 0.035f), 0.07f, FONT_REGULAR);
            }
        }
    }
    
    { // Sliders.
        s32 slider_count = s_min(menu->sliders_to_display, page->slider_count);
        for(s32 i = 0; i < slider_count; ++i) {
            rect2 aabb = page->slider_aabbs[i];
            auto slider = page->sliders + i;
            v4 empty_color = idle_color;
            
            if((hover.type == WIDGET_TYPE_SLIDER) && (hover.index == i)) {
                empty_color = hover_color;
            }
            
            f32 watermark = (*slider->actual - slider->min) / (slider->max - slider->min);
            f32 split = f_lerp(aabb.left, aabb.right, watermark);
            v2 left_max  = V2(split, aabb.   top);
            v2 right_min = V2(split, aabb.bottom);
            
            queue_quad_minmax(render_info, render_info->blank_texture_id,  aabb.min, left_max, V4(0.0f, 1.0f, 0.0f, 1.0f));
            queue_quad_minmax(render_info, render_info->blank_texture_id, right_min, aabb.max, empty_color);
        }
    }
    
    { // Keybinds.
        for(s32 i = 0; i < page->keybind_count; ++i) {
            rect2 aabb = page->keybind_aabbs[i];
            v4 color = idle_color;
            
            if((interaction.type == WIDGET_TYPE_KEYBIND) && (interaction.index == i)) {
                color = active_color;
            } else if((hover.type == WIDGET_TYPE_KEYBIND) && (hover.index == i)) {
                color = hover_color;
            }
            
            queue_quad_minmax(render_info, render_info->blank_texture_id, aabb.min, aabb.max, color);
        }
    }
    
    { // Labels.
        FORI(0, page->label_count) {
            Menu_Label *label = page->labels + i;
            text_add_string(text_info, render_info, label->s, label->p, label->pixel_height, label->font, label->centered);
        }
    }
    
    { // Dropdown options.
        if(interaction.type == WIDGET_TYPE_DROPDOWN) {
            rect2 aabb    = page->dropdown_aabbs[interaction.index];
            auto dropdown = page->dropdowns     [interaction.index];
            
            for(s32 i = 0; i < dropdown.option_count; ++i) {
                auto *option = dropdown.options + i;
                const rect2 option_aabb = rect2_4f(aabb. left, aabb.bottom - 0.1f * i,
                                                   aabb.right, aabb.   top - 0.1f * i);
                v4 color = idle_color;
                if(i == dropdown.active) {
                    color = active_color;
                } else if(i == dropdown.hover) {
                    color = hover_color;
                }
                queue_quad_minmax(render_info, render_info->blank_texture_id, option_aabb.min, option_aabb.max, color);
                
                // :DropdownLabels @Hardcoded! Need colors at least.
                text_add_string(text_info, render_info, option->label, V2(option_aabb.left + 0.02f / GAME_ASPECT_RATIO, option_aabb.bottom + 0.035f), 0.07f, FONT_REGULAR, false);
            }
        }
    }
}
