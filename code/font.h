
enum Font_Index {
    FONT_MONO,
    FONT_FANCY,
    FONT_REGULAR,
    FONT_COUNT,
};

//
// TTF docs:
// https://docs.microsoft.com/en-us/typography/opentype/spec/cmap
//

/* TTF:
[!!] TTF files are big-endian!
Directory(located at the start of the file):
- offset subtable: table count, some(?) table offsets
- table directory: table entries
Actual tables are 4-byte-aligned, which should be taken into account when computing the checksum.

Table tags and their meanings:
- Required:
-   'cmap': character code to glyph index mapping; 0 means that the glyph is not supported.
-   'glyf': glyph data
-   'head': font header
-   'hhea': horizontal header
-   'hmtx': horizontal metrics
-   'loca': index to location (?)
-   'maxp': maximum profile; total number of glyphs, maximum points/contours per glyph...
-   'name': naming (?)
-   'post': PostScript (?)

- Optional:
-   'cvt ': control value (?)
-   'DSIG': digital signature (?)
-   'FFTM': ???
-   'fpgm': font program (?)
-   'gasp': grid-fitting/scan-conversion
-   'GDEF': glyph definition data (?)
-   'GPOS': glyph positioning data
-   'GSUB': glyph substitution data
-   'hdmx': horizontal device metrics
-   'JSTF': justification data
-   'kern': kerning
-   'LTSH': linear threshold data (?)
-   'meta': metadata
-   'OS/2': OS/2 compatibility (?)
-   'PCLT': PCL 5 data (?)
-   'prep': control value program (?)
-   'VDMX': vertical device metrics
*/

enum TTF_cmap_Platform {
    TTF_CMAP_PLATFORM_WINDOWS = 3,
    TTF_CMAP_PLATFORM_MAC = 1,
};

enum TTF_cmap_Encoding {
    TTF_CMAP_ENCODING_WINDOWS_BMP = 1, // 16-bit Unicode table
    TTF_CMAP_ENCODING_WINDOWS_UTF32 = 10, // Full Unicode table
};

enum TTF_Simple_Glyph_Flags {
    TTF_SIMPLE_GLYPH_ON_CURVE_POINT = (1 << 0),
    TTF_SIMPLE_GLYPH_X_SHORT_VECTOR = (1 << 1), // x coords are u8s, not s16s
    TTF_SIMPLE_GLYPH_Y_SHORT_VECTOR = (1 << 2), // Same as above but for y.
    TTF_SIMPLE_GLYPH_REPEAT = (1 << 3), // Subsequent u8 is the number of times this byte should logically repeat
    TTF_SIMPLE_GLYPH_X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR = (1 << 4), // If X_SHORT_VECTOR, stands for the sign of the coordinate. Else, the current x coordinate is the same as the last.
    TTF_SIMPLE_GLYPH_Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR = (1 << 5), // Same as above but for y.
};

enum TTF_Compound_Glyph_Flags {
    TTF_COMPOUND_ARG_1_AND_2_ARE_WORDS = (1 << 0),
    TTF_COMPOUND_ARGS_ARE_XY_VALUES = (1 << 1),
    TTF_COMPOUND_ROUND_XY_TO_GRID = (1 << 2),
    TTF_COMPOUND_WE_HAVE_A_SCALE = (1 << 3),
    TTF_COMPOUND_MORE_COMPONENTS = (1 << 4),
    TTF_COMPOUND_WE_HAVE_AN_X_AND_Y_SCALE = (1 << 5),
    TTF_COMPOUND_WE_HAVE_A_TWO_BY_TWO = (1 << 6),
    TTF_COMPOUND_WE_HAVE_INSTRUCTIONS = (1 << 7),
    TTF_COMPOUND_USE_MY_METRICS = (1 << 8),
    TTF_COMPOUND_OVERLAP_COMPOUND = (1 << 9),
    TTF_COMPOUND_SCALED_COMPONENT_OFFSET = (1 << 10),
    TTF_COMPOUND_UNSCALED_COMPONENT_OFFSET = (1 << 11),
};

