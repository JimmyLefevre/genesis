
struct Decoding_Result {
    u32 codepoint;
    s16 characters_decoded;
};

struct Character {
    union {
        struct {
            u16 utf16_1;
            u16 utf16_2;
        };
        struct {
            u8 utf8_1;
            u8 utf8_2;
            u8 utf8_3;
            u8 utf8_4;
        };
        u16 E16[2];
        u8 E8[4];
    };
};

struct Encoding_Result {
    Character c;
    u32 characters_encoded;
};

struct String_Builder {
    string buffer;
    usize written;
};
static inline String_Builder make_string_builder(string s) {
    String_Builder result;
    result.buffer = s;
    result.written = 0;
    return result;
}