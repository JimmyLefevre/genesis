
inline u16 big_to_little_endian_u16(u16 *ptr) {
    u16 result = (((u8 *)ptr)[0] << 8) + ((u8 *)ptr)[1];
    return result;
}
inline s16 big_to_little_endian_s16(s16 *ptr) {
    s16 result = (s16)big_to_little_endian_u16((u16 *)ptr);
    return result;
}
inline u32 big_to_little_endian_u32(u32 *ptr) {
    u32 result = (((u8 *)ptr)[0] << 24) + (((u8 *)ptr)[1] << 16) + (((u8 *)ptr)[2] << 8) + (((u8 *)ptr)[3]);
    return result;
}

static void accumulate_line(f32 *accumulation, s32 acc_width, s32 acc_height, v2 p0, v2 p1) {
    if(p1.y == p0.y) {
        // Contours are infinitely thin, so there's no area to compute.
        return;
    }
    
    // We go CCW, so upward is a contour start and downward is a contour end.
    f32 area_factor = -1.0f;
    if(p1.y > p0.y) {
        v2 swap = p1;
        p1 = p0;
        p0 = swap;
        area_factor = -area_factor;
    }
    f32 dxdy = (p1.x - p0.x) / (p1.y - p0.y);
    
    v2 at = p0;
    v2 to = p1;
    for(s32 y = s_min(acc_height - 2, (s32)at.y); y >= (s32)to.y; --y) {
        f32 scan_start_y = at.y;
        f32 scan_end_y = f_max((f32)y, to.y);
        f32 scan_dy = scan_end_y - scan_start_y;
        f32 scan_start_x = at.x;
        f32 scan_end_x = scan_start_x + dxdy * scan_dy;
        bool swapped_horizontal = false;
        if(scan_start_x > scan_end_x) {
            f32 swap = scan_start_x;
            scan_start_x = scan_end_x;
            scan_end_x = swap;
            swapped_horizontal = true;
        }
        
        {
            s32 first_pixel_x = (s32)scan_start_x;
            s32 last_pixel_x = (s32)scan_end_x;
            
            f32 at_x = scan_start_x;
            f32 at_y = scan_start_y;
            for(s32 x = first_pixel_x; x <= last_pixel_x; ++x) {
                f32 pixel_left_boundary = (f32)x;
                f32 pixel_right_boundary = (f32)(x + 1);
                f32 pixel_start_x = f_max(at_x, pixel_left_boundary);
                f32 pixel_end_x = f_min(scan_end_x, pixel_right_boundary);
                
                f32 scan_contribution = (scan_end_x == scan_start_x) ? 1.0f : ((pixel_end_x - pixel_start_x) / (scan_end_x - scan_start_x));
                f32 height = f_abs(scan_dy * scan_contribution);
                f32 trap_width = pixel_right_boundary - (at_x + pixel_end_x) * 0.5f;
                
                f32 area = trap_width * height;
                accumulation[y * acc_width + x] += area * area_factor;
                accumulation[y * acc_width + x + 1] += (height - area) * area_factor;
                
                ASSERT((accumulation[y * acc_width + x] < 1.01f) && (accumulation[y * acc_width + x + 1] < 1.01f));
                
                at_x = pixel_end_x;
                at_y -= height;
            }
        }
        
        at.x = (swapped_horizontal) ? scan_start_x : scan_end_x;
        at.y = scan_end_y;
    }
}

static void accumulate_quadratic_bezier(f32 *acc, s32 width, s32 height, v2 a, v2 b, v2 c) {
    v2 curve_midpoint = (a + b * 2.0f + c) * 0.25f;
    v2 approximation = (a + c) * 0.5f;
    
    // @Speed: Adjust this value to get the best quality for time ratio.
    if(v2_length_sq(curve_midpoint - approximation) <= 0.15f) {
        accumulate_line(acc, width, height, a, c);
    } else {
        accumulate_quadratic_bezier(acc, width, height, a, (a + b) / 2.0f, curve_midpoint);
        accumulate_quadratic_bezier(acc, width, height, curve_midpoint, (b + c) / 2.0f, c);
    }
}

