
inline s32 ascii_to_s32(const char *s) {
    s32 result = 0;
    while((*s >= '0') && (*s <= '9')) {
        result *= 10;
        result += (*s - '0');
        ++s;
    }
    return result;
}

inline s32 ascii_to_s32(const char **s) {
    s32 result = 0;
    while((**s >= '0') && (**s <= '9')) {
        result *= 10;
        result += (**s - '0');
        ++*s;
    }
    return result;
}

const char ascii_digits[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
};
const char ascii_digits_uppercase[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
};
// @Speed: Separate decimal/hex procedures would allow us to use compile-time constants.
inline char *u64_to_ascii(u64 value, char *out, u32 symbol_count, const char *symbol_table) {
    for(;;) {
        u8 mod = (u8)(value % symbol_count);
        *out = symbol_table[mod];
        value /= symbol_count;
        if(!value) break;
        --out;
    }
    return out;
}

inline char *f64_to_ascii(f64 value, char *out, u32 past_point_count = 6) {
    s64 truncated = (s64)value;
    u32 integer_width = 1;
    u64 _t = truncated / 10;
    while(_t) {
        ++integer_width;
        _t /= 10;
    }
    u64_to_ascii(truncated, out + integer_width - 1, 10, ascii_digits);
    out += integer_width;
    *out++ = '.';
    f64 fractional = value - (f64)truncated;
    for(u32 i = 0; i < past_point_count; ++i) {
        fractional *= 10;
        u8 digit = (u8)fractional;
        fractional -= (f64)digit;
        *out++ = '0' + digit;
    }
    return out;
}

