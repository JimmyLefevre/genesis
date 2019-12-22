
#define UNICODE
#define _UNICODE

#include <windows.h>
#define USING_CRT 1

#include "basic.h"
#include "string.h"
#include "math.h"
#include "input.h"
#include "profile.h"
#include "menu.h"
#include "log.h"
#include "font.h"
#include "render_new.h"
#include "assets.h"
#include "audio.h"
#include "synth.h"
#include "mesh.h"
#include "game.h"
#include "bmp.h"
#include "utilities.h"
#include "compression_fast.h"
#include "sse.h"

struct Render_Vertex {
    v2 p;
    v2 uv;
    v4 color;
};

#include "print.cpp"
#include "string.cpp"
#include "compression_fast.cpp"
#include "mesh.cpp"

ENUM(SYNTH1) {
    OSC1_WAVEFORM = 0,
    OSC1_FM = 45,
    OSC1_DETUNE = 76,
    OSC2_WAVEFORM = 1,
    OSC2_PITCH = 2,
    OSC2_FINE_PITCH = 3,
    OSC2_TRACK = 4,
    OSC_MIX = 5,
    OSC2_SYNC = 6,
    OSC2_RING = 7,
    OSC_PULSE_WIDTH = 8,
    OSC_KEY_SHIFT = 9,
    M_ENV = 10,
    M_ENV_AMOUNT = 11,
    M_ENV_A = 12,
    M_ENV_D = 13,
    M_ENV_DESTINATION = 71,
    OSC_DETUNE = 72,
    OSC_PHASE = 91,
    OSC1_SUB_MIX = 95,
    OSC1_SUB_WAVEFORM = 96,
    OSC1_SUB_OCTAVE = 97,
    FILTER_TYPE = 14,
    FILTER_ATTACK = 15,
    FILTER_DECAY = 16,
    FILTER_SUSTAIN = 17,
    FILTER_RELEASE = 18,
    FILTER_CUTOFF = 19,
    FILTER_RESONANCE = 20,
    FILTER_AMOUNT = 21,
    FILTER_TRACKING = 22,
    FILTER_SATURATION = 23,
    FILTER_VELOCITY = 24,
    AMP_ATTACK = 25,
    AMP_DECAY = 26,
    AMP_SUSTAIN = 27,
    AMP_RELEASE = 28,
    AMP_GAIN = 29,
    AMP_VELOCITY_TRACKING = 30,
    ARPEGGIATOR = 59,
    ARPEGGIATOR_TYPE = 31,
    ARPEGGIATOR_RANGE = 32,
    ARPEGGIATOR_BEAT = 33,
    ARPEGGIATOR_GATE = 34,
    DELAY = 65,
    DELAY_TYPE = 82,
    DELAY_TIME = 35,
    DELAY_SPREAD = 83,
    DELAY_FEEDBACK = 36,
    DELAY_TONE = 98,
    DELAY_DRY_WET = 37,
    CHORUS = 66,
    CHORUS_MULTIPLIER = 64,
    CHORUS_TIME = 52,
    CHORUS_DEPTH = 53,
    CHORUS_RATE = 54,
    CHORUS_FEEDBACK = 55,
    CHORUS_LEVEL = 56,
    EQUALIZER_TONE = 60,
    EQUALIZER_FREQUENCY = 61,
    EQUALIZER_LEVEL = 62,
    EQUALIZER_Q = 63,
    EQUALIZER_LR = 90,
    EFFECT = 77,
    EFFECT_TYPE = 78,
    EFFECT_CONTROL1 = 79,
    EFFECT_CONTROL2 = 80,
    EFFECT_LEVEL = 81,
    VOICE_TYPE = 38,
    POLYPHONY = 94,
    PORTAMENTO = 39,
    AUTO_PORTAMENTO = 74,
    UNISON = 73,
    UNISON_COUNT = 93,
    UNISON_DETUNE = 75,
    UNISON_SPREAD = 84,
    UNISON_PITCH = 85,
    UNISON_PHASE = 92,
    PB_RANGE = 40,
    MIDI1_SRC = 86,
    MIDI1_AMOUNT = 50,
    MIDI1_DEST = 87,
    MIDI2_SRC = 88,
    MIDI2_AMOUNT = 51,
    MIDI2_DEST = 89,
    LFO1 = 57,
    LFO1_DEST = 41,
    LFO1_WAVEFORM = 42,
    LFO1_SPEED = 43,
    LFO1_AMOUNT = 44,
    LFO1_TEMPO_SYNC = 67,
    LFO1_KEY_SYNC = 68,
    LFO2 = 58,
    LFO2_DEST = 46,
    LFO2_WAVEFORM = 47,
    LFO2_SPEED = 48,
    LFO2_AMOUNT = 49,
    LFO2_TEMPO_SYNC = 69,
    LFO2_KEY_SYNC = 70,
};

static inline s32 midi_decodemod_and_advance(u8 **at) {
    MIRROR_VARIABLE(u8 *, _at, at);
    s32 result = 0;
    
    for(;;) {
        u8 byte = *_at;
        result <<= 7;
        result |= byte & ((1 << 7) - 1);
        
        _at += 1;
        
        if(!(byte & (1 << 7))) {
            break;
        }
    }
    
    return result;
}

