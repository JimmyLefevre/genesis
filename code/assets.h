
#define DATAPACK_VERSION 1

enum Asset_UID {
    // Fonts
    ASSET_UID_hack_ttf = 0,
    ASSET_UID_charter_italic_ttf,
    ASSET_UID_charter_regular_ttf,
    
    // Music
    ASSET_UID_danse_macabre_stereo_uvga,
    
    // Sounds
    ASSET_UID_erase_uvga,
    
    // Texture atlases
    ASSET_UID_test_ta,
    
    // Levels
    ASSET_UID_1_lvl,
    
    // Whatevers
    ASSET_UID_test_midi,
    
    ASSET_UID_COUNT,
};

enum Texture_Uid {
    TEXTURE_UID_blank = 0,
    TEXTURE_UID_untextured,
    TEXTURE_UID_circle,
    TEXTURE_UID_default_hitbox,
    TEXTURE_UID_default_hurtbox,
    TEXTURE_UID_default_solid,
    TEXTURE_UID_player,
    TEXTURE_UID_xhair,
    TEXTURE_UID_grid_tile,
    
    TEXTURE_UID_COUNT,
};

#pragma pack(push, 1)

//
// WAV
//

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

//
// Ogg/Vorbis
//

enum Ogg_Header_Type_Flags {
    OGG_HEADER_TYPE_continued_packet                = (1 << 0),
    OGG_HEADER_TYPE_first_page_of_logical_bitstream = (1 << 1),
    OGG_HEADER_TYPE_last_page_of_logical_bitstream  = (1 << 2),
};

struct Ogg_Header {
    union {
        u8 by_byte[4];
        u32 all;
    } capture_pattern;
    u8 stream_structure_version;
    u8 header_type_flags; // ;Flags Ogg_Header_Type_Flags
    u64 absolute_granule_position; // -1: no packets finish on this page
    u32 stream_serial_number;
    u32 page_sequence_number;
    u32 crc; // Polynomial 0x04c11db7, if we ever care
    u8 segment_count;
    // u8 lacing_values[segment_count];
};

enum Vorbis_Header_Type {
    // If bit 0 is not set, the packet is an audio packet.
    VORBIS_HEADER_TYPE_identification = 1,
    VORBIS_HEADER_TYPE_comment = 3,
    VORBIS_HEADER_TYPE_setup = 5,
};

struct Vorbis_Header_Base {
    u8 packet_type;
    u8 magic_number[6];
};

struct Vorbis_Identification_Header {
    u32 vorbis_version; // Must be 0
    u8 audio_channels; // Must be > 0
    u32 sample_rate;   // Must be > 0
    s32 bitrate_maximum;
    s32 bitrate_nominal;
    s32 bitrate_minimum;
    u8 blocksizes; // 0xBA where A is blocksize_0, B is blocksize_1, log encoded
    // blocksize_0 must be <= blocksize_1 and both must be >= 6 and <= 13.
    // u1 framing_flag; // Must be 1
};

struct Vorbis_Codebook {
    u8 *codeword_lengths;
    u16 *multiplicands;
    f32 delta_value;
    f32 minimum_value;
    u32 lookup_values;
    u16 dimensions;
    u8 lookup_type;
    bool sequence_p;
    
    u32 *huffman;
    u8  *huffman_lengths; // @Cleanup: Do we need this?
    u32 entry_count;
};

struct Vorbis_Floor {
    u16 type;
    union {
        struct {
            u8 *books;
            u16 rate;
            u16 bark_map_size;
            u8 order;
            u8 amplitude_bits;
            u8 amplitude_offset;
            u8 book_count;
        } type0;
        
        struct {
            u8 *partition_classes; // u8[class_count]
            u8 *class_dimensions;  // u8[class_count]
            u8 *class_subclasses;  // u8[class_count]
            u8 *class_masterbooks; // u8[class_count]
            u8 *subclass_books;    // u8[class_count][8]
            u16 *Xs;               // u16[65]
            u8 multiplier;
            u8 partition_count;
        } type1;
    };
};

struct Vorbis_Residue {
    u8 *cascade; // u8[classification_count]
    u8 *books;   // u8[classification_count][8]
    u32 begin;
    u32 end;
    u32 partition_size;
    u16 type;
    u8 classification_count;
    u8 classbook;
};

struct Vorbis_Mapping {
    // u16 type; // No reason to store this in Vorbis 1
    u8 *magnitude_channels; // u8[ilog(audio_channels - 1)]
    u8 *angle_channels;     // u8[ilog(audio_channels - 1)]
    u8 *multiplex;          // u8[audio_channels]
    u8 *submap_floors;      // u8[mapping_submaps];
    u8 *submap_residues;    // u8[mapping_submaps];
    u16 coupling_steps;
};