//
// This function expects varargs to be the first vararg in the list, i.e.
//
// char *varargs;
// BEGIN_VARARG(varargs, last_arg_before_varargs);
// print([...], varargs);
//
// See right below for examples.
//
static usize _print(char *out, usize out_length, const char *format, char *varargs) {
    if(!out_length) return 0;
    char *started_at = out;
    
    while(*format) {
        if(out_length == 1) {
            *out++ = 0;
            return out - started_at;
        }
        
        if(*format == '%') {
            ++format;
            
            char temp[64];
            char *temp_start = 0;
            
            //
            // Flags:
            //
            bool always_show_sign = false;
            bool pad_with_zeroes = false;
            bool left_justify = false;
            bool space_when_no_sign = false;
            char hex_prefix[2] = {0};
            
            bool exit = false;
            while(!exit) {
                switch(*format) {
                    case '+': {
                        // Add a '+' or '-' before the result.
                        ++format;
                        always_show_sign = true;
                    } break;
                    case '0': {
                        // Pad with zeroes instead of spaces.
                        ++format;
                        pad_with_zeroes = true;
                    } break;
                    case '-': {
                        // Left-justify instead of right-justify when padding.
                        ++format;
                        left_justify = true;
                    } break;
                    case ' ': {
                        // Insert a space instead of omitting the sign entirely.
                        ++format;
                        space_when_no_sign = true;
                    } break;
                    case '#': {
                        ++format;
                        switch(*format) {
                            case 'o': {
                                // Prefix with 0.
                                ++format;
                                hex_prefix[1] = '0';
                            } break;
                            
                            case 'x': {
                                // Prefix with 0x.
                                ++format;
                                hex_prefix[0] = '0';
                                hex_prefix[1] = 'x';
                            } break;
                            
                            case 'X': {
                                // Prefix with 0X.
                                ++format;
                                hex_prefix[0] = '0';
                                hex_prefix[1] = 'X';
                            } break;
                            
                            case 'a':
                            case 'A':
                            case 'e':
                            case 'E':
                            case 'f':
                            case 'F':
                            case 'g':
                            case 'G': {
                                // Always add a decimal point.
                                ++format;
                            } break;
                        }
                    } break;
                    default: {
                        exit = true;
                    } break;
                }
            }
            
            //
            // Width:
            //
            s32 width = 0;
            switch(*format) {
                case '*': {
                    // Width is the next va_arg.
                    width = NEXT_VARARG(varargs, int);
                    ++format;
                } break;
                
                case '0': 
                case '1': 
                case '2': 
                case '3': 
                case '4':
                case '5': 
                case '6': 
                case '7': 
                case '8': 
                case '9': {
                    // Width is given as a number.
                    width = ascii_to_s32(&format);
                } break;
            }
            
            //
            // Precision: @Duplication
            // 
            s32 precision = 0;
            if(*format == '.') {
                ++format;
                switch(*format) {
                    case '*': {
                        precision = NEXT_VARARG(varargs, int);
                        ++format;
                    } break;
                    
                    case '0': 
                    case '1': 
                    case '2': 
                    case '3': 
                    case '4':
                    case '5': 
                    case '6': 
                    case '7': 
                    case '8': 
                    case '9': {
                        precision = ascii_to_s32(&format);
                    } break;
                    
                    default: UNHANDLED;
                }
            }
            
            //
            // Length:
            //
            switch(*format) {
                case 'h': {
                    // Short
                    if(format[1] == 'h') {
                        // Char
                        ++format;
                    }
                } break;
                
                case 'l': {
                    // Long
                    if(format[1] == 'l') {
                        // Long long
                        ++format;
                    }
                } break;
                
                case 'j': {
                    // intmax_t
                } break;
                
                case 'z': {
                    // size_t
                } break;
                
                case 't': {
                    // ptrdiff_t
                } break;
                
                case 'L': {
                    // long double
                } break;
                
                default: {
                    --format;
                } break;
            }
            ++format;
            
            //
            // Specifier:
            //
            bool value_is_positive = true;
            switch(*format) {
                case 'd':
                case 'i': {
                    // Signed decimal
                    s32 value = NEXT_VARARG(varargs, int);
                    if(value < 0) {
                        value = -value;
                        value_is_positive = false;
                    }
                    temp_start = u64_to_ascii(value, temp + 63, 10, ascii_digits);
                } break;
                
                case 'u': {
                    // Unsigned decimal
                    u32 value = NEXT_VARARG(varargs, unsigned int);
                    temp_start = u64_to_ascii(value, temp + 63, 10, ascii_digits);
                } break;
                
                case 'o': {
                    // Unsigned octal
                    u32 value = NEXT_VARARG(varargs, unsigned int);
                    temp_start = u64_to_ascii(value, temp + 63, 8, ascii_digits);
                } break;
                
                case 'x': {
                    // Unsigned hex
                    u32 value = NEXT_VARARG(varargs, unsigned int);
                    temp_start = u64_to_ascii(value, temp + 63, 16, ascii_digits);
                } break;
                
                case 'X': {
                    // Unsigned hex, uppercase
                    u32 value = NEXT_VARARG(varargs, unsigned int);
                    temp_start = u64_to_ascii(value, temp + 63, 16, ascii_digits_uppercase);
                } break;
                
                case 'f': // Float
                case 'F': // Float, uppercase
                case 'e': // Scientific
                case 'E': // Scientific, uppercase
                case 'g': // Shortest representation between 'e' and 'f'
                case 'G': // Shortest representation between 'E' and 'F'
                case 'a': // Hexfloat
                case 'A': // Hexfloat, uppercase
                {
                    f64 value = NEXT_VARARG(varargs, double);
                    if(value < 0.0) {
                        value_is_positive = false;
                        value = -value;
                    }
                    char *end = f64_to_ascii(value, temp);
                    // @Hack: Relocate the string to the end of the buffer.
                    u32 length = (u32)(end - temp);
                    temp_start = temp + 64 - length;
                    for(u32 i = 0; i < length; ++i) {
                        temp_start[i] = temp[i];
                    }
                } break;
                
                case 'c': {
                    // Char passed as an int
                    s8 value = (s8)NEXT_VARARG(varargs, int);
                    *out++ = value;
                    --out_length;
                } break;
                
                case 's': {
                    // String
                    char *s = NEXT_VARARG(varargs, char *);
                    s32 count = (s32)(out_length - 1);
                    if(precision) {
                        count = (count < precision) ? count : precision;
                    }
                    for(s32 i = 0; (i < count) && *s; ++i) {
                        *out++ = *s++;
                        --out_length;
                    }
                } break;
                
                case 'p': {
                    // Pointer
                    void *value = NEXT_VARARG(varargs, void *);
                    temp_start = u64_to_ascii((u64)value, temp + 63, 15, ascii_digits);
                } break;
                
                case 'n': {
                    // Nothing
                    // int *varargs_printed_so_far =
                    NEXT_VARARG(varargs, int *);
                } break;
                
                case '%': {
                    // %
                    *out++ = '%';
                    --out_length;
                } break;
                
                default: UNHANDLED;
            }
            ++format;
            
            if(temp_start) {
                if(hex_prefix[1]) {
                    --temp_start;
                    *temp_start = hex_prefix[1];
                    if(hex_prefix[0]) {
                        --temp_start;
                        *temp_start = hex_prefix[0];
                    }
                }
                
                if(value_is_positive) {
                    if(always_show_sign) {
                        --temp_start;
                        *temp_start = '+';
                    } else if(space_when_no_sign) {
                        --temp_start;
                        *temp_start = ' ';
                    }
                } else {
                    --temp_start;
                    *temp_start = '-';
                }
                
                s32 _count0 = (s32)(temp + 64 - temp_start);
                s32 _count1 = (s32)(out_length - 1);
                s32 count = (_count0 < _count1) ? _count0 : _count1;
                if(precision) {
                    // For integers only.
                    count = (count < precision) ? precision : count;
                    char *_temp_start = (temp + 64 - count);
                    while(_temp_start < temp_start) {
                        --temp_start;
                        *temp_start = '0';
                    }
                }
                if(!left_justify && width) {
                    // For integers only.
                    count = (count < width) ? width : count;
                    char *_temp_start = (temp + 64 - count);
                    while(_temp_start < temp_start) {
                        --temp_start;
                        *temp_start = (pad_with_zeroes) ? '0' : ' ';
                    }
                }
                for(s32 i = 0; i < count; ++i) {
                    *out++ = *temp_start++;
                }
                out_length -= count;
                
                if(left_justify && width) {
                    s32 _to_add0 = width - count;
                    s32 _to_add1 = (s32)(out_length - 1);
                    s32 to_add = (_to_add0 < _to_add1) ? _to_add0 : _to_add1;
                    for(s32 i = 0; i < to_add; ++i) {
                        *out++ = (pad_with_zeroes) ? '0' : ' ';
                        ++count;
                        --out_length;
                    }
                }
            }
        } else {
            *out++ = *format++;
            --out_length;
        }
    }
    
    if(out_length > 0) {
        *out = 0;
    } else {
        *(out - 1) = 0;
    }
    
    return out - started_at;
}

// Vararg wrapper:
inline usize print(char *out, usize out_length, const char *format, ...) {
    char *varargs;
    BEGIN_VARARG(varargs, format);
    return _print(out, out_length, format, varargs);
}
inline usize print(u8 *out, usize out_length, const char *format, ...) {
    char *varargs;
    BEGIN_VARARG(varargs, format);
    return _print((char *)out, out_length, format, varargs);
}