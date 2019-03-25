
struct dsound_buffer {
    s16 *start_of_buffer;
    s16 *fill_sec_1;
    s16 *fill_sec_2;
    u32 fill_bytes_1;
    u32 fill_bytes_2;
    u32 buffer_bytes;
    u32 buffer_samples;
    u32 samples_played;
};
