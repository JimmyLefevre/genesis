
#define MAX_MATCH_COUNT 8
#define MIN_MATCH_LENGTH 2
#define HASH_WAYS 16

#define LRL_MOD_BITS 1
#define MATCH_LENGTH_MOD_BITS 2
#define MATCH_OFFSET_MOD_BITS 4

#define LRL_MOD (1 << LRL_MOD_BITS)
#define MATCH_LENGTH_MOD (1 << MATCH_LENGTH_MOD_BITS)
#define MATCH_OFFSET_MOD (1 << MATCH_OFFSET_MOD_BITS)

#define HEADER_LRL_BITS 3
#define HEADER_MATCH_LENGTH_BITS 3
#define HEADER_OFFSET_BITS 2

#define MAX_HEADER_LRL ((1 << HEADER_LRL_BITS) - 1)
#define MAX_HEADER_MATCH_LENGTH ((1 << HEADER_MATCH_LENGTH_BITS) - 1)
#define MAX_HEADER_OFFSET ((1 << HEADER_OFFSET_BITS) - 1)

// This determines the granularity at which we check the match table and our parse
// lookahead. It is *NOT* the granularity at which we reset the match table.
// Right now, for a single file, the match table is never cleared; we're not chunking our compression.
// We may want to do that for multithreaded decompression.
#define PARSE_BLOCK_LENGTH 131072

// Taken from stb_lib.h:
static inline u32 stb_hash(u8 *s, usize length) {
    u32 hash = 0;
    
    for(usize i = 0; i < length; ++i) {
        hash = (hash << 7) + (hash >> 25) + s[i];
    }
    hash = hash + (hash >> 16);
    
    return hash;
}

static inline void encode_mod_bytewise(u8 **_at, s32 value, s32 mod) {
    u8 *at = *_at;
    const s32 threshold = 256 - mod;
    
    for(;;) {
        if(value >= threshold) {
            *at++ = (u8)(((value - threshold) % mod) + threshold);
            value /= mod;
        } else {
            *at++ = (u8)value;
            break;
        }
    }
    
    *_at = at;
}

static inline s32 decode_mod_bytewise(u8 **_at, s32 mod) {
    u8 *at = *_at;
    s32 result = 0;
    
    const s32 threshold = 256 - mod;
    s32 mul = 1;
    for(;;) {
        const u8 at_byte = *at++;
        
        if(at_byte < threshold) {
            result += at_byte * mul;
            break;
        } else {
            result += (at_byte - threshold) * mul;
            mul *= mod;
        }
    }
    
    *_at = at;
    return result;
}

static inline s32 encode_mod_bytewise_length(s32 value, s32 mod) {
    const s32 threshold = 256 - mod;
    s32 result = 0;
    
    for(;;) {
        ++result;
        if(value >= threshold) {
            value /= mod;
        } else {
            break;
        }
    }
    
    return result;
}

static inline void encode_mod_bytewise_bits(u8 **_at, s32 value, const s32 bits) {
    u8 *at = *_at;
    const s32 threshold = 256 - (1 << bits);
    const s32 mod_mask = (1 << bits) - 1;
    
    for(;;) {
        if(value >= threshold) {
            *at++ = (u8)(((value - threshold) & mod_mask) + threshold);
            value >>= bits;
        } else {
            *at++ = (u8)value;
            break;
        }
    }
    
    *_at = at;
}

static inline s32 decode_mod_bytewise_bits(u8 **_at, const s32 bits) {
    u8 *at = *_at;
    s32 result = 0;
    
    const s32 threshold = 256 - (1 << bits);
    s32 shift = 0;
    for(;;) {
        const u8 at_byte = *at++;
        
        if(at_byte < threshold) {
            result += at_byte << shift;
            break;
        } else {
            result += (at_byte - threshold) << shift;
            shift += bits;
        }
    }
    
    *_at = at;
    return result;
}

