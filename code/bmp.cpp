
inline u32 bmp_mask_lsb(u32 mask) {
    u32 result = 0;
    while(!(mask & 1)) {
        result += 8;
        mask >>= 8;
        ASSERT(result < 32);
    }
    return result;
}

// @Leak: Only use this offline or copy the result into a buffer and then free.
static Loaded_BMP load_bmp(string name) {
    string loaded = os_platform.read_entire_file(name);
    
    ASSERT(loaded.length);
    BMP_Header *header = (BMP_Header *)loaded.data;
    ASSERT(header->magic_value == BMP_MAGIC_VALUE);
    ASSERT(!header->zero_pad0 && !header->zero_pad1);
    ASSERT(header->header_size >= 40);
    ASSERT(header->planes == 1);
    ASSERT(header->bits_per_pixel == 32);
    ASSERT(header->compression == 3);
    
    Loaded_BMP result;
    result.data = (u32 *)(loaded.data + header->pixel_offset_from_start_of_file);
    result.width = header->width;
    result.height = header->height;
    
    u32 alpha_mask = ~(header->red_mask | header->green_mask | header->blue_mask);
    // @Speed: We could get away with only probing the 0th, 8th, 16th and 24th bits instead.
    u32 shift_r = bmp_mask_lsb(header->red_mask);
    u32 shift_g = bmp_mask_lsb(header->green_mask);
    u32 shift_b = bmp_mask_lsb(header->blue_mask);
    u32 shift_a = bmp_mask_lsb(alpha_mask);
    
    u32 stride = result.width; //(result.width + 3) & ~3;
    for(u32 j = 0; j < result.height; ++j) {
        for(u32 i = 0; i < result.width; ++i) {
            u32 *pixel = result.data + j * stride + i;
            
            u8 a = (u8)(*pixel >> shift_a);
            u8 r = (u8)(*pixel >> shift_r);
            u8 g = (u8)(*pixel >> shift_g);
            u8 b = (u8)(*pixel >> shift_b);
            *pixel = (a << 24) | (b << 16) | (g << 8) | (r << 0);
        }
    }
    
    return result;
}