enum TTF_kern_Flags {
    TTF_KERN_HORIZONTAL = (1 << 0),
    TTF_KERN_MINIMUM = (1 << 1),
    TTF_KERN_CROSS_STREAM = (1 << 2),
    TTF_KERN_OVERRIDE = (1 << 3),
    TTF_KERN_FIRST_FORMAT_BIT = (1 << 8),
};

#pragma pack(push, 1)
struct TTF_Offset_Subtable {
    u32 scaler_type;
    u16 table_count;
    u16 search_range; // (Largest power of 2 smaller than table_count) * 16
    u16 entry_selector; // log2(largest power of 2 smaller than table_count)
    u16 range_shift; // table_count * 16 - search_range
};
// An array of these directly follows the offset subtable:
struct TTF_Table_Directory {
    union {
        u32 tag;
        u8 tag_characters[4];
    };
    u32 checksum;
    u32 offset_from_start_of_file;
    u32 length_in_bytes;
};

struct TTF_Glyph_Header {
    s16 contour_count; // Negative for a compound glyph.
    s16 x_min;
    s16 y_min;
    s16 x_max;
    s16 y_max;
    /*
    union {
        struct simple {
            u16 last_contour_points[contour_count];
            u16 instruction_count;
            u8 instructions[instruction_count];
            u8 flags[];
            u8 or s16 x_coords[];
            u8 or s16 y_coords[];
        };
        struct compound {
            u16 flags;
            u16 glyph_index;
            s16 or u16 or s8 or u8 argument1;
            s16 or u16 or s8 or u8 argument2;
            [transformation option] (?)
        };
    };
    */
};

struct TTF_head_Header {
    u16 major;
    u16 minor;
    s32 revision;
    u32 checksum_adjustment;
    u32 magic_number;
    u16 flags;
    u16 units_per_em;
    s64 created;
    s64 modified;
    s16 x_min;
    s16 y_min;
    s16 x_max;
    s16 y_max;
    u16 mac_style;
    u16 smallest_readable_size_in_pixels;
    s16 _deprecated;
    s16 loca_format; // 0 is a u16 table that contains the half offsets(!), 1 is u32 that contains the full offsets.
    s16 glyph_format; // Always zero but officially not deprecated/reserved.
};

struct TTF_hhea_Header {
    s32 version;
    s16 ascent;
    s16 descent;
    s16 line_gap;
    u16 max_advance;
    s16 min_left_side_bearing;
    s16 min_right_side_bearing;
    s16 max_extent;
    s16 caret_rise;
    s16 caret_run;
    s16 caret_offset;
    s16 _reserved[4];
    s16 format; // Always zero
    u16 horizontal_metric_count;
};

struct Horizontal_Metric {
    u16 advance;
    s16 left_side_bearing;
};
/*
struct TTF_hmtx_Header {
    Horizontal_Metric metrics[horizontal_metrics_count];
    s16 left_side_bearings[]; // Advance is assumed to be the last in the above table.
};
*/

struct TTF_kern_Header {
    u16 version;
    u16 subtable_count;
};
struct TTF_kern_Subtable_Header {
    u16 version;
    u16 length_in_bytes;
    u16 coverage_flags;
};
struct TTF_kern_Subtable {
    u16 pair_count;
    u16 search_range;
    u16 entry_selector;
    u16 range_shift;
    // Kerning_Pair pairs[pair_count];
};
struct Kerning_Pair {
    union {
        struct {
            u16 left_glyph;
            u16 right_glyph;
        };
        u32 sort_key; // For binary search
    };
    s16 kerning;
};

struct TTF_cmap_Header {
    u16 version; // Always zero.
    u16 table_count; // Number of subsequent encoding tables.
    // TTF_cmap_Encoding_Table[table_count];
};