static inline usize match_length_upto(u8 *a, u8 *b, const usize max_length) {
    usize scanned = 0;
    
    while(scanned < max_length) {
        const __m128i av = _mm_loadu_si128(CAST(__m128i *, a + scanned));
        const __m128i bv = _mm_loadu_si128(CAST(__m128i *, b + scanned));
        
        const __m128i eq = _mm_cmpeq_epi8(av, bv);
        u32 match = _mm_movemask_epi8(eq);
        
        if(match == 0xFFFF) {
            scanned += 16;
        } else { // We found a difference.
            while((match & 1) && (scanned < max_length)) {
                match >>= 1;
                ++scanned;
            }
            
            return scanned;
        }
    }
    
    return max_length;
}

static void maybe_update_best_match(u8 **match_string_hash, u32 *match_string_check, const u32 hash_ways,
                                    u8 *at, u32 hash, u32 lookup, usize bytes_left,
                                    u8 **possible_matches, u32 *possible_match_lengths, u8 *possible_match_count, u8 **min_match, bool *updated_min) {
    u8 match_count = *possible_match_count;
    
    for(u32 i = 0; (i < hash_ways) && (match_count < MAX_MATCH_COUNT); ++i) {
        u8 *this_match = match_string_hash[lookup * hash_ways + i];
        u32 check = match_string_check[lookup * hash_ways + i];
        
        if(this_match < *min_match) {
            *min_match = this_match;
            *updated_min = true;
        }
        
        if(this_match && (check == hash)) {
            ssize this_match_offset = at - this_match;
            
            if(this_match_offset > 0) {
                ssize this_match_length = match_length_upto(at, this_match, bytes_left);
                ASSERT(this_match_length <= S32_MAX);
                
                s32 this_min_match_length = MIN_MATCH_LENGTH;
                if(this_match_offset >= 3) {
                    this_min_match_length += encode_mod_bytewise_length((s32)this_match_offset - 3, MATCH_OFFSET_MOD);
                }
                
                if(this_match_length >= this_min_match_length) {
                    possible_matches[match_count] = this_match;
                    possible_match_lengths[match_count] = (u32)this_match_length;
                    ++match_count;
                }
            }
        }
    }
    
    *possible_match_count = match_count;
}

static inline void header_byte_get_info(u8 **_at, s32 *literal_run_length, s32 *match_offset, s32 *match_length) {
    u8 *at = *_at;
    u8 header = *at++;
    
    s32 lrl = header >> 5;
    s32 length = (header >> 2) & 7;
    s32 offset = header & 3;
    
    if(lrl == MAX_HEADER_LRL) {
        lrl += decode_mod_bytewise_bits(&at, LRL_MOD_BITS);
    }
    if(length == MAX_HEADER_MATCH_LENGTH) {
        length += decode_mod_bytewise_bits(&at, MATCH_LENGTH_MOD_BITS);
    }
    if(offset == MAX_HEADER_OFFSET) {
        offset += decode_mod_bytewise_bits(&at, MATCH_OFFSET_MOD_BITS);
    }
    
    *literal_run_length = lrl;
    *match_offset = offset;
    *match_length = length;
    *_at = at;
}

static inline void put_header_byte(u8 **_at, u8 *lr_start, s32 lrl, s32 offset, s32 length) {
    u8 *at = *_at;
    
    u8 *at_header = at++;
    u8 header = 0;
    
    if(lrl >= MAX_HEADER_LRL) {
        header |= MAX_HEADER_LRL << 5;
        lrl -= MAX_HEADER_LRL;
        encode_mod_bytewise_bits(&at, lrl, LRL_MOD_BITS);
    } else {
        header |= lrl << 5;
    }
    
    if(length >= MAX_HEADER_MATCH_LENGTH) {
        header |= MAX_HEADER_MATCH_LENGTH << 2;
        length -= MAX_HEADER_MATCH_LENGTH;
        encode_mod_bytewise_bits(&at, length, MATCH_LENGTH_MOD_BITS);
    } else {
        header |= length << 2;
    }
    
    if(offset >= MAX_HEADER_OFFSET) {
        header |= MAX_HEADER_OFFSET;
        offset -= MAX_HEADER_OFFSET;
        encode_mod_bytewise_bits(&at, offset, MATCH_OFFSET_MOD_BITS);
    } else {
        header |= offset;
    }
    
    *at_header = header;
    *_at = at;
}