static void init_font(Text_Info *info, Datapack_Handle *datapack, u32 font_index, u32 asset_uid) {
    ASSERT(font_index < FONT_COUNT);
    Asset_Metadata metadata = get_asset_metadata(datapack, asset_uid);
    
    u8 * const file_data = push_array(&info->font_block, u8, metadata.size);
    os_platform.read_file(datapack->file_handle, file_data, metadata.location.offset, metadata.size);
    
    TTF_Offset_Subtable *subtable = (TTF_Offset_Subtable *)file_data;
    TTF_Table_Directory *first_dir = (TTF_Table_Directory *)(subtable + 1);
    f32 inv_height = 0.0f;
    
    bool found_a_cmap_we_can_read = false;
    
    u16 table_count = big_to_little_endian_u16(&subtable->table_count);
    Font_Parse_Data font = {};
    for(u32 itable = 0; itable < table_count; ++itable) {
        TTF_Table_Directory *it = first_dir + itable;
        u8 *table_location = file_data + big_to_little_endian_u32(&it->offset_from_start_of_file);
        switch(it->tag) {
            case FOUR_BYTES_TO_U32('g', 'l', 'y', 'f'): {
                font.glyf = table_location;
            } break;
            
            case FOUR_BYTES_TO_U32('h', 'e', 'a', 'd'): {
                TTF_head_Header *head = (TTF_head_Header *)table_location;
                
                font.loca_format = big_to_little_endian_s16(&head->loca_format);
            } break;
            
            case FOUR_BYTES_TO_U32('h', 'h', 'e', 'a'): {
                TTF_hhea_Header *hhea = (TTF_hhea_Header *)table_location;
                
                font.ascent = big_to_little_endian_s16(&hhea->ascent);
                font.descent = big_to_little_endian_s16(&hhea->descent);
                font.line_gap = big_to_little_endian_s16(&hhea->line_gap);
                font.horizontal_metric_count = big_to_little_endian_u16(&hhea->horizontal_metric_count);
                inv_height = 1.0f / (f32)(font.ascent - font.descent);
            } break;
            
            case FOUR_BYTES_TO_U32('k', 'e', 'r', 'n'): {
                TTF_kern_Header *header = (TTF_kern_Header *)table_location;
                ASSERT((header->version == 0) && (big_to_little_endian_u16(&header->subtable_count) == 1));
                
                TTF_kern_Subtable_Header *subheader = (TTF_kern_Subtable_Header *)(header + 1);
                font.kern = (TTF_kern_Subtable *)(subheader + 1);
            } break;
            
            case FOUR_BYTES_TO_U32('h', 'm', 't', 'x'): {
                font.horizontal_metrics = (Horizontal_Metric *)table_location;
            } break;
            
            case FOUR_BYTES_TO_U32('l', 'o', 'c', 'a'): {
                font.loca = file_data + big_to_little_endian_u32(&it->offset_from_start_of_file);
            } break;
            
            // @Incomplete:
            // Both Microsoft and Apple recommend using Unicode cmap tables.
            // We should probably use them.
            case FOUR_BYTES_TO_U32('c', 'm', 'a', 'p'): {
                TTF_cmap_Header *cmap = (TTF_cmap_Header *)table_location;
                TTF_cmap_Encoding_Table *first_encoding_table = (TTF_cmap_Encoding_Table *)(cmap + 1);
                
                for(u32 iencoding = 0; iencoding < big_to_little_endian_u16(&cmap->table_count); ++iencoding) {
                    TTF_cmap_Encoding_Table *table = first_encoding_table + iencoding;
                    u16 platform = big_to_little_endian_u16(&table->platform);
                    
                    if(platform == TTF_CMAP_PLATFORM_WINDOWS) {
                        u16 encoding = big_to_little_endian_u16(&table->encoding);
                        
                        if((encoding == TTF_CMAP_ENCODING_WINDOWS_BMP) || (encoding == TTF_CMAP_ENCODING_WINDOWS_UTF32)) {
                            TTF_cmap_Format_12_13_Subtable_Header *cmap_subtable = (TTF_cmap_Format_12_13_Subtable_Header *)(((u8 *)cmap) + big_to_little_endian_u32(&table->byte_offset_to_subtable));
                            u16 format = big_to_little_endian_u16(&cmap_subtable->format);
                            
                            if(format == 4) {
                                font.cmap = (TTF_cmap_Format_4_Subtable_Header *)cmap_subtable;
                                found_a_cmap_we_can_read = true;
                            } else {
                                ASSERT((format == 12) || (format == 13));
                                // @Incomplete: This should be much easier than format 4.
                            }
                        }
                    } else if(platform == TTF_CMAP_PLATFORM_MAC) {
                        // @Todo: Check how to get the right info from this
                    }
                }
            } break;
        }
    }
    
    ASSERT(found_a_cmap_we_can_read);
    
    info->fonts.glyf_tables[font_index] = font.glyf;
    info->fonts.loca_tables[font_index] = font.loca;
    info->fonts.cmap_tables[font_index] = font.cmap;
    info->fonts.kern_tables[font_index] = font.kern;
    info->fonts.horizontal_metrics[font_index] = font.horizontal_metrics;
    info->fonts.horizontal_metric_counts[font_index] = font.horizontal_metric_count;
    info->fonts.loca_formats[font_index] = font.loca_format;
    info->fonts.ascents[font_index] = font.ascent;
    info->fonts.descents[font_index] = font.descent;
    info->fonts.line_gaps[font_index] = font.line_gap;
    info->fonts.inv_heights[font_index] = inv_height;
}

