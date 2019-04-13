
#define SAMPLES_PER_CHUNK 65537
#define UNPADDED_SAMPLES_PER_CHUNK 65536
#define MAX_SOUND_COUNT 256
// @Hardcoded: We probably want to pass this and AUDIO_HZ at runtime?
#define AUDIO_BUFFER_SIZE 44100
#define PLAYING_SOUND_HANDLE_BITFIELD_SIZE (IDIV_ROUND_UP(MAX_SOUND_COUNT, 64))
#define AUDIO_PAGE_SIZE UNPADDED_SAMPLES_PER_CHUNK

enum Sound_Uid {
    SOUND_UID_unloaded = 0,
    
    // Music
    SOUND_UID_danse_macabre,
    
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

struct Audio_Volumes {
    union {
        struct {
            f32 master;
            f32 by_category[AUDIO_CATEGORY_COUNT];
        };
        
        f32 all[AUDIO_CATEGORY_COUNT + 1];
    };
};

struct Audio_Play_Commands {
    f32 *old_volume;
    f32 *volume;
    f32 pitch;
    s16 live_plays;
    u8 category;
};

struct Audio_Buffered_Play {
    s16 load;
    u32 pages;
    s8 channel_index;
    s16 commands;
};

struct Audio_Info;
struct Load_Audio_Chunk_Payload {
    Loaded_Sound *loaded;
    Audio_Info *audio;
    s32 chunk_index;
    s16 page;
    s8 channel_index;
};

struct Audio_Info {
    s32 core_count;
    u8 channel_count;
    
    Audio_Volumes current_volume;
    Audio_Volumes target_volume; // ;Settings
    
    f32 *temp_buffer; // f32[audio_buffer_size]
    f32 **mix_buffers; // f32[channel_count][audio_buffer_size]
    s16 *out_buffer; // s16[audio_buffer_size]
    
    Loaded_Sound_Array loads;
    
    s16 *audio_pages[MAX_SOUND_COUNT * 2];
    s32 audio_page_ids[MAX_SOUND_COUNT * 2];
    s16 audio_page_uses[MAX_SOUND_COUNT * 2];
    
    Audio_Play_Commands play_commands[MAX_SOUND_COUNT]; // ;NoRelocate
    u64 free_commands[PLAYING_SOUND_HANDLE_BITFIELD_SIZE];
    
    Load_Audio_Chunk_Payload load_payloads[MAX_SOUND_COUNT * 2]; // ;NoRecolate ;Parallel:audio_pages
    
    Audio_Buffered_Play buffered_plays[MAX_SOUND_COUNT]; // ;NoRelocate
    u64 free_buffered_plays[PLAYING_SOUND_HANDLE_BITFIELD_SIZE];
    
    u64 buffered_play_first_page_loads[PLAYING_SOUND_HANDLE_BITFIELD_SIZE];
    u64 **completed_reads; // ;PerThread Two bits per play.
    
    struct {
        s16 count;
        s16 loads[MAX_SOUND_COUNT];
        s32 samples_played[MAX_SOUND_COUNT];
        f32 sample_offset[MAX_SOUND_COUNT];
        u32 audio_pages[MAX_SOUND_COUNT]; // 0xCCCCNNNN, C = current page, N = next page
        s8 channel_indices[MAX_SOUND_COUNT];
        s16 commands[MAX_SOUND_COUNT];
    } plays;
    
    s16 page_discards[MAX_SOUND_COUNT];
    s16 page_discard_count;
    
    s16 plays_to_retire[MAX_SOUND_COUNT];
    s16 play_retire_count;
    
    s16 sounds_by_uid[SOUND_UID_COUNT];
};
