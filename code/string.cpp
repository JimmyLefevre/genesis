
static Decoding_Result utf16_to_codepoint(u16 *utf16) {
    
    Decoding_Result result = {};
    u16 first = *utf16;
    
    if(first <= 0xD7FF || (first >= 0xE000)) {
        
        /* The character is the codepoint. */
        result.codepoint = first;
        result.characters_decoded = 1;
        return(result);
    }
    else {
        
        /* The first character is a surrogate. */
        ASSERT((first >= 0xD800) && (first <= 0xDBFF));
        u16 second = *(utf16 + 1);
        result.codepoint = ((first - 0xD7C0) << 10) | (second & 0x03FF);
        result.characters_decoded = 2;
        return(result);
    }
    /* first >= 0xDC00 && first <= 0xDFFF means a low surrogate. */
}

static Decoding_Result utf8_to_codepoint(u8 *utf8) {
    
    Decoding_Result result = {};
    u8 c = *utf8;
    // We go to the start of the encoded codepoint.
    while((c >> 6) == 2) {
        
        c = *(--utf8);
    }
    
    s16 additional_characters = 0;
    u32 offset = 7;
    while(c & (1 << offset)) {
        
        ++additional_characters;
        --offset;
    }
    
    c = c << additional_characters;
    c = c >> additional_characters;
    result.codepoint = c;
    for(s16 i = 1;
        i < additional_characters;
        ++i) {
        
        c = *++utf8;
        u32 ormask = c & 0x3F;
        result.codepoint = (result.codepoint << 6) | ormask;
    }
    
    result.characters_decoded = 1 + additional_characters;
    return(result);
}

static Encoding_Result codepoint_to_utf16(u32 codepoint) {
    
    Encoding_Result result = {};
    if(codepoint > 0xFFFF) {
        result.c.utf16_1 = (u16) ((0xD7C0) + (codepoint >> 10));
        result.c.utf16_2 = (u16) ((0xDC00 | (codepoint & 0x03FF)));
        result.characters_encoded = 2;
    }
    else {
        result.c.utf16_1 = codepoint & 0xFFFF;
        result.characters_encoded = 1;
    }
    
    return(result);
}

static Encoding_Result codepoint_to_utf8(u32 codepoint) {
    
    Encoding_Result result = {};
    if(codepoint < 0x80) {
        result.c.utf8_1 = (u8)codepoint;
        result.characters_encoded = 1;
    }
    else if(codepoint < 0x800) {
        result.c.utf8_1 = (u8)(0xC0 | (codepoint >> 6));
        result.c.utf8_2 = (u8)(0x80 | (codepoint & 0x3F));
        result.characters_encoded = 2;
    }
    else if(codepoint < 0x10000) {
        result.c.utf8_1 = (u8)(0xE0 | (codepoint >> 12));
        result.c.utf8_2 = (0x80 | ((codepoint >> 6) & 0x3F));
        result.c.utf8_3 = (0x80 | (codepoint & 0x3F));
        result.characters_encoded = 3;
    }
    else if(codepoint < 0x200000) {
        result.c.utf8_1 = (u8)(0xF0 | (codepoint >> 18));
        result.c.utf8_2 = (0x80 | ((codepoint >> 12) & 0x3F));
        result.c.utf8_3 = (0x80 | ((codepoint >> 6) & 0x3F));
        result.c.utf8_4 = (0x80 | (codepoint & 0x3F));
        result.characters_encoded = 4;
    }
    
    return(result);
}

inline u8 *eat_spaces(u8 *ptr) {
    while(*ptr == ' ') {
        ++ptr;
    }
    
    return(ptr);
}

inline u8 *eat_word(u8 *ptr) {
    while((*ptr != ' ') &&
          (*ptr != '\n')) {
        ++ptr;
    }
    ++ptr;
    
    return(ptr);
}

inline u8 *eat_rest_of_line(u8 *ptr) {
    
    while(*ptr != '\n') {
        ++ptr;
    }
    ++ptr;
    return(ptr);
}

inline bool IsWhitespace(u8 *ptr) {
    return !((!*ptr)        ||
             (*ptr == ' ' ) ||
             (*ptr == '\f') ||
             (*ptr == '\n') ||
             (*ptr == '\r') ||
             (*ptr == '\t'));
}
static string tokenize(u8 *ptr) {
    string result;
    result.length = 0;
    result.data = ptr;
    while(!IsWhitespace(ptr)) {
        ++result.length;
        ++ptr;
    }
    ++ptr;
    
    return(result);
}

static bool strings_equal(string a, string b) {
    if(a.length != b.length) {
        return false;
    }
    for(u32 i = 0; i < a.length; ++i) {
        if(a.data[i] != b.data[i]) {
            return false;
        }
    }
    return true;
}