static u16 get_glyph_index(TTF_cmap_Format_4_Subtable_Header *subtable4, s32 codepoint) {
    u16 segment_count = big_to_little_endian_u16(&subtable4->segment_count_times_two) / 2;
    
    u16 * const end_codepoints = (u16 *)(subtable4 + 1);
    u16 * const start_codepoints = end_codepoints + segment_count + 1;
    s16 * const index_deltas = (s16 *)(start_codepoints + segment_count);
    u16 * const index_offsets = (u16 *)(index_deltas + segment_count);
    // @Speed: Binary search instead of linear lookup if the codepoint is big.
    for(u32 j = 0; j < segment_count; ++j) {
        const u16 end_codepoint = big_to_little_endian_u16(end_codepoints + j);
        if(end_codepoint >= codepoint) {
            const u16 start_codepoint = big_to_little_endian_u16(start_codepoints + j);
            const s16 index_delta = big_to_little_endian_s16(index_deltas + j);
            const u16 index_offset = big_to_little_endian_u16(index_offsets + j);
            
            if(!index_offset) {
                return (u16)(codepoint + index_delta);
            } else {
                return big_to_little_endian_u16(index_offsets + j + (codepoint - start_codepoint) + (index_offset / 2));
            }
        }
    }
    return 0;
}

inline Horizontal_Metric get_horizontal_metrics(Horizontal_Metric *horizontal_metrics, u16 horizontal_metric_count, u16 glyph_index) {
    Horizontal_Metric result;
    if(glyph_index > horizontal_metric_count) {
        result.advance = big_to_little_endian_s16((s16 *)(horizontal_metrics + horizontal_metric_count - 1));
        result.left_side_bearing = big_to_little_endian_s16((s16 *)horizontal_metrics + horizontal_metric_count + glyph_index);
    } else {
        Horizontal_Metric metric = horizontal_metrics[glyph_index];
        result.advance = big_to_little_endian_u16(&metric.advance);
        result.left_side_bearing = big_to_little_endian_s16(&metric.left_side_bearing);
    }
    return result;
}

inline s16 get_kerning(TTF_kern_Subtable *kern, u16 left_glyph, u16 right_glyph) {
    const u16 count = kern->pair_count;
    Kerning_Pair *kerning_pairs = (Kerning_Pair *)(kern + 1);
    for(u32 i = 0; i < big_to_little_endian_u16(&kern->pair_count); ++i) {
        Kerning_Pair it = kerning_pairs[i];
        if((big_to_little_endian_u16(&it.left_glyph) == left_glyph) && (big_to_little_endian_u16(&it.right_glyph) == right_glyph)) {
            return big_to_little_endian_s16(&it.kerning);
        }
    }
    return 0;
}