static void *win_mem_alloc(usize size) {
    return VirtualAlloc(0, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
}

static Memory_Block allocate_memory_block(usize size) {
    Memory_Block block;
    
    block.used = 0;
    block.size = size;
    block.mem = (u8 *)win_mem_alloc(size);
    
    return block;
}

static bool write_entire_file(string file_name, string file_data) {
    ASSERT(file_name.length < 100);
    u16 utf16filename[100] = {0};
    utf8_to_utf16(file_name, utf16filename);
    
    bool success = false;
    
    HANDLE file_handle = CreateFileW(CAST(LPCWSTR, utf16filename),
                                     GENERIC_READ|GENERIC_WRITE,
                                     FILE_SHARE_READ|FILE_SHARE_WRITE,
                                     0,
                                     CREATE_ALWAYS,
                                     0,
                                     0);
    
    if(file_handle != INVALID_HANDLE_VALUE) {
        
        u32 bytes_written;
        if(WriteFile(file_handle, file_data.data, (u32)file_data.length, CAST(LPDWORD, &bytes_written), 0) &&
           (bytes_written == file_data.length)) {
            success = true;
        }
        
        CloseHandle(file_handle);
    }
    
    return(success);
}

static usize win_file_size(void *file_handle) {
    LARGE_INTEGER file_size;
    if(GetFileSizeEx(file_handle, &file_size)) {
        return(file_size.QuadPart);
    }
    return(0);
}

static void *win_open_file_utf16(u16 *file_name) {
    HANDLE file_handle = CreateFileW(CAST(LPCWSTR, file_name), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    
    return(file_handle);
}

static void *win_open_file_utf8(string file_name) {
    ASSERT(file_name.length < 100);
    u16 utf16filename[256];
    s32 written = utf8_to_utf16(file_name, utf16filename);
    utf16filename[written] = '\0';
    
    return win_open_file_utf16(utf16filename);
}

inline void win_free(void *data) {
    if(data) VirtualFree(data, 0, MEM_RELEASE);
}

static string read_entire_file(string file_name) {
    ASSERT(file_name.length < 100);
    u16 utf16filename[100] = {0};
    utf8_to_utf16(file_name, utf16filename);
    
    string result = {0};
    
    void *file_handle = win_open_file_utf16(utf16filename);
    
    if(file_handle != INVALID_HANDLE_VALUE) {
        
        u64 file_size = win_file_size(file_handle);
        ASSERT(file_size);
        ASSERT(file_size <= 0xFFFFFFFF);
        // @Cleanup: Pass a memory block instead of VirtualAllocing.
        void *file_data = win_mem_alloc(file_size);
        
        u32 bytes_read;
        if(ReadFile(file_handle, file_data, (u32)file_size, CAST(LPDWORD, &bytes_read), 0) &&
           (bytes_read == file_size)) {
            result.data = (u8 *)file_data;
            result.length = bytes_read;
        }
        else {
            win_free(file_data);
        }
        CloseHandle(file_handle);
    }
    
    return(result);
}

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
    string loaded = read_entire_file(name);
    
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


static Sound_Asset load_wav(string name) {
    Sound_Asset result = {};
    s16 *read_data = 0;
    string loaded = read_entire_file( name );
    
    WAV_File_Header *file_header = (WAV_File_Header *)loaded.data;
    ASSERT((file_header->chunk_id == WAV_ID_RIFF) &&
           (file_header->wave_id == WAV_ID_WAVE));
    
    WAV_Reader reader;
    for(reader.at = (u8 *)(file_header + 1);
        reader.at < (((u8 *)(file_header + 1)) + file_header->chunk_size - 4);
        reader.at += (((reader.h->size + 1) & ~1) + sizeof(WAV_Chunk_Header))) {
        
        if(reader.h->id == WAV_ID_FMT) {
            ASSERT((reader.h->id == WAV_ID_FMT) &&
                   ((reader.h->size == 16) ||
                    (reader.h->size == 18) ||
                    (reader.h->size == 40)));
            
            WAV_Fmt *format = (WAV_Fmt *)(reader.h + 1);
            
            ASSERT((format->format_tag == 1) || // PCM...
                   ((format->format_tag == 0xFFFE) && (format->sub_format[0] == 1)));
            ASSERT(format->bits_per_sample == 16); // ...ie. signed 16-bit.
            ASSERT(format->data_block_byte_count == (format->channel_count*2));
            
            result.channelCount = format->channel_count;
        }
        else if(reader.h->id == WAV_ID_DATA) {
            read_data = (s16 *)(reader.h + 1);
            result.samplesPerChannel = reader.h->size / (result.channelCount * sizeof(s16));
        }
    }
    ASSERT( read_data && ( result.channelCount < 3 ) );
    result.samples = read_data;
    
    return result;
}

// :ReenableGraphics
string files_convert_bmp_to_ta(string *bmp_names, v2 *bmp_halfdims, v2 *bmp_offsets, u32 file_count, Memory_Block *temp_block) {
#if 0
    SCOPE_MEMORY(temp_block);
    Loaded_BMP *bmps = push_array(temp_block, Loaded_BMP, file_count);
    v2s16 *rect_dims = push_array(temp_block, v2s16, file_count);
    
    for(u32 ibmp = 0; ibmp < file_count; ++ibmp) { SCOPE_MEMORY(temp_block);
        string bmp_name = bmp_names[ibmp];
        Loaded_BMP bmp = load_bmp(bmp_name);
        
        v2s16 dim = V2S16((s16)(bmp.width + 2), (s16)(bmp.height + 2));
        
        bmps[ibmp] = bmp;
        rect_dims[ibmp] = dim;
    }
    
    // :TAAtlasSize @Incomplete: Adjust atlas dimensions based on BMPs' dimensions?
    TA_Header header;
    header.texture_count = (u16)file_count;
    header.atlas_width = 1024;
    header.atlas_height = 1024;
    header.texture_encoding = 0;
    
    // 24 bytes per texture entry: 8 for ID, 8*2 for UVs.
    // ^ Is this even correct anymore???
    u64 file_size = sizeof(TA_Header) + sizeof(Texture_Framing) * header.texture_count +
        4 * header.atlas_width * header.atlas_height;
    string write_file = make_string(push_array(temp_block, u8, file_size), file_size);
    zero_mem(write_file.data, write_file.length);
    
    TA_Header *out_header = (TA_Header *)write_file.data;
    *out_header = header;
    
    Texture_Framing *out_framings = (Texture_Framing *)(out_header + 1);
    
    v2s16 *rect_ps = push_array(temp_block, v2s16, file_count);
    { // Packing.
        ASSERT((header.atlas_width < S16_MAX) && (header.atlas_height < S16_MAX));
        v2s16 target_dim = V2S16(header.atlas_width, header.atlas_height);
        
        s16 *line_lefts = push_array(temp_block, s16, 256);
        s16 *line_ys = push_array(temp_block, s16, 256);
        s16 line_count;
        rect_pack_init(line_lefts, line_ys, &line_count, target_dim.x);
        // :TAAtlasSize @Incomplete: If rect_pack fails, double the atlas dimensions?
        rect_pack(rect_dims, (s16)file_count, line_lefts, line_ys, &line_count,
                  target_dim.x, target_dim.y, rect_ps);
    }
    
    // Writing textures into the atlas
    // Computing UVs
    // Writing Texture_Framings
    u32 *texture = (u32 *)(out_framings + file_count);
    const f32 f_atlas_width = (f32)header.atlas_width;
    const f32 f_atlas_height = (f32)header.atlas_height;
    const f32 inv_atlas_width = 1.0f / f_atlas_width;
    const f32 inv_atlas_height = 1.0f / f_atlas_height;
    const f32 inv_255 = 1.0f / 255.0f;
    for(u32 iuv = 0; iuv < file_count; ++iuv) {
        Loaded_BMP bmp = bmps[iuv];
        v2s16 rect_min = rect_ps[iuv] + V2S16(1, 1);
        v2s16 rect_max = rect_ps[iuv] + rect_dims[iuv] - V2S16(1, 1);
        
        const s32 width = rect_max.x - rect_min.x;
        const v2 uvmin = V2(((f32)rect_min.x) * inv_atlas_width,
                            ((f32)rect_min.y) * inv_atlas_height);
        const v2 uvmax = V2(((f32)rect_max.x) * inv_atlas_width,
                            ((f32)rect_max.y) * inv_atlas_height);
        
        Texture_Framing framing;
        framing.offset = bmp_offsets[iuv];
        framing.halfdim = bmp_halfdims[iuv];
        
        for(s32 y = rect_min.y; y <= rect_max.y; ++y) {
            const s32 yoffset = (y - rect_min.y) * width;
            for(s32 x = rect_min.x; x <= rect_max.x; ++x) {
                s32 itexel = yoffset + x - rect_min.x;
                const u32 read_pixel = bmp.data[itexel];
                
                // Premultiplying alpha:
                f32 r = (read_pixel & 0xFF) * inv_255;
                f32 g = ((read_pixel >> 8) & 0xFF) * inv_255;
                f32 b = ((read_pixel >> 16) & 0xFF) * inv_255;
                f32 a = ((read_pixel >> 24) & 0xFF) * inv_255;
                r *= a;
                g *= a;
                b *= a;
                ASSERT((r >= 0.0f) && (r <= 1.0f));
                ASSERT((g >= 0.0f) && (g <= 1.0f));
                ASSERT((b >= 0.0f) && (b <= 1.0f));
                ASSERT((a >= 0.0f) && (a <= 1.0f));
                
                u32 write_pixel = ((u32)(r * 255.0f)) | ((u32)(g * 255.0f) << 8) | 
                    ((u32)(b * 255.0f) << 16) | ((u32)(a * 255.0f) << 24);
                texture[y * header.atlas_width + x] = write_pixel;
            }
        }
        
        framing.uvs.min = uvmin;
        framing.uvs.max = uvmax;
        out_framings[iuv] = framing;
    }
    
#if 0
    // Trying some filters for compression:
    // (With our basic texture atlas, this is actually worsening compression!)
    {
        FORI_REVERSE_NAMED(y, header.atlas_height - 1, 0) {
            FORI_REVERSE_NAMED(x, header.atlas_width - 1, 0) {
                u32 *it = &texture[y * header.atlas_width + x];
                u32 pixel = *it;
                
                s32 r = pixel & 0xFF;
                s32 g = (pixel >>  8) & 0xFF;
                s32 b = (pixel >> 16) & 0xFF;
                s32 a = (pixel >> 24) & 0xFF;
                
                // LOCO Color space:
                r -= g;
                b -= g;
                
                // DPCM:
                // (Clamped grad from JPEG-LS)
                if(y > 0 && x > 0) {
                    s32 nwr, nwg, nwb, nwa;
                    s32 nr, ng, nb, na;
                    s32 wr, wg, wb, wa;
                    unpack_rgba8(texture[(y-1) * header.atlas_width + (x-1)], &nwr, &nwg, &nwb, &nwa);
                    unpack_rgba8(texture[(y-1) * header.atlas_width +  x   ], &nr, &ng, &nb, &na);
                    unpack_rgba8(texture[ y    * header.atlas_width + (x-1)], &wr, &wg, &wb, &wa);
                    
                    s32 dpcm_r = nr + wr - nwr;
                    s32 dpcm_g = ng + wg - nwg;
                    s32 dpcm_b = nb + wb - nwb;
                    s32 dpcm_a = na + wa - nwa;
                    
                    // @Duplication: Make a s_min_max function.
                    s32 min_r = nr;
                    s32 max_r = wr;
                    if(min_r > max_r) {
                        SWAP(min_r, max_r);
                    }
                    dpcm_r = s_clamp(r, min_r, max_r);
                    
                    s32 min_g = ng;
                    s32 max_g = wg;
                    if(min_g > max_g) {
                        SWAP(min_g, max_g);
                    }
                    dpcm_g = s_clamp(g, min_g, max_g);
                    
                    s32 min_b = nb;
                    s32 max_b = wb;
                    if(min_b > max_b) {
                        SWAP(min_b, max_b);
                    }
                    dpcm_b = s_clamp(b, min_b, max_b);
                    
                    s32 min_a = na;
                    s32 max_a = wa;
                    if(min_a > max_a) {
                        SWAP(min_a, max_a);
                    }
                    dpcm_a = s_clamp(a, min_a, max_a);
                    
                    r -= dpcm_r;
                    g -= dpcm_g;
                    b -= dpcm_b;
                    a -= dpcm_a;
                }
                
                *it = ( (r & 0xFF)        | 
                       ((g & 0xFF) << 8)  | 
                       ((b & 0xFF) << 16) | 
                       ((a & 0xFF) << 24));
            }
        }
    }
#endif
    
    // LZ:
    {
        string lz = fastcomp_compress(write_file, temp_block);
        __debugbreak();
    }
    
    return write_file;
#endif
    return STRING("");
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    auto memory = allocate_memory_block(GiB(2));
    {
        s32 ok = SetCurrentDirectoryW(L"W:\\genesis\\assets\\");
        ASSERT(ok);
    }
    
    
    { // Asset build.
#if 0
        string *data_files = push_array(&memory, string, ASSET_UID_COUNT);
        usize data_file_count = 0;
        
        { // Fonts
            string font_names[] = {
                STRING("fonts/Hack-Regular.ttf"),
                STRING("fonts/Charter Italic.ttf"),
                STRING("fonts/Charter Regular.ttf"),
            };
            
            SCOPE_MEMORY(&memory);
            
            for(s32 ifont = 0; ifont < ARRAY_LENGTH(font_names); ++ifont) {
                string font_full_path = font_names[ifont];
                string font_file = read_entire_file(font_full_path);
                
                data_files[data_file_count++] = font_file;
            }
        }
        
#if 0
        { // Audio files
            string wav_files[] = {
                STRING("music/Danse Macabre.wav"),
                STRING("sounds/erase4.wav"),
            };
            const usize wav_file_count = ARRAY_LENGTH(wav_files);
            files_convert_wav_to_uvga(wav_files, ARRAY_LENGTH(wav_files), game_block, data_files + data_file_count);
            
            data_file_count += wav_file_count;
        }
#endif
        
#endif
        { // MIDI -> Synth_Input
            string midi_filenames[] = {
                STRING("music/test3.mid"),
            };
            
            FORI_NAMED(file_index, 0, ARRAY_LENGTH(midi_filenames)) {
                SCOPE_MEMORY(&memory);
                string midi_filename = midi_filenames[file_index];
                string midi_file = read_entire_file(midi_filename);
                
                auto *base = CAST(Midi_Chunk_Base *, midi_file.data);
                ASSERT(base->chunk_type == MIDI_CHUNK_TYPE_HEADER);
                u32 payload_length = read_u32_be(&base->payload_length);
                
                auto *header = CAST(Midi_Header_Chunk *, base);
                u16 track_chunk_count = read_u16_be(&header->track_chunk_count);
                s16 format = read_u16_be(&header->format);
                s16 time_unit = read_s16_be(CAST(s16 *, &header->time_unit));
                
                s32 ticks_per_quarter_note = 0;
                
                if(format == 0) {
                    // Single track
                } else if(format == 1) {
                    // 1+ simultaneous tracks
                } else if(format == 2) {
                    // 1+ independent tracks
                }
                
                if(time_unit >= 0) {
                    ticks_per_quarter_note = time_unit;
                } else {
                    // @???
                    UNHANDLED;
                    
                    s16 ticks_per_SMTPE_frame = time_unit & 0xFF;
                    s16 SMTPE_fps = time_unit >> 8;
                    if(SMTPE_fps == -24) {
                        // 24 FPS
                    } else if(SMTPE_fps == -25) {
                        // 25 FPS
                    } else if(SMTPE_fps == -29) {
                        // 30 FPS, drop frame
                    } else if(SMTPE_fps == -30) {
                        // 30 FPS, non-drop frame
                    }
                }
                
                usize out_file_reserved_length = KiB(64);
                string out_file = print(&memory, "test%d.synth_input", file_index);
                String_Builder out_file_builder = make_string_builder(push_string(&memory, out_file_reserved_length));
                auto out_header = RESERVE_STRUCT(&out_file_builder, Synth_Input_Header);
                out_header->version = SYNTH_INPUT_VERSION;
                
                const u32 track_note_buffer_length = 65536;
                const u32 track_note_buffer_mask   = track_note_buffer_length - 1;
                Synth_Input_Note *track_notes = push_array(&memory, Synth_Input_Note, track_note_buffer_length);
                
                u16 tracks_with_notes = 0;
                FORI(0, track_chunk_count) {
                    Synth_Input_Track* track = RESERVE_STRUCT(&out_file_builder, Synth_Input_Track);
                    u32 track_note_count = 0;
                    Synth_Input_Note note_state[256] = {};
                    s32 current_time           =  0;
                    
                    base = CAST(Midi_Chunk_Base *, CAST(u8 *, base + 1) + payload_length);
                    ASSERT(base->chunk_type == MIDI_CHUNK_TYPE_TRACK);
                    
                    payload_length = read_u32_be(&base->payload_length);
                    
                    // A MIDI track chunk has a base, then an event.
                    u8 *event = CAST(u8 *, base + 1);
                    
                    u8 channel_status = 0;
                    
                    for(;;) {
                        const s32 dt = midi_decodemod_and_advance(&event);
                        const u8 maybe_status = *event++;
                        
                        current_time += dt;
                        
                        if(maybe_status > 0x7F) {
                            // We have a new status byte.
                            channel_status = 0;
                            switch(maybe_status) {
                                case 0xFF: {
                                    // Meta event.
                                    u8 first_byte = *event++;
                                    
                                    switch(first_byte) {
                                        // One-byte event types are all text, except 0x7F.
                                        case 1:      // Arbitrary text
                                        case 2:      // Copyright
                                        case 3:      // Track name
                                        case 4:      // Instrument name
                                        case 5:      // Lyric
                                        case 6:      // Marker
                                        case 7:      // Cues
                                        case 0x7F: { // Manufacturer-specific event
                                            // We just skip these.
                                            
                                            // @Incomplete: If we do decide to use track names to identify instruments,
                                            // this is the place. Confirmed working with REAPER's output.
                                            
                                            s32 length = midi_decodemod_and_advance(&event);
                                            event += length;
                                        } break;
                                        
                                        default: {
                                            u16 event_type = read_u16_be(CAST(u16 *, event - 1));
                                            event += 1;
                                            
                                            switch(event_type) {
                                                case 2: {
                                                    // Sequence number
                                                    // Skip
                                                    event += 2;
                                                } break;
                                                
                                                case 0x2001: {
                                                    // MIDI channel prefix: all non-MIDI events target this channel
                                                    s8 channel = *event++;
                                                } break;
                                                
                                                case 0x2F00: {
                                                    // End of track
                                                    goto midi_parse_end;
                                                } break;
                                                
                                                case 0x5103: {
                                                    // Set tempo, in microseconds per quarter note.
                                                    // Only expected in "ticks per quarter note" time units.
                                                    // Default is 120BPM ie. 500000us/quarter note.
                                                    s32 tempo = read_u32_be(CAST(u32 *, event - 1)) & 0x00FFFFFF;
                                                    event += 3;
                                                    
                                                    u32 ticks_per_second = (1000000 * ticks_per_quarter_note) / tempo;
                                                    f64 precision_check = (1000000.0 * CAST(f64, ticks_per_quarter_note)) / CAST(f64, tempo);
                                                    u32 precision_check_quantized = f_round_s(precision_check);
                                                    
                                                    ASSERT(precision_check_quantized == ticks_per_second);
                                                    
                                                    out_header->ticks_per_second = ticks_per_second;
                                                } break;
                                                
                                                case 0x5405: {
                                                    // SMTPE offset for the start of the track.
                                                    // Default is 0.
                                                    // In format 1, must be on the 1st track.
                                                    
                                                    u8 hours                 = *event++;
                                                    u8 minutes               = *event++;
                                                    u8 seconds               = *event++;
                                                    u8 frames                = *event++;
                                                    u8 hundredths_of_a_frame = *event++;
                                                } break;
                                                
                                                case 0x5804: {
                                                    // Time signature
                                                    u8 numerator                              = *event++;
                                                    u32 denominator                           = 1 << *event++;
                                                    u8 clocks_per_metronome_tick              = *event++;
                                                    u8 thirty_second_notes_per_24_midi_clocks = *event++; // 8 is the standard
                                                } break;
                                                
                                                case 0x5902: {
                                                    // Key signature
                                                    
                                                    // >0 is sharps, <0 is flats.
                                                    s8 alterations = *event++;
                                                    bool minor     = *event++;
                                                } break;
                                                
                                                default: UNHANDLED;
                                            }
                                        } break;
                                    }
                                } break;
                                
                                case 0xF0: {
                                    // System event.
                                    s32 data_length = midi_decodemod_and_advance(&event);
                                    event += data_length;
                                } break;
                                
                                case 0xF7: {
                                    // System event but 0xF0 isn't sent to the MIDI stream.
                                    s32 data_length = midi_decodemod_and_advance(&event);
                                    event += data_length;
                                } break;
                                
                                default: {
                                    // MIDI channel status.
                                    channel_status = maybe_status;
                                }
                            }
                        } else {
                            // maybe_status was a data byte (running status, so the status was omitted),
                            // so backtrack the pointer one byte.
                            // This should only happen when receiving MIDI channel messages!
                            ASSERT(channel_status);
                            
                            event -= 1;
                        }
                        
                        if(channel_status) {
                            // Read MIDI channel messages.
                            u8 first_nibble   = channel_status >> 4;
                            u8 channel        = channel_status & 0xF;
                            
                            // Channel voice messages
                            switch(first_nibble) {
                                case 8: {
                                    // Key released
                                    s8 key      = *event++;
                                    // 0x40 for velocity-agnostic instruments:
                                    s8 velocity = *event++;
                                    
                                    auto note = &note_state[key];
                                    ASSERT(note->duration == -1);
                                    note->duration = current_time - note->start;
                                    
                                    track_notes[track_note_count & track_note_buffer_mask] = *note;
                                    track_note_count += 1;
                                    // Flush the buffer if needed.
                                    if(!(track_note_count & track_note_buffer_mask)) { // We wrapped around.
                                        append_bytes(&out_file_builder, CAST(u8*, track_notes), sizeof(Synth_Input_Note) * track_note_buffer_length);
                                    }
                                } break;
                                
                                case 9: {
                                    // Key pressed
                                    s8 key      = *event++;
                                    // If velocity is 0, then this is equivalent to a key release with velocity 40.
                                    s8 velocity = *event++;
                                    
                                    auto note = &note_state[key];
                                    note->key        = key;
                                    note->start      = current_time;
                                    note->duration   = -1;
                                    note->velocity   = velocity;
                                } break;
                                
                                case 10: {
                                    // Aftertouch
                                    s8 key      = *event++;
                                    s8 pressure = *event++;
                                } break;
                                
                                case 11: {
                                    // Controller change
                                    s8 controller = *event++;
                                    s8 value      = *event++;
                                    
                                    // Channel Mode messages
                                    if(controller == 0x78) {
                                        // Global mute
                                    } else if(controller == 0x79) {
                                        // Reset controllers
                                    } else if(controller == 0x7A) {
                                        // Toggle keyboard connection
                                        if(!value) {
                                            // Disconnect
                                        } else {
                                            ASSERT(value == 0x7F);
                                        }
                                    } else if(controller == 0x7B) {
                                        // Global key release
                                        // Ignore is Omni is on
                                    } else if(controller == 0x7C) {
                                        // Omni off:
                                        // Only receive channel voice for the basic channel
                                    } else if(controller == 0x7D) {
                                        // Omni on:
                                        // Receive all channel voice messages
                                    } else if(controller == 0x7E) {
                                        // Monophonic:
                                        // If omni, read additional byte:
                                        u8 midi_channels_to_use = value;
                                        ASSERT(midi_channels_to_use <= 16);
                                    } else {
                                        // Controller change (Channel Voice message)
                                        // Center for value is 0x40
                                    }
                                } break;
                                
                                case 12: {
                                    // Program change, ie. instrument change
                                    s8 program = *event++;
                                } break;
                                
                                case 13: {
                                    // Channel-wide aftertouch
                                    s8 pressure = *event++;
                                } break;
                                
                                case 14: {
                                    // Pitch bend
                                    
                                    // What are these values even?
                                    s8 lsb = *event++; // Center is 0
                                    s8 msb = *event++; // Center is 0x40
                                } break;
                            }
                        }
                    }
                    midi_parse_end:;
                    
                    if(track_note_count) {
                        track->instrument = tracks_with_notes++; // Right now we don't really have a good way to identify instruments.
                        track->note_count = track_note_count;
                        
                        if(track_note_count & track_note_buffer_mask) {
                            append_bytes(&out_file_builder, CAST(u8*, track_notes), sizeof(Synth_Input_Note) * (track_note_count & track_note_buffer_mask));
                        }
                    } else {
                        // Cancel writing out this track.
                        // We know for sure we haven't written anything else in the meantime since we only write notes.
                        out_file_builder.written -= sizeof(Synth_Input_Track);
                    }
                }
                
                out_header->track_count = tracks_with_notes;
                
                write_entire_file(out_file, extract(&out_file_builder));
            }
        }
        
#if 0
        { // Synth1 config files -> Synth_Instrument
            SCOPE_MEMORY(&memory);
            string sy1_filename = STRING("instruments/001.sy");
            string sy1_file = read_entire_file(sy1_filename);
            
            s32 extracted_values[99] = {};
            
            u8 *at = sy1_file.data;
            while((at - sy1_file.data) < CAST(ssize, sy1_file.length)) {
                s32 key = ascii_to_s32(CAST(const char **, &at));
                
                ASSERT(*at == ',');
                at += 1;
                
                s32 value = ascii_to_s32(CAST(const char **, &at));
                
                ASSERT(*at == '\n');
                at += 1;
                
                extracted_values[key] = value;
            }
            
            using namespace SYNTH1;
            
            Synth_Input_Instrument instrument = {};
            {
                f32 mix = CAST(f32, extracted_values[OSC_MIX]) / 127.0f;
                instrument.oscillators.mix[0] = 1.0f - mix;
                instrument.oscillators.mix[1] = mix;
            }
            
            {
                u8 waveform = CAST(u8, extracted_values[OSC1_WAVEFORM]);
                
                switch(waveform) {
                    case 0: {
                        waveform = SYNTH_WAVEFORM::SINE;
                    } break;
                    case 1: {
                        waveform = SYNTH_WAVEFORM::SAWTOOTH;
                    } break;
                    case 2: {
                        waveform = SYNTH_WAVEFORM::PULSE;
                    } break;
                    case 3: {
                        waveform = SYNTH_WAVEFORM::TRIANGLE;
                    } break;
                    
                    default: UNHANDLED;
                }
                
                instrument.oscillators.waveforms[0] = waveform;
            }
            
            {
                u8 waveform = CAST(u8, extracted_values[OSC2_WAVEFORM]);
                
                switch(waveform) {
                    case 1: {
                        waveform = SYNTH_WAVEFORM::SAWTOOTH;
                    } break;
                    case 2: {
                        waveform = SYNTH_WAVEFORM::PULSE;
                    } break;
                    case 3: {
                        waveform = SYNTH_WAVEFORM::TRIANGLE;
                    } break;
#if 0 // @Incomplete
                    case 4: {
                        waveform = SYNTH_WAVEFORM::NOISE;
                    } break;
#endif
                    
                    default: UNHANDLED;
                }
                
                instrument.oscillators.waveforms[1] = waveform;
            }
            
            instrument.oscillators.count = 2;
            if(!instrument.oscillators.mix[0]) {
                instrument.oscillators.mix[0] = 1.0f;
                instrument.oscillators.waveforms[0] = instrument.oscillators.waveforms[1];
                
                instrument.oscillators.mix[1] = 0.0f;
            }
            
            if(!instrument.oscillators.mix[1]) {
                instrument.oscillators.count = 1;
            }
            
            {
                s32 attack = extracted_values[AMP_ATTACK];
                instrument.attack = attack_f;
            }
            
        }
#endif
        
#if 0
        // Compression benchmark.
        {
            string comp_names[] = {
                STRING("silesia/dickens"),
                STRING("silesia/mozilla"),
                STRING("silesia/mr"),
                STRING("silesia/nci"),
                STRING("silesia/ooffice"),
                STRING("silesia/osdb"),
                STRING("silesia/reymont"),
                STRING("silesia/samba"),
                STRING("silesia/sao"),
                STRING("silesia/webster"),
                STRING("silesia/xml"),
                STRING("silesia/x-ray"),
            };
            
            string uncompressed_files[ARRAY_LENGTH(comp_names)] = {};
            string compressed_files[ARRAY_LENGTH(comp_names)] = {};
            u64 decode_cycles[ARRAY_LENGTH(comp_names)] = {};
            
            for(u32 i = 0; i < ARRAY_LENGTH(comp_names); ++i) {
                string uncompressed = os_platform.read_entire_file(comp_names[i]);
                string compressed = fastcomp_compress(uncompressed, game_block);
                uncompressed_files[i] = uncompressed;
                compressed_files[i] = compressed;
            }
            
            u8 *decode_buffer = push_array(game_block, u8, KiB(256));
            for(u32 i = 0; i < ARRAY_LENGTH(comp_names); ++i) {
                string compressed = compressed_files[i];
                u64 tstart = READ_CPU_CLOCK();
                fastcomp_decompress(compressed, decode_buffer);
                u64 tend = READ_CPU_CLOCK();
                u64 dcy = tend - tstart;
                decode_cycles[i] = dcy;
            }
            
            u8 print_buffer[512];
            string prt;
            prt.data = print_buffer;
            for(u32 i = 0; i < ARRAY_LENGTH(comp_names); ++i) {
                prt.length = print(print_buffer, 512, "%s : %u -> %u, %ucy\n",
                                   comp_names[i].data, uncompressed_files[i].length,
                                   compressed_files[i].length, decode_cycles[i]);
                os_platform.print(prt);
            }
        }
        
#endif
        
#if 0 // :ReenableGraphics
        { // Texture files
            // Input data needed for a file:
            string bmp_names[TEXTURE_UID_COUNT] = {
                STRING("textures/blank.bmp"),
                STRING("textures/untextured.bmp"),
                STRING("textures/circle.bmp"),
                STRING("textures/default_hitbox.bmp"),
                STRING("textures/default_hurtbox.bmp"),
                STRING("textures/default_solid.bmp"),
                STRING("textures/player.bmp"),
                STRING("textures/xhair.bmp"),
                STRING("textures/grid_tile.bmp"),
            };
            // Debug textures should have a scale of 1.0f so as to preserve input dimensions.
            v2 bmp_halfdims[TEXTURE_UID_COUNT] = {
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(1.0f),
                V2(0.02f * INV_GAME_ASPECT_RATIO, 0.02f),
                V2(1.0f),
            };
            // We specify these here as 0-1 and then prebake the multiplication by halfdim.
            v2 bmp_offsets[TEXTURE_UID_COUNT] = {
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
                V2(),
            };
            const u32 bmp_count = TEXTURE_UID_COUNT;
            for(s32 i = 0; i < bmp_count; ++i) {
                bmp_offsets[i] = v2_hadamard_prod(bmp_offsets[i], bmp_halfdims[i]);
            }
            string ta = files_convert_bmp_to_ta(bmp_names, bmp_halfdims, bmp_offsets, bmp_count, &memory);
            data_files[data_file_count++] = ta;
        }
#endif
        
#if 0
        
        { // Levels
            { // 1
                usize allocated_size = KiB(4);
                string output = push_string(&memory, allocated_size);
                
                v2 map_halfdim = V2(TEST_MAP_WIDTH, TEST_MAP_HEIGHT);
                
                const s32 player_count = 1;
                
                v2 player_ps[player_count] = {
                    V2(),
                };
                s32 player_partner_counts[player_count] = {
                    3,
                };
                
                const s32 partner_count = 4;
                
                s32 player_partners[player_count][partner_count] = {
                    {
                        0, 1, 2, 3,
                    },
                };
                v2 partner_offsets[partner_count] = {
                    V2(-1.0f, 0.0f),
                    V2(0.0f, -1.0f),
                    V2(1.0f, 0.0f),
                    V2(0.0f, 1.0f),
                };
                
                const s32 turret_count = 1;
                
                v2 turret_ps[turret_count] = {
                    V2(0.0f, 2.0f),
                };
                f32 turret_cooldowns[turret_count] = {
                    1.0f,
                };
                
                
                const s32 dog_count = 1;
                
                v2 dog_ps[dog_count] = {
                    V2(2.0f),
                };
                
                const s32 wall_turret_count = 1;
                
                v2 wall_turret_ps[wall_turret_count] = {
                    V2(-1.0f),
                };
                
                /* @Cleanup: Do we care about trees?
                const s32 tree_count = 1;
                
                v2 
                */
                
                const s32 target_count = 1;
                
                v2 target_ps[target_count] = {
                    V2(-3.0f, 2.2f),
                };
                
                Serialised_Level_Data level;
                
                // @Cleanup: Use layout_serialised_level_data()
                
                level.header = (Level_Header *)output.data;
                
                level.player_ps = (v2 *)(level.header + 1);
                level.partner_offsets = (v2 *)(level.player_ps + player_count);
                level.turret_ps = (v2 *)(level.partner_offsets + partner_count);
                level.turret_cooldowns = (f32 *)(level.turret_ps + turret_count);
                level.dog_ps = (v2 *)(level.turret_cooldowns + turret_count);
                level.wall_turret_ps = (v2 *)(level.dog_ps + dog_count);
                // @Cleanup: Do we care about trees?
                level.target_ps = (v2 *)(level.wall_turret_ps + wall_turret_count);
                
                u8 *end_of_file = (u8 *)(level.target_ps + target_count);
                usize file_size = end_of_file - output.data;
                ASSERT(file_size < allocated_size);
                
                level.header->version = 1;
                level.header->index = 1;
                level.header->map_halfdim = map_halfdim;
                level.header->player_count = player_count;
                level.header->partner_count = partner_count;
                level.header->turret_count = turret_count;
                level.header->dog_count = dog_count;
                level.header->wall_turret_count = wall_turret_count;
                level.header->target_count = target_count;
                
                for(s32 i = 0; i < player_count; ++i) {
                    s32 this_partner_count = player_partner_counts[i];
                    
                    level.player_ps[i] = player_ps[i];
                }
                for(s32 i = 0; i < partner_count; ++i) {
                    level.partner_offsets[i] = partner_offsets[i];
                }
                for(s32 i = 0; i < turret_count; ++i) {
                    level.turret_ps[i] = turret_ps[i];
                    level.turret_cooldowns[i] = turret_cooldowns[i];
                }
                for(s32 i = 0; i < dog_count; ++i) {
                    level.dog_ps[i] = dog_ps[i];
                }
                FORI(0, wall_turret_count) {
                    level.wall_turret_ps[i] = wall_turret_ps[i];
                }
                for(s32 i = 0; i < target_count; ++i) {
                    level.target_ps[i] = target_ps[i];
                }
                
                data_files[data_file_count++] = output;
            }
        }
#endif
        
#if 0
        
        { // Writing a datapack:
            string pack_name = STRING("1.datapack");
            ASSERT(data_file_count == ASSET_UID_COUNT);
            usize file_entries = data_file_count + 1;
            usize write_offset = sizeof(Datapack_Header) + sizeof(Datapack_Asset_Info) * file_entries;
            Datapack_Header header;
            header.version = DATAPACK_VERSION;
            usize data_files_total_size = 0;
            
            for(usize i = 0; i < data_file_count; ++i) {
                data_files_total_size += data_files[i].length;
            }
            
            string pack_file = push_string(&memory, data_files_total_size + write_offset);
            Datapack_Asset_Info *asset_table = (Datapack_Asset_Info *)(((Datapack_Header *)pack_file.data) + 1);
            
            for(usize i = 0; i < data_file_count; ++i) {
                string source = data_files[i];
                ASSERT(source.data && source.length);
                
                Datapack_Asset_Info entry = {};
                // @Cleanup: We technically support more than 32-bit offsets.
                ASSERT(write_offset < U32_MAX);
                entry.location_low = (u32)write_offset;
                entry.compression = 0;
                asset_table[i] = entry;
                
                mem_copy(source.data, pack_file.data + write_offset, source.length);
                
                write_offset += source.length;
            }
            
            Datapack_Asset_Info last_entry;
            last_entry.all = write_offset;
            asset_table[data_file_count] = last_entry;
            
            *(Datapack_Header *)pack_file.data = header;
            ASSERT(write_offset == data_files_total_size + sizeof(Datapack_Header) + sizeof(Datapack_Asset_Info) * file_entries);
            
            write_entire_file(pack_name, pack_file);
        }
#endif
    }
    
    return 0;
}
