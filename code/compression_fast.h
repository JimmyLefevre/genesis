
enum LZ_Coding_Choice {
    LZ_NONE = 0,
    LZ_LITERAL = 1,
    LZ_MATCH = 2,
};

struct Coding_Choice {
    u8 choice;
    u16 data;
};