static void rasterize_glyph(Glyph_Raster_Data *data, f32 scale,
                            f32 *acc, s32 width, s32 height, u32 *out) {
    u8 *flags = data->flags;
    s32 flags_length = data->flag_count;
    u8 *x_coords = data->x_coords;
    u8 *y_coords = data->y_coords;
    s16 x_min = data->x_min;
    s16 y_min = data->y_min;
    u16 *last_contour_points = data->last_contour_points;
    u8 current_flags = 0;
    s16 current_x = 0;
    s16 current_y = 0;
    u8 flag_repeat = 0;
    bool last_off_curve = false;
    v2 first_contour_p = V2();
    v2 last_p = V2();
    v2 off_curve_p = V2();
    u16 running_contour_point_i = 0;
    u16 start_of_next_contour_i = 0;
    u32 running_contour_index = 0;
    s32 running_x = 0;
    s32 running_y = 0;
    s32 running_flag = 0;
    while((running_flag < flags_length) || flag_repeat) {
        // Updating flags:
        if(!flag_repeat) {
            current_flags = flags[running_flag++];
            if(current_flags & TTF_SIMPLE_GLYPH_REPEAT) {
                flag_repeat = flags[running_flag++];
            }
        } else {
            --flag_repeat;
        }
        
        // Updating current position:
        if(current_flags & TTF_SIMPLE_GLYPH_X_SHORT_VECTOR) {
            if(current_flags & TTF_SIMPLE_GLYPH_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR) {
                current_x += x_coords[running_x];
            } else {
                current_x -= x_coords[running_x];
            }
            ++running_x;
        } else if(!(current_flags & TTF_SIMPLE_GLYPH_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR)) {
            current_x += big_to_little_endian_s16((s16 *)(x_coords + running_x));
            running_x += 2;
        }
        if(current_flags & TTF_SIMPLE_GLYPH_Y_SHORT_VECTOR) {
            if(current_flags & TTF_SIMPLE_GLYPH_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR) {
                current_y += y_coords[running_y];
            } else {
                current_y -= y_coords[running_y];
            }
            ++running_y;
        } else if(!(current_flags & TTF_SIMPLE_GLYPH_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR)) {
            current_y += big_to_little_endian_s16((s16 *)(y_coords + running_y));
            running_y += 2;
        }
        
        // Actually using our points to draw:
        v2 p = V2((f32)(current_x - x_min), (f32)(current_y - y_min)) * scale;
        // Sadly we're doing these two adds every iteration. A faster way would be to use "inv_pixel" above as a base.
        p.x += 1.0f;
        p.y += 1.0f;
        
        if(running_contour_point_i++ == start_of_next_contour_i) {
            // Close the contour, move to the start of the next.
            if(start_of_next_contour_i) {
                if(!last_off_curve) {
                    accumulate_line(acc, width, height, last_p, first_contour_p);
                } else {
                    accumulate_quadratic_bezier(acc, width, height, last_p, off_curve_p, first_contour_p);
                }
            }
            start_of_next_contour_i = big_to_little_endian_u16(last_contour_points + running_contour_index++) + 1;
            last_p = p;
            first_contour_p = p;
            last_off_curve = false;
            ASSERT(current_flags & TTF_SIMPLE_GLYPH_ON_CURVE_POINT); // Unhandled for now.
            
            continue;
        }
        
        ASSERT(p.y >= 0.0f && p.y <= (f32)height && p.x >= 0.0f && p.x <= (f32)width);
        if(current_flags & TTF_SIMPLE_GLYPH_ON_CURVE_POINT) {
            if(last_off_curve) {
                accumulate_quadratic_bezier(acc, width, height, last_p, off_curve_p, p);
            } else {
                accumulate_line(acc, width, height, last_p, p);
            }
            
            last_p = p;
            last_off_curve = false;
        } else {
            if(last_off_curve) {
                v2 interp_p = v2_lerp(off_curve_p, p, 0.5f);
                accumulate_quadratic_bezier(acc, width, height, last_p, off_curve_p, interp_p);
                
                last_p = interp_p;
            }
            
            off_curve_p = p;
            last_off_curve = true;
        }
    }
    
    // Closing the shape at the end.
    if(!last_off_curve) {
        accumulate_line(acc, width, height, last_p, first_contour_p);
    } else {
        accumulate_quadratic_bezier(acc, width, height, last_p, off_curve_p, first_contour_p);
    }
    
    // @Cleanup: Put this outside of the if/else statements once/if we support multiple formats.
    f32 v = 0.0f;
    f32 *accumulator = acc;
    for(s32 y = 0; y < height; ++y) {
        const s32 y_offset = y * width;
        for(s32 x = 0; x < width; ++x) {
            s32 ipixel = y_offset + x;
            v += accumulator[ipixel];
            u32 value8 = (u32)((u8)(f_min(f_abs(v), 1.0f) * 255.0f));
            // out[ipixel] = (value8 << 24) | 0x00FFFFFF;
            // Premultiplied:
            out[ipixel] = (value8 << 24) | (value8 << 16) | (value8 << 8) | value8;
        }
    }
}

static void find_glyph_raster_data(u8 *loca, s16 loca_format, u8 *glyf, u16 glyph_index, Glyph_Raster_Data *out_data) {
    // Finding the glyph's offset in the glyph table:
    u32 glyph_offset = 0;
    bool no_outline = false;
    if(loca_format == 0) {
        u16 *table16 = (u16 *)loca;
        glyph_offset = big_to_little_endian_u16(table16 + glyph_index) * 2;
        if(table16[glyph_index] == table16[glyph_index + 1]) {
            no_outline = true;
        }
    } else {
        ASSERT(loca_format == 1);
        u32 *table32 = (u32 *)loca;
        glyph_offset = big_to_little_endian_u32(table32 + glyph_index);
        if(table32[glyph_index] == table32[glyph_index + 1]) {
            no_outline = true;
        }
    }
    
    // Actually finding the glyph in the table:
    TTF_Glyph_Header *glyph = (TTF_Glyph_Header *)(glyf + glyph_offset);
    s16 contour_count = big_to_little_endian_s16(&glyph->contour_count);
    if(contour_count >= 0) {
        // The glyph is simple.
        u16 *last_contour_points = (u16 *)(glyph + 1);
        u16 instruction_count = big_to_little_endian_u16(last_contour_points + contour_count);
        u8 *instructions = (u8 *)(last_contour_points + contour_count + 1);
        u8 *flags = instructions + instruction_count;
        u16 contour_point_count = (no_outline) ? (0) : (1 + big_to_little_endian_u16(last_contour_points + contour_count - 1));
        
        // We need to parse the flags first to locate the x/y arrays.
        u16 x_size_in_bytes = 0;
        u16 point_entry_count = 0;
        u8 *_flags = flags;
        while(point_entry_count < contour_point_count) {
            u8 f = *_flags++;
            u8 logical_flag_count = 1;
            
            if(f & TTF_SIMPLE_GLYPH_REPEAT) {
                logical_flag_count += *_flags++;
            }
            
            if(f & TTF_SIMPLE_GLYPH_X_SHORT_VECTOR) {
                x_size_in_bytes += logical_flag_count;
            } else if(!(f & TTF_SIMPLE_GLYPH_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR)) {
                x_size_in_bytes += 2 * logical_flag_count;
            }
            point_entry_count += logical_flag_count;
        }
        
        u8 *x_coords = _flags;
        u8 *y_coords = x_coords + x_size_in_bytes;
        
        s16 x_min = big_to_little_endian_s16(&glyph->x_min);
        s16 y_min = big_to_little_endian_s16(&glyph->y_min);
        s16 x_max = big_to_little_endian_s16(&glyph->x_max);
        s16 y_max = big_to_little_endian_s16(&glyph->y_max);
        
        Glyph_Raster_Data data;
        data.flags = flags;
        data.flag_count = (s32)(x_coords - flags);
        data.x_coords = x_coords;
        data.y_coords = y_coords;
        data.last_contour_points = last_contour_points;
        data.x_min = x_min;
        data.y_min = y_min;
        data.x_max = x_max;
        data.y_max = y_max;
        *out_data = data;
    }
}

