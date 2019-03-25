
#define BMP_MAGIC_VALUE (('B' << 0) | ('M' << 8))

#pragma pack(push, 1)
struct BMP_Header {
    // File header:
    u16 magic_value;
    u32 file_size;
    u16 zero_pad0;
    u16 zero_pad1;
    u32 pixel_offset_from_start_of_file;
    
    // Image header:
    u32 header_size;
    u32 width;
    u32 height;
    u16 planes;
    u16 bits_per_pixel;
    u32 compression;
    u32 image_size;
    u32 x_pixels_per_meter;
    u32 y_pixels_per_meter;
    u32 color_map_entry_count;
    u32 significant_color_count;
    
    // Additional information(compression 3):
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
};
#pragma pack(pop)

struct Loaded_BMP {
    u32 *data;
    u32 width;
    u32 height;
};