static char *zero_terminate(string s, Memory_Block *temp_mem) {
    
    char *res = (char *)push_size(temp_mem, s.length + 1);
    for(u32 i = 0;
        i < s.length;
        ++i) {
        res[i] = s.data[i];
    }
    res[s.length] = '\0';
    
    return(res);
}


static bool str_eq_zero_terminated(char *a, char *b) {
    
    while(a && b && (a == b)) {
        if((!a) && (!b)) {
            return(true);
        }
    }
    return(false);
}

static bool str_is_at_start_of (string a, char *b) {
    for(u32 i = 0; i < a.length; ++i) {
        if((a.data[i] != b[i]) || (!b[i])) {
            return(false);
        }
    }
    return(true);
}

static s32 utf8_to_utf16(string utf8, u16 *utf16) {
    s32 written = 0;
    for(u32 i = 0; i < utf8.length; ++i) {
        Decoding_Result dr = utf8_to_codepoint(utf8.data);
        utf8.data += dr.characters_decoded;
        Encoding_Result er = codepoint_to_utf16(dr.codepoint);
        for(u32 ci = 0; ci < er.characters_encoded; ++ci) {
            *utf16++ = er.c.E16[ci];
        }
        written += er.characters_encoded;
    }
    return written;
}

static const u32 CRC32Lookup[256] =
{
    0, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

static u32 CRC32(string s) {
    u32 result = 0xFFFFFFFF;
    for(u32 i = 0; i < s.length; ++i) {
        u8 lookup = (u8)(result ^ s.data[i]);
        result = (result >> 8) ^ CRC32Lookup[lookup];
    }
    return ~result;
}

static s32 cstring_length(char *s) {
    s32 result = 0;
    while(*s) {
        ++result;
        ++s;
    }
    return result;
}

static inline usize string_match_length_upto(u8 *s0, u8 *s1, const usize max_length) {
    usize i = 0;
    
    while((s0[i] == s1[i]) && (i < max_length)) {
        ++i;
    }
    
    return i;
}
static inline usize string_match_length_upto(char *s0, char *s1, usize max_length) {
    return string_match_length_upto((u8 *)s0, (u8 *)s1, max_length);
}

static inline u32 string_match_length(string s0, string s1) {
    ssize result = 0;
    ASSERT(s0.length < U32_MAX);
    ASSERT(s1.length < U32_MAX);
    u32 max_match_length = (u32)s_min((s32)s0.length, (s32)s1.length);
    
    return (u32)string_match_length_upto(s0.data, s1.data, max_match_length);
}

static inline ssize cstring_match_length(char *s0, char *s1) {
    ssize result = 0;
    
    while(*s0 && *s1 && (*s0 == *s1)) {
        ++result;
        ++s0;
        ++s1;
    }
    
    return result;
}

static inline string make_string(u8 *data, usize length) {
    string result;
    result.data = data;
    result.length = length;
    return result;
}

// NOTE: This does not support UTF-8.
static inline string advance_to_after_last_occurrence(string from, u8 c) {
    ASSERT(from.length);
    usize i = from.length - 1;
    while((from.data[i] != c) && (i > 0)) {
        --i;
    }
    
    ++i;
    
    string result;
    result.data = from.data + i;
    result.length = from.length - i;
    
    return result;
}

static inline string file_extension(string filename) {
    return advance_to_after_last_occurrence(filename, '.');
}

static inline string push_string(Memory_Block *block, usize size, u32 align = 4) {
    string result;
    result.data = push_array(block, u8, size, 1);
    result.length = size;
    return result;
}

static inline string concat(Memory_Block *block, string a, string b) {
    string result = push_string(block, a.length + b.length);
    mem_copy(a.data, result.data, a.length);
    mem_copy(b.data, result.data + a.length, b.length);
    return result;
}

static inline string concat(Memory_Block *block, string *ss, usize count) {
    ASSERT(count);
    string result = make_string(block->mem + block->used, 0);
    
    FORI_TYPED(usize, 0, count) {
        string s = ss[i];
        // Reserve the memory:
        u8 *append_to = (u8 *)push_size(block, s.length, 1);
        // Do the concat:
        mem_copy(s.data, append_to, s.length);
        result.length += s.length;
    }
    
    return result;
}

static inline string substring(string *from, usize start, usize end) {
    string result;
    result.data = from->data + start;
    result.length = end - start;
    return result;
}

static bool append_string(string *s, string append, usize max_length) {
    if((s->length + append.length + 1) < max_length) {
        u8 *append_at = s->data + s->length;
        mem_copy(append.data, append_at, append.length);
        
        s->length += append.length;
        
        return true;
    } else {
        return false;
    }
}