inline void text_add_string_pixel_height(Text_Info *info, string str, v2 p, f32 height, s8 font, bool centered = false, v4 color = V4(1.0f)) {
    ASSERT(info->strings.count < TEXT_STRING_COUNT);
    ASSERT((info->text_length + str.length) < TEXT_BUFFER_LENGTH);
    ASSERT(str.length);
    const s32 add = info->strings.count++;
    s32 text_length = info->text_length;
    s16 string_length = (s16)str.length;
    // Strip out zero terminators.
    if(str.data[string_length - 1] == '\0') {
        --string_length;
    }
    ASSERT(string_length);
    
    mem_copy(str.data, info->text_buffer + text_length, string_length);
    info->text_length = (s16)(text_length + string_length);
    info->strings.ps      [add] = p;
    info->strings.heights [add] = height;
    info->strings.lengths [add] = string_length;
    info->strings.fonts   [add] = font;
    info->strings.centered[add] = centered;
    info->strings.colors  [add] = color;
}

inline f32 to_text_height(f32 height, Rendering_Info *render_info) {
    f32 result = f_round(height * render_info->render_target_dim.y);
    return result;
}

inline void text_add_string(Text_Info *info, Rendering_Info *render_info, string str, v2 p, f32 height, s8 font, bool centered = false, v4 color = V4(1.0f)) {
    height = f_round(height * render_info->render_target_dim.y);
    text_add_string_pixel_height(info, str, p, height, font, centered, color);
}

inline f32 text_font_line_advance(Text_Info *info, s8 font) {
    f32 result = (f32)(info->fonts.ascents[font] - info->fonts.descents[font] +
                       info->fonts.line_gaps[font]) * info->fonts.inv_heights[font];
    return result;
}

inline f32 line_height_for_font(Text_Info *info, Rendering_Info *render_info, s8 font, f32 height) {
    f32 result = text_font_line_advance(info, font) * f_round(height * render_info->render_target_dim.y) / render_info->render_target_dim.y;
    return result;
}

inline f32 character_width_for_mono_font(Text_Info *info, Rendering_Info *render_info, s8 font, f32 height) {
    auto hm = get_horizontal_metrics(info->fonts.horizontal_metrics[font],
                                     info->fonts.horizontal_metric_counts[font],
                                     get_glyph_index(info->fonts.cmap_tables[font], 'M'));
    
    // @Duplication: We are essentially emulating the scaling from text_add_string and text_update.
    height = f_round(height * render_info->render_target_dim.y);
    f32 scale = (1.0f / render_info->render_target_dim.y) / render_info->aspect_ratio;
    
    f32 result = (f32)hm.advance * height * info->fonts.inv_heights[font] * scale;
    
    return result;
}

