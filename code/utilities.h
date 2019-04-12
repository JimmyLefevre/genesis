// @Cleanup: Use this.
struct Rect_Pack_Data {
    s16 *line_lefts;
    s16 *line_ys;
    s16 line_count;
    s16 target_width;
    s16 target_height;
};
inline void rect_pack_init(s16 *line_lefts, s16 *line_ys, s16 *line_count, s16 width) {
    line_lefts[0] = 0;
    line_ys[0] = 0;
    line_lefts[1] = width;
    *line_count = 1;
}

// Returns false on failure. In that case, we'll have packed as much in order as we could have. 
static bool rect_pack(v2s16 *in_dims, s16 rect_count, s16 *line_lefts, s16 *line_ys,
                      s16 *line_count_inout, s16 target_width, s16 target_height, v2s16 *out_ps) {
    bool result = true;
    s16 line_count = *line_count_inout;
    
    for(s16 irect = 0; irect < rect_count; ++irect) {
        const s16 width = in_dims[irect].x;
        const s16 height = in_dims[irect].y;
        s16 best_x = 0;
        s16 best_y = target_height;
        u16 ibest = 0;
        u16 ibestscan = 0;
        
        {
            s16 iline = 0;
            s16 line_left = line_lefts[0];
            s16 rect_right = line_left + width;
            if(rect_right <= target_width) {
                do {
                    s16 max_y = line_ys[iline];
                    
                    if(max_y < best_y) {
                        s16 iscan = iline + 1;
                        s16 scan_left = line_lefts[iscan];
                        
                        if(scan_left < rect_right) {
                            do {
                                s16 scan_y = line_ys[iscan];
                                u16 scan_gt_max = TEST_GT(scan_y, max_y, 16);
                                max_y = SELECT(scan_gt_max, scan_y, max_y);
                                
                                scan_left = line_lefts[iscan + 1];
                                ++iscan;
                            } while(scan_left < rect_right);
                        }
                        
                        if(max_y < best_y) {
                            best_x = line_left;
                            best_y = max_y;
                            ibest = iline;
                            ibestscan = iscan;
                        }
                    }
                    
                    ++iline;
                    line_left = line_lefts[iline];
                    rect_right = line_left + width;
                } while(rect_right <= target_width);
            }
        }
        
        ASSERT(ibest < 255);
        // If we haven't found a place to put the rect, signal failure and bail.
        if(best_y == target_height) {
            result = false;
            break;
        }
        
        if(line_lefts[ibest + 1] != (best_x + width + 1)) {
            for(u32 iline = line_count + 1; iline > ibest; --iline) {
                line_lefts[iline] = line_lefts[iline - 1];
                line_ys[iline] = line_ys[iline - 1];
            }
            line_lefts[ibest] = best_x;
            line_ys[ibest] = best_y + height;
            ++line_count;
            
            const s16 to_rem = ibestscan - ibest - 1;
            if(to_rem) {
                const u16 lastirem = line_count - to_rem;
                u16 irem = ibest + 1;
                do {
                    line_ys[irem] = line_ys[irem + to_rem];
                    line_lefts[irem] = line_lefts[irem + to_rem];
                    ++irem;
                } while(irem <= lastirem);
                line_count -= to_rem;
            }
            line_lefts[ibest + 1] = best_x + width;
        } else {
            line_ys[ibest] += height;
        }
        
        out_ps[irect] = V2S16(best_x, best_y);
    }
    
    *line_count_inout = line_count;
    return result;
}

inline void s32_dumb_sort_descending(s32 *values, const s32 count) {
    for(s32 j = 0; j < count; ++j) {
        s32 highest = values[j];
        s32 ihighest = j;
        
        for(s32 i = j + 1; i < count; ++i) {
            s32 value = values[i];
            if(value > highest) {
                highest = value;
                ihighest = i;
            }
        }
        
        values[ihighest] = values[j];
        values[j] = highest;
    }
}

inline void u32_dumb_sort_descending(u32 *values, const u32 count) {
    for(u32 j = 0; j < count; ++j) {
        u32 highest = values[j];
        u32 ihighest = j;
        
        for(u32 i = j + 1; i < count; ++i) {
            u32 value = values[i];
            if(value > highest) {
                highest = value;
                ihighest = i;
            }
        }
        
        values[ihighest] = values[j];
        values[j] = highest;
    }
}

inline void f32_dumb_sort_ascending(f32 *values, const u32 count) {
    for(u32 j = 0; j < count; ++j) {
        f32 lowest = values[j];
        u32 ilowest = j;
        
        for(u32 i = j + 1; i < count; ++i) {
            f32 value = values[i];
            if(value < lowest) {
                lowest = value;
                ilowest = i;
            }
        }
        
        values[ilowest] = values[j];
        values[j] = lowest;
    }
}

inline void s16_dumb_sort_descending(s16 *values, const u32 count) {
    for(u32 j = 0; j < count; ++j) {
        s16 highest = values[j];
        u32 ihighest = j;
        
        for(u32 i = j + 1; i < count; ++i) {
            s16 value = values[i];
            if(value > highest) {
                highest = value;
                ihighest = i;
            }
        }
        
        values[ihighest] = values[j];
        values[j] = highest;
    }
}

inline void s16_dumb_sort_ascending(s16 *values, const u32 count) {
    for(u32 j = 0; j < count; ++j) {
        s16 lowest = values[j];
        u32 ilowest = j;
        
        for(u32 i = j + 1; i < count; ++i) {
            s16 value = values[i];
            if(value < lowest) {
                lowest = value;
                ilowest = i;
            }
        }
        
        values[ilowest] = values[j];
        values[j] = lowest;
    }
}

inline void u8_dumb_sort_ascending(u8 *values, const u32 count) {
    for(u32 j = 0; j < count; ++j) {
        u8 lowest = values[j];
        u32 ilowest = j;
        
        for(u32 i = j + 1; i < count; ++i) {
            u8 value = values[i];
            if(value < lowest) {
                lowest = value;
                ilowest = i;
            }
        }
        
        values[ilowest] = values[j];
        values[j] = lowest;
    }
}