static string fastcomp_compress(string source, Memory_Block *work_memory) {
    usize match_table_length = source.length;
    if(source.length > KiB(2)) {
        usize min_match_table_length = source.length >> 8;
        match_table_length = 1024;
        
        s32 shift;
        bitscan_reverse(source.length, &shift);
        match_table_length = ((usize)1 << (shift - 7));
    }
    
    // Memory footprint is ~20MB + match table, which scales with file size.
    u8 **match_table_matches      = push_array(work_memory, u8 *, match_table_length * HASH_WAYS);
    u32 *match_table_checks       = push_array(work_memory, u32, match_table_length * HASH_WAYS);
    u8 **possible_matches         = push_array(work_memory, u8 *, MAX_MATCH_COUNT * PARSE_BLOCK_LENGTH);
    u32 *possible_match_lengths   = push_array(work_memory, u32, MAX_MATCH_COUNT * PARSE_BLOCK_LENGTH);
    u8  *possible_match_counts    = push_array(work_memory, u8, PARSE_BLOCK_LENGTH);
    s32 *costs_to_end             = push_array(work_memory, s32, PARSE_BLOCK_LENGTH * (MAX_HEADER_LRL + 1));
    s32 *best_costs_to_end_by_row = push_array(work_memory, s32, PARSE_BLOCK_LENGTH);
    Coding_Choice *best_choices   = push_array(work_memory, Coding_Choice, PARSE_BLOCK_LENGTH);
    
    zero_mem(match_table_matches, sizeof(u8 *) * match_table_length * HASH_WAYS);
    
    string result;
    result.data = work_memory->mem + work_memory->used;
    u8 *encoding_at = result.data;
    
    usize bytes_parsed = 0;
    s32 lrl_carry_over = 0;
    u8 *lr_start_carry_over = 0;
    while(bytes_parsed < source.length) {
        u8 *block_data = source.data + bytes_parsed;
        s32 block_length = PARSE_BLOCK_LENGTH;
        ssize bytes_left = source.length - bytes_parsed;
        if(bytes_left < block_length) {
            block_length = (s32)bytes_left;
        }
        
        { // Match finding:
            for(ssize ibyte_in_parse_block = 0; ibyte_in_parse_block < block_length; ++ibyte_in_parse_block) {
                u8 *at = block_data + ibyte_in_parse_block;
                usize out_array_index = ibyte_in_parse_block * MAX_MATCH_COUNT;
                usize block_bytes_left = block_length - 1 - ibyte_in_parse_block;
                ASSERT((ssize)block_bytes_left >= 0);
                u8 **this_possible_matches = possible_matches + out_array_index;
                u32 *this_possible_match_lengths = possible_match_lengths + out_array_index;
                
                {
                    u8 possible_match_count = 0;
                    if(at < source.data + source.length - 4) {
                        u32        hash = stb_hash(at, 3);
                        u32 second_hash = stb_hash(at, 4);
                        u32        lookup =        hash % match_table_length;
                        u32 second_lookup = second_hash % match_table_length;
                        
                        
                        ASSERT(ibyte_in_parse_block + block_bytes_left < block_length);
                        u8 *min_match = (u8 *)UPTR_MAX;
                        bool min_match_is_first = false;
                        bool min_match_is_second = false;
                        
                        maybe_update_best_match(match_table_matches, match_table_checks, HASH_WAYS,
                                                at, second_hash, lookup, block_bytes_left,
                                                this_possible_matches, this_possible_match_lengths, &possible_match_count, &min_match, &min_match_is_first);
                        maybe_update_best_match(match_table_matches, match_table_checks, HASH_WAYS,
                                                at, hash, second_lookup, block_bytes_left,
                                                this_possible_matches, this_possible_match_lengths, &possible_match_count, &min_match, &min_match_is_second);
                        
                        u32 min_lookup = lookup;
                        u32 insert_hash = second_hash;
                        if(min_match_is_second) {
                            min_lookup = second_lookup;
                            insert_hash = hash;
                        }
                        
                        // @Speed: We could make this a circular buffer instead.
                        for(s32 i = HASH_WAYS - 1; i > 0; --i) {
                            match_table_matches[min_lookup * HASH_WAYS + i] = match_table_matches[min_lookup * HASH_WAYS + i-1];
                            match_table_checks[min_lookup * HASH_WAYS + i] = match_table_checks[min_lookup * HASH_WAYS + i-1];
                        }
                        match_table_matches [min_lookup * HASH_WAYS + 0] = at;
                        match_table_checks[min_lookup * HASH_WAYS + 0] = insert_hash;
                    }
                    
                    possible_match_counts[ibyte_in_parse_block] = possible_match_count;
                }
            }
        }
        
        for(s32 byte_in_block = block_length - 1; byte_in_block >= 0; --byte_in_block) {
            best_costs_to_end_by_row[byte_in_block] = S32_MAX;
            
            for(s32 lrl_state = 0; lrl_state < (MAX_HEADER_LRL + 1); ++lrl_state) {
                const usize match_index = byte_in_block * MAX_MATCH_COUNT;
                u8 **this_possible_matches = possible_matches + match_index;
                u32 *this_possible_match_lengths = possible_match_lengths + match_index;
                const u8 possible_match_count = possible_match_counts[byte_in_block];
                
                if(byte_in_block == (block_length - 1)) {
                    costs_to_end[byte_in_block * (MAX_HEADER_LRL + 1) + lrl_state] = 0;
                    best_choices[byte_in_block] = {LZ_LITERAL, 0};
                } else {
                    // Consider choices.
                    s32 best_cost_to_end = S32_MAX;
                    Coding_Choice best_choice = {};
                    
                    { // Literal:
                        s32 check_lrl = lrl_state + 1;
                        s32 check_byte_in_block = byte_in_block + 1;
                        
                        // @Hack: We only bump up the encoding cost when the literal run gets to MAX_HEADER_LRL,
                        // not for further multiples of it. Not technically correct, but good enough.
                        u16 cost = 1;
                        if(check_lrl == (MAX_HEADER_LRL)) {
                            ++cost;
                        } else if(check_lrl == (MAX_HEADER_LRL + 1)) {
                            check_lrl = MAX_HEADER_LRL;
                        }
                        
                        s32 check_cost_to_end = costs_to_end[check_byte_in_block * (MAX_HEADER_LRL + 1) + check_lrl];
                        ASSERT(check_byte_in_block == block_length - 1 || check_cost_to_end);
                        s32 cost_to_end = check_cost_to_end + cost;
                        if(cost_to_end < best_cost_to_end) {
                            best_cost_to_end = cost_to_end;
                            best_choice.choice = LZ_LITERAL;
                        }
                    }
                    
                    { // Matches:
                        for(u8 i = 0; i < possible_match_count; ++i) {
                            s32 check_lrl = 0;
                            s32 offset = (s32)(block_data + byte_in_block - this_possible_matches[i]);
                            s32 length = this_possible_match_lengths[i];
                            s32 check_byte_in_block = byte_in_block + length;
                            ASSERT(check_byte_in_block < block_length);
                            
                            s32 cost = 1;
                            if(length >= MAX_HEADER_MATCH_LENGTH) {
                                cost += encode_mod_bytewise_length(length - MAX_HEADER_MATCH_LENGTH, MATCH_LENGTH_MOD);
                            }
                            if(offset >= MAX_HEADER_OFFSET) {
                                cost += encode_mod_bytewise_length(offset - MAX_HEADER_OFFSET, MATCH_OFFSET_MOD);
                            }
                            
                            s32 check_cost_to_end = costs_to_end[check_byte_in_block * (MAX_HEADER_LRL + 1) + check_lrl];
                            s32 cost_to_end = check_cost_to_end + cost;
                            if(cost_to_end < best_cost_to_end) {
                                best_cost_to_end = cost_to_end;
                                best_choice.choice = LZ_MATCH;
                                best_choice.data = i;
                            }
                        }
                    }
                    ASSERT(best_cost_to_end);
                    ASSERT(best_cost_to_end != S32_MAX);
                    costs_to_end[byte_in_block * (MAX_HEADER_LRL + 1) + lrl_state] = best_cost_to_end;
                    
                    if(byte_in_block) {
                        if(best_costs_to_end_by_row[byte_in_block] > best_cost_to_end) {
                            best_costs_to_end_by_row[byte_in_block] = best_cost_to_end;
                            best_choices[byte_in_block] = best_choice;
                        }
                    } else {
                        s32 lrl_start_state = lrl_carry_over;
                        if(lrl_start_state >= (MAX_HEADER_LRL)) {
                            lrl_start_state = MAX_HEADER_LRL - 1;
                        }
                        
                        if(lrl_state == lrl_start_state) {
                            best_choices[0] = best_choice;
                        }
                    }
                }
            }
        }
        
        s32 byte_in_block = 0;
        { // Encoding the parse:
            s32 lrl = lrl_carry_over;
            u8 *lr_start = lr_start_carry_over;
            while(byte_in_block < block_length) {
                Coding_Choice choice = best_choices[byte_in_block];
                
                switch(choice.choice) {
                    case LZ_LITERAL: {
                        if(!lrl) {
                            lr_start = block_data + byte_in_block;
                        }
                        ASSERT(block_data[byte_in_block] == lr_start[lrl]);
                        ++lrl;
                        ++byte_in_block;
                    } break;
                    
                    case LZ_MATCH: {
                        s32 index = choice.data;
                        s32 offset = (s32)(block_data + byte_in_block - possible_matches[byte_in_block * MAX_MATCH_COUNT + index]);
                        s32 length = possible_match_lengths[byte_in_block * MAX_MATCH_COUNT + index];
                        
                        u8 *_at = encoding_at;
                        put_header_byte(&encoding_at, lr_start, lrl, offset, length);
                        
                        for(s32 i = 0; i < lrl; ++i) {
                            *encoding_at++ = *lr_start++;
                        }
                        
                        byte_in_block += length;
                        lrl = 0;
                    } break;
                    
                    default: UNHANDLED;
                }
            }
            
            // Flush the encoder.
            if(lrl) {
                // Only flush in degenerate cases, or at the end of a file.
                if((lrl < byte_in_block) && ((bytes_parsed + byte_in_block) < source.length)) {
                    // byte_in_block -= lrl;
                    lrl_carry_over = lrl;
                    lr_start_carry_over = lr_start;
                } else {
                    put_header_byte(&encoding_at, lr_start, lrl, 0, 0);
                    for(s32 i = 0; i < lrl; ++i) {
                        *encoding_at++ = *lr_start++;
                    }
                    lrl_carry_over = 0;
                    lr_start = 0;
                }
            }
        }
        
        bytes_parsed += byte_in_block;
    }
    
    result.length = encoding_at - result.data;
    ASSERT((work_memory->used + result.length) < work_memory->size);
    work_memory->used += result.length;
    return result;
}

// No range checking, but we should know how large the original file was.
static void fastcomp_decompress(u8 *source, const usize length, u8 *out) {
    u8 * const end = source + length;
    
    while(source < end) {
        s32 literal_run_length;
        s32 match_offset;
        s32 match_length;
        header_byte_get_info(&source, &literal_run_length, &match_offset, &match_length);
        
        for(s32 i = 0; i < literal_run_length; ++i) {
            *out++ = *source++;
        }
        
        for(s32 i = 0; i < match_length; ++i) {
            *out = out[-match_offset];
            out += 1;
        }
    }
}

static inline void fastcomp_decompress(string source, u8 *out) {
    fastcomp_decompress(source.data, source.length, out);
}
