
#define SAMPLES_PER_CHUNK 65537
#define UNPADDED_SAMPLES_PER_CHUNK 65536
#define MAX_SOUND_COUNT 256
// @Hardcoded: We probably want to pass this and AUDIO_HZ at runtime?
#define AUDIO_BUFFER_SIZE 44100

enum Sound_Uid {
    // Music
    SOUND_UID_danse_macabre = 0,
    
    // Sounds
    SOUND_UID_erase,
    
    SOUND_UID_COUNT,
};

struct Play_Cursor {
    s32 samples_played;
    f32 sample_offset;
    f32 pitch;
};

struct Loaded_Sound {
    Asset_Location streaming_data;
    s32 chunk_count;
    u8 channel_count;
    u8 category;
};

// @Speed: Right now, we're reading from disk for every sound.
// We may want to make a chunk cache in order to avoid doing so.
struct Playing_Sound {
    union {
        struct {
            s16 *current_samples;
            s16 *next_samples;
        };
        s16 *samples[2];
    };
    
    Play_Cursor cursor;
    
    s32 next_chunk_to_load;
    s32 chunk_count;
    s16 loaded;
    u8 channel_index;
    
    bool silent;
    u8 category; // @Compression
    
    v2 position;
    v2 new_position;
};

enum Audio_Category {
    AUDIO_CATEGORY_SFX = 0,
    AUDIO_CATEGORY_MUSIC,
    AUDIO_CATEGORY_COUNT,
};

#define ITEM_TYPE Loaded_Sound
#define ITEM_MAX_COUNT MAX_SOUND_COUNT
#define COUNT_TYPE s32
#define STRUCT_TYPE Loaded_Sound_Array
#include "static_array.cpp"

#define ITEM_TYPE Playing_Sound
#define ITEM_MAX_COUNT MAX_SOUND_COUNT
#define COUNT_TYPE s32
#define STRUCT_TYPE Playing_Sound_Array
#include "static_array.cpp"

#define ITEM_TYPE s32
#define ITEM_MAX_COUNT MAX_SOUND_COUNT
#define COUNT_TYPE s32
#include "static_array.cpp"

struct Audio_Volumes {
    union {
        struct {
            f32 master;
            f32 by_category[AUDIO_CATEGORY_COUNT];
        };
        
        f32 all[AUDIO_CATEGORY_COUNT + 1];
    };
};

struct Audio_Info {
    u8 channel_count;
    
    Audio_Volumes current_volume;
    Audio_Volumes target_volume; // @Settings
    
    f32 *temp_buffer ; // f32[audio_buffer_size]
    f32 **mix_buffers; // f32[channel_count][audio_buffer_size]
    s16  *out_buffer ; // s16[audio_buffer_size]
    
    Loaded_Sound_Array   loads;
    Playing_Sound_Array sounds;
    static_array_s32 sounds_to_destroy;
    
    s16 sounds_by_uid[SOUND_UID_COUNT];
};
