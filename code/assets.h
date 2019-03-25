
#define DATAPACK_VERSION 0

enum Asset_UID {
    // Fonts
    ASSET_UID_hack_ttf = 0,
    ASSET_UID_charter_italic_ttf,
    ASSET_UID_charter_regular_ttf,
    
    // Music
    ASSET_UID_danse_macabre_stereo_facs,
    
    // Sounds
    ASSET_UID_erase_facs,
    
    // Texture atlases
    ASSET_UID_test_ta,
    
    ASSET_UID_COUNT,
};

#pragma pack(push, 1)
struct WAV_File_Header {
    u32 chunk_id;
    u32 chunk_size;
    u32 wave_id;
};

enum WAV_Chunk_ID {
    WAV_ID_RIFF = FOUR_BYTES_TO_U32('R', 'I', 'F', 'F'),
    WAV_ID_WAVE = FOUR_BYTES_TO_U32('W', 'A', 'V', 'E'),
    WAV_ID_FMT = FOUR_BYTES_TO_U32('f', 'm', 't', ' '),
    WAV_ID_DATA = FOUR_BYTES_TO_U32('d', 'a', 't', 'a'),
};

struct WAV_Chunk_Header {
    u32 id;
    u32 size;
};

struct WAV_Fmt {
    u16 format_tag;
    u16 channel_count;
    u32 samples_per_sec;
    u32 avg_bytes_per_sec;
    u16 data_block_byte_count;
    u16 bits_per_sample;
    u16 extension_size;
    u16 valid_bits_per_sample;
    u32 channel_mask;
    u8 sub_format[16];
};

union WAV_Reader {
    u8 *at;
    WAV_Chunk_Header *h;
};
#pragma pack(pop)

struct Sound_Asset {
    s16 *samples;
    u32 samplesPerChannel;
    u16 channelCount;
};

enum FACS_Encoding_Flags {
    FACS_ENCODING_FLAGS_NONE = 0,
    FACS_ENCODING_FLAGS_SAMPLE_INTERPOLATION_PADDING = (1 << 0),
};

enum Datapack_Compression {
    DATAPACK_COMPRESSION_NONE = 0,
    DATAPACK_COMPRESSION_FASTCOMP = 1,
    // These are not implemented yet.
    DATAPACK_COMPRESSION_LOSSLESS_IMAGE = 2,
    DATAPACK_COMPRESSION_AUDIO = 3,
};

#pragma pack(push, 1)
struct FACS_Header {
    u32 sample_rate;
    u32 chunk_size;
    u16 chunk_count;
    u8 channel_count;
    u8 bytes_per_sample;
    u16 encoding_flags;
};
struct TA_Header {
    u16 texture_count;
    u16 atlas_width;
    u16 atlas_height;
    u8 texture_encoding;
    // @Todo: Change TA data to
    // texture_framing framings[texture_count];
    // u32 *pixels[atlas_width * atlas_height];
};
struct Datapack_Asset_Info {
    union {
        struct {
            u32 location_low;
            u16 location_high;
            u8 padding;
            u8 compression;
        };
        usize all;
    };
};
struct Datapack_Header {
    u32 version;
    // Asset count and IDs are implicit.
    // To get an asset's length, use locations[id + 1] - locations[id].
    // datapack_asset_info asset_table[asset_count];
};
#pragma pack(pop)

struct Datapack_Handle {
    void *file_handle;
    Datapack_Asset_Info *assets;
};

struct Asset_Location {
    void *file_handle;
    u64 offset;
};

struct Asset_Metadata {
    Asset_Location location;
    u64 size;
};

static inline u64 get_offset_in_file(Datapack_Handle *pack, u32 asset_uid) {
    return pack->assets[asset_uid].all & (((u64)1 << 48) - 1);
}
static Asset_Location get_asset_location(Datapack_Handle *pack, u32 asset_uid) {
    ASSERT(asset_uid < ASSET_UID_COUNT);
    Asset_Location result;
    
    result.file_handle = pack->file_handle;
    result.offset = get_offset_in_file(pack, asset_uid);
    
    return result;
}
static Asset_Metadata get_asset_metadata(Datapack_Handle *pack, u32 asset_uid) {
    ASSERT(asset_uid < ASSET_UID_COUNT);
    Asset_Metadata result;
    
    result.location = get_asset_location(pack, asset_uid);
    result.size = get_offset_in_file(pack, asset_uid + 1) - result.location.offset;
    
    return result;
}

struct Asset_Storage {
    Datapack_Handle datapack;
};