struct TTF_cmap_Encoding_Table {
    u16 platform;
    /* Platform IDs:
    0: Unicode(encoding should be 5)
    1: Mac(encoding should be 0)
    3: Windows(encoding should be 1 for Unicode fonts, 0 for symbol fonts,
    but can actually range from 0 to 10 with 7, 8, 9 being reserved)
    4: Custom/NT compatibility
    */
    u16 encoding;
    u32 byte_offset_to_subtable;
};
struct TTF_cmap_Format_0_Subtable_Header {
    u16 format; // 0
    u16 length_in_bytes;
    u16 language;
    u8 codepoint_to_index[256];
};
// Parsing format 2 is more involved. See the MS doc for details.
struct TTF_cmap_Sub_Header {
    u16 first_valid_low_byte;
    u16 valid_low_byte_count;
    s16 id_delta;
    u16 id_range_offset;
};
struct TTF_cmap_Format_2_Subtable_Header {
    u16 format; // 2
    u16 length_in_bytes;
    u16 language;
    TTF_cmap_Sub_Header high_bytes_to_sub_headers[256];
    // TTF_cmap_Sub_Header sub_headers[];
    // u16 codepoint_to_index[]; For the low 16 bits.
};
struct TTF_cmap_Format_4_Subtable_Header {
    u16 format; // 4
    u16 length_in_bytes;
    u16 language;
    u16 segment_count_times_two;
    u16 search_range;
    u16 entry_selector;
    u16 range_shift;
    // u16 end_codepoint[segment_count];
    // u16 _padding;
    // u16 start_codepoint[segment_count];
    // s16 index_delta[segment_count]; Delta for all codepoints in the segment.
    // u16 index_offset[segment_count]; Offset into codepoint_to_index.
    // u16 codepoint_to_index[];
};
struct TTF_cmap_Format_6_Subtable_Header {
    u16 format; // 6
    u16 length_in_bytes;
    u16 language;
    u16 first_code;
    u16 entry_count;
    // u16 codepoint_to_index[entry_count];
};
struct TTF_cmap_Mapping_Group {
    /*  This struct was made to facilitate binary searching through codepoint ranges.
    For formats 8 and 12, start_index is the index for the starting codepoint.
    For format 13, start_index is the index for all codepoints between start and end.
    */
    u32 start_codepoint;
    u32 end_codepoint;
    u32 start_index;
};
/* Skipping formats 8 and 10 because they are not supported by Microsoft.
struct TTF_cmap_Format_8_Subtable_Header {
    u16 format; // 8
    u16 _reserved;
    u32 length;
    u32 language;
    u8 is_32_bits[8192]; // bitfield
    u32 group_count;
    // TTF_Sequential_Map_Group groups[group_count];
};
*/
struct TTF_cmap_Format_12_13_Subtable_Header {
    u16 format; // 12 or 13
    u16 _reserved;
    u32 length_in_bytes; // Includes header size
    u32 language;
    u32 group_count;
    // TTF_cmap_Mapping_Group[group_count];
};
// Finally, format 14 is a mess. Again, see the MS doc for more information.
#pragma pack(pop)

struct Font_Parse_Data {
    u8 *glyf;
    u8 *loca;
    TTF_cmap_Format_4_Subtable_Header *cmap;
    TTF_kern_Subtable *kern;
    Horizontal_Metric *horizontal_metrics;
    s16 loca_format;
    u16 horizontal_metric_count;
    s16 ascent;
    s16 descent;
    s16 line_gap;
};

struct Hashed_Glyph_Data {
    s16 pixel_height;
    s16 glyph_index;
    s8  font;
};

// @Cleanup
struct Hashed_Glyph_Additional_Data {
    v2 uvs[2];
    v2 offset;
    v2 halfdim;
    s16 advance;
};

struct Glyph_Visual_Data {
    // @Cutnpaste from Additional_Data
    v2 uvmin;
    v2 uvmax;
    
    // This offset is a halfdim factor.
    // 1.0f aligns the bottom/left of the texture to p,
    // while -1.0f aligns the top/right of the texture.
    v2 offset;
    
    v2s16 dim_in_pixels;
};

