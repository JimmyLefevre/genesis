
struct bitwriter {
    u64 scratch;
    u32 *buffer;
    u32 bits_in_scratch;
    u32 words_written;
};

struct bitreader {
    u64 scratch;
    u32 *buffer;
    u32 bits_in_scratch;
    u32 words_read;
};