struct Vorbis_Mode {
    // u16 window_type;    // Always 0 in Vorbis 1
    // u16 transform_type; // Always 0 in Vorbis 1
    u8 mapping;
    bool blockflag;
};
#pragma pack(pop)

struct Vorbis_Identification {
    u32 sample_rate;
    u16 blocksize_0;
    u16 blocksize_1;
    u8 audio_channels;
};

struct Vorbis_Setup {
    Vorbis_Codebook *codebooks;
    Vorbis_Floor    *floors;
    Vorbis_Residue  *residues;
    Vorbis_Mapping  *mappings;
    Vorbis_Mode     *modes;
    u8 mode_count;
};

struct Vorbis_Info {
    Vorbis_Identification identification;
    Vorbis_Setup setup;
};

struct Ogg_Vorbis_Reader {
    u8         *packet_base;
    u8         *page_base;
    Ogg_Header *ogg_header;
    u8         *lacing_values;
    s32 current_segment;
};
static Ogg_Vorbis_Reader make_ogg_vorbis_reader(u8 *data) {
    Ogg_Vorbis_Reader result;
    
    result.page_base       = data;
    result.ogg_header      = CAST(Ogg_Header *, data);
    result.lacing_values   = CAST(u8 *, result.ogg_header + 1);
    result.packet_base     = result.lacing_values + result.ogg_header->segment_count;
    result.current_segment = 0;
    
    return result;
}
static inline void step_ogg_vorbis_segment(Ogg_Vorbis_Reader *reader) {
    reader->packet_base += reader->lacing_values[reader->current_segment];
    reader->page_base   += reader->lacing_values[reader->current_segment];
    reader->current_segment += 1;
    
    if(reader->current_segment == reader->ogg_header->segment_count) {
        reader->current_segment = 0;
        reader->page_base += sizeof(Ogg_Header) + sizeof(u8) * reader->ogg_header->segment_count;
        
        reader->ogg_header    = CAST(Ogg_Header *,      reader->page_base);
        reader->lacing_values = CAST(        u8 *, reader->ogg_header + 1);
        reader->packet_base   = reader->lacing_values + reader->ogg_header->segment_count;
    }
}
static inline void step_ogg_vorbis_packet(Ogg_Vorbis_Reader *reader) {
    while(reader->lacing_values[reader->current_segment] == 255) {
        reader->packet_base += reader->lacing_values[reader->current_segment];
        reader->page_base   += reader->lacing_values[reader->current_segment];
        reader->current_segment += 1;
    }
    step_ogg_vorbis_segment(reader);
}

//
// MIDI
// !!!MIDI files are BIG ENDIAN!!!
//
#define MIDI_CHUNK_TYPE_HEADER (FOUR_BYTES_TO_U32('M', 'T', 'h', 'd'))
#define MIDI_CHUNK_TYPE_TRACK  (FOUR_BYTES_TO_U32('M', 'T', 'r', 'k'))

// We need to see which of these we actually care about.
ENUM(MIDI_CONTROLLERS) {
    BANK_SELECT                           = 0,
    MOD_WHEEL                             = 1,
    BREATH_CONTROLLER                     = 2,
    FOOT_CONTROLLER                       = 4,
    PORTAMENTO_TIME                       = 5,
    DATA_ENTRY_MSB                        = 6, // Something to do with pitch bending maybe?
    VOLUME                                = 7,
    BALANCE                               = 8,
    PAN                                   = 0xA,
    EXPRESSION                            = 0xB,
    EFFECT_CONTROL_1                      = 0xC,
    EFFECT_CONTROL_2                      = 0xD,
    GENERAL_PURPOSE_1                     = 0x10,
    GENERAL_PURPOSE_2                     = 0x11,
    GENERAL_PURPOSE_3                     = 0x12,
    GENERAL_PURPOSE_4                     = 0x13,
    SUSTAIN                               = 0x40,
    PORTAMENTO                            = 0x41,
    SOSTENUTO                             = 0x42,
    SOFT_PEDAL                            = 0x43,
    LEGATO_FOOTSWITCH                     = 0x44,
    HOLD_2                                = 0x45,
    SOUND_CONTROLLER_1                    = 0x46, // Sound Variation by default
    SOUND_CONTROLLER_2                    = 0x47, // Timbre/Harmonic Content by default
    SOUND_CONTROLLER_3                    = 0x48, // Release Time by default
    SOUND_CONTROLLER_4                    = 0x49, // Attack Time by default
    SOUND_CONTROLLER_5                    = 0x4A, // Brightness by default
    SOUND_CONTROLLER_6                    = 0x4B, // No default
    SOUND_CONTROLLER_7                    = 0x4C, // No default
    SOUND_CONTROLLER_8                    = 0x4D, // No default
    SOUND_CONTROLLER_9                    = 0x4E, // No default
    SOUND_CONTROLLER_10                   = 0x4F, // No default
    GENERAL_PURPOSE_5                     = 0x50,
    TEMP_CHANGE = 0x51, GENERAL_PURPOSE_6 = 0x51,
    GENERAL_PURPOSE_7                     = 0x52,
    GENERAL_PURPOSE_8                     = 0x53,
    PORTAMENTO_CONTROL                    = 0x54,
    EFFECTS_1_DEPTH                       = 0x5B, // Used to be External Effects Depth
    EFFECTS_2_DEPTH                       = 0x5C, // Used to be Tremolo Depth
    EFFECTS_3_DEPTH                       = 0x5D, // Used to be Chorus Depth
    EFFECTS_4_DEPTH                       = 0x5E, // Used to be Detune Depth
    EFFECTS_5_DEPTH                       = 0x5F, // Used to be Phaser Depth
    DATA_INCREMENT                        = 0x60,
    DATA_DECREMENT                        = 0x61,
    NON_REGISTERED_PARAM_LSB              = 0x62,
    NON_REGISTERED_PARAM_MSB              = 0x63,
    REGISTERED_PARAM_LSB                  = 0x64,
    REGISTERED_PARAM_MSB                  = 0x65,
};