struct Glyph_Raster_Data {
    u8 *flags;
    s32 flag_count;
    u8 *x_coords;
    u8 *y_coords;
    u16 *last_contour_points;
    s16 x_min;
    s16 y_min;
    s16 x_max;
    s16 y_max;
};

struct Font_Height_Data {
    s16 ascent;
    s16 descent;
    s16 line_gap;
    f32 inv_height;
};

#define TEXT_BUFFER_LENGTH 8192
#define TEXT_STRING_COUNT 512
#define GLYPH_HASH_SIZE 1024
#define GLYPH_ATLAS_SIZE 1024
#define MAX_RASTERS_PER_UPDATE 64

static u8 hash_glyph(Hashed_Glyph_Data *);
// @XX
static u8 hash_glyph(Hashed_Glyph_Data *a) {return 0;}
#define KEY_TYPE u8
#define STRUCT_TYPE Glyph_Hash
#define ITEM_TYPE Hashed_Glyph_Data
#define MAX_ITEM_COUNT GLYPH_HASH_SIZE
#define HASH_FUNCTION hash_glyph
#include "static_hash.cpp"

struct Text_Info {
    Memory_Block font_block;
    
    struct {
        u8 *glyf_tables                               [FONT_COUNT];
        u8 *loca_tables                               [FONT_COUNT];
        TTF_cmap_Format_4_Subtable_Header *cmap_tables[FONT_COUNT];
        TTF_kern_Subtable *kern_tables                [FONT_COUNT];
        Horizontal_Metric *horizontal_metrics         [FONT_COUNT];
        u16 horizontal_metric_counts                  [FONT_COUNT];
        s16 loca_formats                              [FONT_COUNT];
        s16 ascents                                   [FONT_COUNT];
        s16 descents                                  [FONT_COUNT];
        s16 line_gaps                                 [FONT_COUNT];
        f32 inv_heights                               [FONT_COUNT];
    } fonts;
    
    u8  text_buffer[TEXT_BUFFER_LENGTH];
    s16 text_length;
    
    struct {
        v2   ps            [TEXT_STRING_COUNT];
        f32  heights       [TEXT_STRING_COUNT];
        s16  lengths       [TEXT_STRING_COUNT];
        s16  glyph_counts  [TEXT_STRING_COUNT];
        s8   fonts         [TEXT_STRING_COUNT];
        f32  draw_scales   [TEXT_STRING_COUNT];
        s32  total_advances[TEXT_STRING_COUNT];
        bool centered      [TEXT_STRING_COUNT];
        v4   colors        [TEXT_STRING_COUNT];
        s32  count;
    } strings;
    
    struct {
        s16 indices     [TEXT_BUFFER_LENGTH];
        s16 advances    [TEXT_BUFFER_LENGTH];
        u8  hash_indices[TEXT_BUFFER_LENGTH];
        v2  ps          [TEXT_BUFFER_LENGTH];
        v4  colors      [TEXT_BUFFER_LENGTH];
        s16 count;
    } glyphs;
    
    struct {
        s8  fonts             [MAX_RASTERS_PER_UPDATE];
        s16 left_side_bearings[MAX_RASTERS_PER_UPDATE];
        s16 glyph_indices     [MAX_RASTERS_PER_UPDATE];
        s16 pixel_heights     [MAX_RASTERS_PER_UPDATE];
        u8  hash_indices      [MAX_RASTERS_PER_UPDATE];
        s16 count;
    } raster_requests;
    
    
    u32 glyph_atlas;
    struct {
        s16 lefts[GLYPH_ATLAS_SIZE];
        s16 ys   [GLYPH_ATLAS_SIZE];
        s16 count;
    } glyph_atlas_lines;
    
    Glyph_Hash glyph_hash;
    Hashed_Glyph_Additional_Data glyph_hash_auxiliary[GLYPH_HASH_SIZE]; // ;Parallel:glyph_hash.items
    Glyph_Visual_Data glyph_hash_visual[GLYPH_HASH_SIZE]; // ;Parallel:glyph_hash.items
};