void Implicit_Context::flush_rasters_to_atlas(Text_Info *info, u8 *hash_indices,
                                              u32 **rasters, v2s16 *raster_dims, v2 *raster_offsets, const s16 raster_count,
                                              s16 *line_lefts, s16 *line_ys, s16 *line_count,
                                              const v2s16 atlas_dim, const f32 inv_atlas_width, const f32 inv_atlas_height) {
    v2s16 *pack_ps = push_array(&temporary_memory, v2s16, raster_count);
    
    if(rect_pack(raster_dims, raster_count, line_lefts, line_ys, line_count, atlas_dim.x, atlas_dim.y, pack_ps)) {
        // Everything was packed properly.
        for(s32 i = 0; i < raster_count; ++i) {
            const v2s16 dim = raster_dims[i];
            const v2s16 p = pack_ps[i];
            u32 *texture_data = rasters[i];
            const u8 hash_index = hash_indices[i];
            const v2 offset = raster_offsets[i];
            const v2s16 rect_min = V2S16(p.x + 1, p.y + 1);
            const v2s16 rect_max = V2S16(p.x + dim.x - 1, p.y + dim.y - 1);
            v2 uvmin, uvmax;
            
#if GENESIS_DEV
            // Make sure we have a one-pixel border around the glyph.
            for(s16 _i = 0; _i < dim.y; ++_i) {
                ASSERT(texture_data[_i * dim.x +         0] == 0);
                ASSERT(texture_data[_i * dim.x + dim.x - 1] == 0);
                if(_i < dim.x) {
                    ASSERT(texture_data[          0 * dim.x + _i] == 0);
                    ASSERT(texture_data[(dim.y - 1) * dim.x + _i] == 0);
                }
            }
#endif
            
            // uvmin is bottom-left, uvmax is top-right.
            const v2 rect_min_f = V2(rect_min);
            const v2 rect_max_f = V2(rect_max);
            const v2 halfdim = V2((f32)(rect_max.x - rect_min.x), (f32)(rect_max.y - rect_min.y)) * 0.5f;
            uvmin.x = rect_min_f.x * inv_atlas_width;
            uvmin.y = rect_min_f.y * inv_atlas_height;
            uvmax.x = rect_max_f.x * inv_atlas_width;
            uvmax.y = rect_max_f.y * inv_atlas_height;
            
            update_texture_rgba(info->glyph_atlas, p.x, p.y, dim.x, dim.y, texture_data);
            Hashed_Glyph_Additional_Data *data = info->glyph_hash_auxiliary + hash_index;
            data->uvs[0] = uvmin;
            data->uvs[1] = uvmax;
            data->offset = offset;
            data->halfdim = halfdim;
        }
    } else {
        // @Incomplete: If the glyph cache is full, we invalidate it entirely and give up on drawing anything
        // for the frame.
        // A way to sidestep no-draw frames is to @Refactor the system to draw glyphs that are currently present
        // in the atlas instead of going linearly through the glyph array.
        
        // Throw the cache away.
        rect_pack_init(line_lefts, line_ys, line_count, GLYPH_ATLAS_SIZE);
        empty(&info->glyph_hash);
        // Also, give up on drawing anything.
        info->raster_requests.count = 0;
        info->glyphs.count = 0;
    }
}

static inline u8 hash_glyph(Hashed_Glyph_Data *data) {
    // FFIIIHHH where F is font, I is glyph_index and H is pixel_height.
    u8 result = (data->font << 5) | ((data->glyph_index & 0x7) << 3) | ((data->pixel_height >> 1) & 0x7);
    if(result == 0) {
        result = 1;
    }
    return result;
}

// NOTE(keeba): This function will flush every added string thus far!
void Implicit_Context::draw_string_immediate(Text_Info *info, Rendering_Info *render_info, string s, v2 p, f32 height, s8 font, bool centered = false, v4 color = V4(1.0f)) {
    TIME_BLOCK;
    text_add_string(info, render_info, s, p, height, font, centered, color);
    text_update(info, render_info);
}