struct Midi_Chunk_Base {
    u32 chunk_type;
    u32 payload_length;
};

struct Midi_Header_Chunk {
    Midi_Chunk_Base base;
    u16 format;
    u16 track_chunk_count;
    u16 time_unit;
};

//
// Internal assets
//

struct Sound_Asset {
    s16 *samples;
    u32 samplesPerChannel;
    u16 channelCount;
};

enum FACS_Encoding_Flags {
    FACS_ENCODING_FLAGS_NONE = 0,
    FACS_ENCODING_FLAGS_SAMPLE_INTERPOLATION_PADDING = (1 << 0),
};

#define UVGA_CODING_CHOICE_RUN_LENGTH_BITS 16
#define UVGA_CODING_CHOICE_BITS_PER_SAMPLE_BITS 5
#define UVGA_CODING_CHOICE_BITS ((UVGA_CODING_CHOICE_RUN_LENGTH_BITS) + (UVGA_CODING_CHOICE_BITS_PER_SAMPLE_BITS))
struct UVGA_Coding_Choice {
    s32 run_length;
    s32 bits_per_sample;
};


#pragma pack(push, 1)
// Uninterleaved Variable-bit Gradient Audio
struct UVGA_Header {
    u32 sample_rate;
    u32 samples_per_chunk;
    u16 chunks_per_channel;
    u8 channel_count;
    u8 bytes_per_sample;
    // u64 chunk_offsets_in_file[chunk_count];
};

struct FACS_Header {
    u32 sample_rate;
    u32 chunk_size;
    u16 chunk_count;
    u8 channel_count;
    u8 bytes_per_sample;
    u16 encoding_flags;
};

//
// TA
//

struct TA_Header {
    u16 texture_count;
    u16 atlas_width;
    u16 atlas_height;
    u8 texture_encoding;
    // @Todo: Change TA data to
    // texture_framing framings[texture_count];
    // u32 *pixels[atlas_width * atlas_height];
};

//
// Datapack
//

struct Datapack_Asset_Info {
    union {
        struct {
            u32 location_low;
            u16 location_high;
            u8 padding;
            u8 compression; // @Cleanup: Remove this.
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

//
// Synth Input
//

#define SYNTH_INPUT_VERSION 0

struct Synth_Input_Note {
    s32 start;
    s32 duration;
    u8  key;
    u8  velocity;
};
struct Synth_Input_Track {
    u16 instrument;
    u32 note_count;
    // [] Synth_Input_Note;
};
struct Synth_Input_Header {
    u32 version;
    u32 ticks_per_second;
    u16 track_count;
    // [] Synth_Input_Track;
};

// Synth1 instrument presets are simple text files; maybe we could read them and translate them to a
// format we like?

/*
// Maybe???
struct Synth_Instrument {
    struct {
        u8 waveform;
        f32 volume;
    } oscillators[3];
    
    f32 delay;
    f32 delay_feedback;
    
    u8 filter_type;
    f32 cutoff;
    f32 resonance;
};
*/

//
// Meshes
//

struct Serialized_Mesh { // ;Serialized
    v2 offset;
    v2 scale;
    v2 rot;
    u8 layer_count;
    
    // v4 layer_colors[layer_count];
    // u16 layer_running_vert_total[layer_count];
    
    // v2 verts[layer_running_vert_total[layer_count-1]];
    // u16 indices[sum(index_count_or_zero(layer_counts))]; where layer_counts is running_total[i] - running_total[i-1]
};