void Implicit_Context::text_update(Text_Info *info, Rendering_Info *render_info) {
    
    TIME_BLOCK;
    const f32 master_scale_y = 1.0f / render_info->render_target_dim.y;
    const f32 master_scale_x = master_scale_y / render_info->aspect_ratio;
    const v2 one_pixel = render_info->one_pixel;
    
    { // Decoding input strings to glyph indices.
        u8 *text = info->text_buffer;
        const s32 string_count = info->strings.count;
        s16 running_string_offset = 0;
        s16 glyph_count = 0;
        
        for(s32 istr = 0; istr < string_count; ++istr) {
            const s16 length = info->strings.lengths[istr];
            const s8 font = info->strings.fonts[istr];
            const v4 color = info->strings.colors[istr];
            TTF_cmap_Format_4_Subtable_Header *cmap = info->fonts.cmap_tables[font];
            s16 i = 0;
            
            do {
                const s16 add = glyph_count;
                const s16 itxt = running_string_offset + i;
                ASSERT(text[itxt] >= 0x20);
                
                Decoding_Result decode = utf8_to_codepoint(text + itxt);
                s32 codepoint = decode.codepoint;
                s16 glyph_index = get_glyph_index(cmap, codepoint);
                
                info->glyphs.indices[add] = glyph_index;
                info->glyphs.colors [add] = color;
                i += decode.characters_decoded;
                ++glyph_count;
            } while(i < length);
            
            info->strings.glyph_counts[istr] = glyph_count;
            running_string_offset += length;
        }
        
        info->glyphs.count = glyph_count;
    }
    
    { // Building list of glyphs to rasterize; computing advances + kerning.
        const s32 string_count = info->strings.count;
        s16 running_string_offset = 0;
        s16 raster_request_count = info->raster_requests.count;
        
        for(s32 istr = 0; istr < string_count; ++istr) {
            const s8 font = info->strings.fonts[istr];
            const s16 length = info->strings.lengths[istr];
            TTF_kern_Subtable * const kern = info->fonts.kern_tables[font];
            Horizontal_Metric * const horz = info->fonts.horizontal_metrics[font];
            const u16 horz_count = info->fonts.horizontal_metric_counts[font];
            const f32 height = info->strings.heights[istr];
            // @Temporary Fix this when we change scales on the calling side.
            const s16 pixel_height = (s16)height;
            s16 iglyph = running_string_offset;
            const s16 ilastglyph = iglyph + length;
            s16 glyph_index = info->glyphs.indices[iglyph];
            s32 total_advance = 0;
            
            do {
                const s16 next_glyph_index = info->glyphs.indices[iglyph + 1];
                s16 advance = 0;
                u8 index;
                
                // We need to clear these to zero, because otherwise the padding byte
                // will mess up the mem_compare.
                Hashed_Glyph_Data data = {};
                data.pixel_height = pixel_height;
                data.glyph_index  = glyph_index;
                data.font         = font;
                
                Hashed_Glyph_Data *found = find(&info->glyph_hash, &data);
                if(found) {
                    index = (u8)(found - info->glyph_hash.items);
                    ASSERT(index == (found - info->glyph_hash.items));
                    advance = info->glyph_hash_auxiliary[index].advance;
                } else {
                    found = add(&info->glyph_hash, &data);
                    ASSERT(found);
                    
                    index = (u8)(found - info->glyph_hash.items);
                    Horizontal_Metric hm = get_horizontal_metrics(horz, horz_count, glyph_index);
                    
                    info->glyph_hash_auxiliary[index] = {};
                    info->glyph_hash_auxiliary[index].advance = hm.advance;
                    
                    advance = hm.advance;
                    
                    s16 add = raster_request_count++;
                    ASSERT(add < MAX_RASTERS_PER_UPDATE);
                    
                    info->raster_requests.fonts             [add] = font;
                    info->raster_requests.left_side_bearings[add] = hm.left_side_bearing;
                    info->raster_requests.glyph_indices     [add] = glyph_index;
                    info->raster_requests.pixel_heights     [add] = pixel_height;
                    info->raster_requests.hash_indices      [add] = index;
                    
                }
                
                if(kern && (iglyph < (ilastglyph - 1))) {
                    s16 kerning = get_kerning(kern, glyph_index, next_glyph_index);
                    advance += kerning;
                }
                
                info->glyphs.advances[iglyph] = advance;
                info->glyphs.hash_indices[iglyph] = index;
                glyph_index = next_glyph_index;
                total_advance += advance;
                ++iglyph;
            } while(iglyph < ilastglyph);
            
            info->strings.total_advances[istr] = total_advance;
            running_string_offset += length;
        }
        
        info->raster_requests.count = raster_request_count;
    }
    
    { // Scaling advances + kerning; positioning strings.
        const s32 string_count = info->strings.count;
        s16 running_string_offset = 0;
        
        for(s32 istr = 0; istr < string_count; ++istr) {
            const s8 font = info->strings.fonts[istr];
            const s16 length        = info->strings.lengths[istr];
            const f32 height        = info->strings.heights[istr];
            const s32 total_advance = info->strings.total_advances[istr];
            
            f32 draw_scale_x = height * info->fonts.inv_heights[font] * master_scale_x;
            
            v2 p = info->strings.ps[istr];
            bool center = info->strings.centered[istr];
            s32 running_advance = 0;
            v2 draw_p;
            draw_p.y = p.y;
            
            const f32 total_width = (f32)total_advance * draw_scale_x;
            if(center) {
                p.x -= total_width * 0.5f;
            }
            
            for(s16 i = 0; i < length; ++i) {
                const s16 iglyph = running_string_offset + i;
                const s16 advance = info->glyphs.advances[iglyph];
                
                draw_p.x = p.x + (f32)running_advance * draw_scale_x;
                running_advance += advance;
                
                info->glyphs.ps[iglyph] = draw_p;
            }
            
            running_string_offset += length;
        }
    }
    
    { // Rasterizing missing glyphs.
        SCOPE_MEMORY(&temporary_memory); //Scoped_Memory _ = Scoped_Memory(&temporary_memory);
        const s16 request_count = info->raster_requests.count;
        const v2s16 atlas_dim = V2S16(GLYPH_ATLAS_SIZE);
        const f32 inv_atlas_width  = 1.0f / (f32)atlas_dim.x;
        const f32 inv_atlas_height = 1.0f / (f32)atlas_dim.y;
        v2s16 *pack_dims   = push_array(&temporary_memory, v2s16, request_count);
        v2    *offsets     = push_array(&temporary_memory, v2,    request_count);
        u32   **raster_out = push_array(&temporary_memory, u32 *, request_count);
        s16 *line_lefts = info->glyph_atlas_lines.lefts;
        s16 *line_ys    = info->glyph_atlas_lines.ys;
        s16  line_count = info->glyph_atlas_lines.count;
        
        usize rasterize_watermark = temporary_memory.used;
        
        s16 total_pack_count = 0;
        s16 pack_count = 0;
        
        for(s16 i = 0; i < request_count; ++i) {
            const s8 font               = info->raster_requests.fonts[i];
            const s16 glyph_index       = info->raster_requests.glyph_indices[i];
            const s16 left_side_bearing = info->raster_requests.left_side_bearings[i];
            const s16 pixel_height      = info->raster_requests.pixel_heights[i];
            const u8 hash_index         = info->raster_requests.hash_indices[i];
            u8 *loca            = info->fonts.loca_tables[font];
            s16 loca_format     = info->fonts.loca_formats[font];
            u8 *glyf            = info->fonts.glyf_tables[font];
            f32 inv_font_height = info->fonts.inv_heights[font];
            Glyph_Raster_Data raster_data;
            
            const f32 raster_scale = (f32)pixel_height * inv_font_height;
            find_glyph_raster_data(loca, loca_format, glyf, glyph_index, &raster_data);
            
            const f32 ttf_width_f  = (f32)(raster_data.x_max - raster_data.x_min);
            const f32 ttf_height_f = (f32)(raster_data.y_max - raster_data.y_min);
            const s16 width  = (s16)f_ceil_s(ttf_width_f  * raster_scale) + 2;
            const s16 height = (s16)f_ceil_s(ttf_height_f * raster_scale) + 2;
            // ASSERT(height <= pixel_height); // NOTE: This makes sure we respect the requested raster height.
            const s16 stride = width;
            const s32 pixel_count = stride * height;
            
            if((temporary_memory.size - temporary_memory.used) < ((sizeof(f32) + sizeof(u32)) * pixel_count)) {
                flush_rasters_to_atlas(info, info->raster_requests.hash_indices + total_pack_count,
                                       raster_out, pack_dims, offsets, pack_count,
                                       line_lefts, line_ys, &line_count,
                                       atlas_dim, inv_atlas_width, inv_atlas_height);
                total_pack_count += pack_count;
                pack_count = 0;
                temporary_memory.used = rasterize_watermark;
            }
            f32 *acc = push_array(&temporary_memory, f32, pixel_count);
            u32 *out = push_array(&temporary_memory, u32, pixel_count);
            zero_mem(acc, pixel_count * sizeof(f32));
            rasterize_glyph(&raster_data, raster_scale, acc, stride, height, out);
            
            // This offset is a halfdim factor.
            // 1.0f aligns the bottom/left of the texture to p,
            // while -1.0f aligns the top/right of the texture.
            v2 offset;
            offset.x = 1.0f + 2.0f * ((f32)left_side_bearing / ttf_width_f );
            offset.y = 1.0f + 2.0f * ((f32)raster_data.y_min / (f32)(raster_data.y_max - raster_data.y_min));
            
            pack_dims[pack_count] = V2S16(width, height);
            offsets[pack_count] = offset;
            raster_out[pack_count] = out;
            
            ++pack_count;
        }
        
        flush_rasters_to_atlas(info, info->raster_requests.hash_indices + total_pack_count,
                               raster_out, pack_dims, offsets, pack_count,
                               line_lefts, line_ys, &line_count,
                               atlas_dim, inv_atlas_width, inv_atlas_height);
        
        info->glyph_atlas_lines.count = line_count;
    }
    
    { // Queueing glyph draw commands.
        const s16 count = info->glyphs.count;
        const u32 glyph_atlas = info->glyph_atlas;
        
        for(s16 i = 0; i < count; ++i) {
            const u8 hash_index = info->glyphs.hash_indices[i];
            const Hashed_Glyph_Additional_Data aux = info->glyph_hash_auxiliary[hash_index];
            const v4 color = info->glyphs.colors[i];
            v2 p = info->glyphs.ps[i];
            v2 halfdim = aux.halfdim;
            v2 offset = aux.offset;
            
            halfdim.x *= master_scale_x;
            halfdim.y *= master_scale_y;
            offset.x *= halfdim.x;
            offset.y *= halfdim.y;
            
            v2 bottom_left = p - halfdim + offset;
            v2 dim = halfdim * 2.0f;
            
            // We snap the glyphs to pixels.
            bottom_left.x = (f32)(s32)((bottom_left.x / one_pixel.x) + 0.5f) * one_pixel.x;
            bottom_left.y = (f32)(s32)((bottom_left.y / one_pixel.y) + 0.5f) * one_pixel.y;
            dim.x         = (f32)(s32)((        dim.x / one_pixel.x) + 0.5f) * one_pixel.x;
            dim.y         = (f32)(s32)((        dim.y / one_pixel.y) + 0.5f) * one_pixel.y;
            
            ASSERT(f_abs((dim.x / one_pixel.x) - (aux.halfdim.x * 2.0f)) < 0.0001f);
            ASSERT(f_abs((dim.y / one_pixel.y) - (aux.halfdim.y * 2.0f)) < 0.0001f);
            
            v2 top_right = bottom_left + dim;
            
            queue_quad_minmax(render_info, glyph_atlas, aux.uvs[0], aux.uvs[1], bottom_left, top_right, V2(), color);
        }
    }
    // DEBUG Glyph atlas visualisation:
    // queue_quad_minmax(render_info, info->glyph_atlas, V2(), V2(1.0f), V2(), V2(one_pixel.x * 1024.0f, one_pixel.y * 1024.0f), V2(), V4(1.0f));
    
    
    info->strings.count = 0;
    info->text_length = 0;
    info->glyphs.count = 0;
    info->raster_requests.count = 0;
}