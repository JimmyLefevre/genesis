
//
// WAV
//

static Sound_Asset load_wav(string name) {
    Sound_Asset result = {};
    s16 *read_data = 0;
    string loaded = os_platform.read_entire_file( name );
    
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

//
// Ogg/Vorbis
//

static inline s32 vorbis_ilog(u64 a) {
    s32 result = 0;
    if(CAST(s64, a) > 0) {
        result += 1;
        a>>= 1;
    }
    return result;
}

static inline f32 vorbis_unpack_f32(u32 a) {
    s32 mantissa =  a & 0x01FFFFF;
    u32 sign     =  a & 0x8000000;
    u32 exponent = (a & 0x7FE0000) >> 21;
    mantissa ^= sign;
    u32 result = mantissa * (1 << (exponent - 788));
    return *CAST(f32 *, &result);
}

// @Speed!!!
static u32 vorbis_codebook_scalar_lookup(Vorbis_Codebook *codebook, bitreader *reader) {
    u32 bits = 0;
    u32 bit_count = 0;
    FORI_NAMED(j, 0, 32) {
        u32 bit = read_bits(reader, 1).u;
        bits |= bit;
        bit_count += 1;
        
        FORI_TYPED(u32, 0, codebook->entry_count) {
            if((codebook->huffman[i] == bits) && (codebook->huffman_lengths[i] == bit_count)) {
                return i;
            }
        }
        
        bits <<= 1;
    }
    
    UNHANDLED;
    return 0;
}

//
// MIDI
//

static inline s32 midi_decodemod_and_advance(u8 **at) {
    MIRROR_VARIABLE(u8 *, _at, at);
    s32 result = *_at;
    while(*_at++ & (1 << 7)) {
        result = (result << 7) | *_at;
    }
    
    return result;
}

//
// Internal assets
//
#if USE_DATAPACK
static inline string read_entire_asset(Memory_Block *block, Datapack_Handle *pack, s32 asset_uid) {
    Asset_Metadata metadata = get_asset_metadata(pack, asset_uid);
    string result = push_string(block, metadata.size);
    os_platform.read_file(pack->file_handle, result.data, metadata.location.offset, result.length);
    
    return result;
}
#define READ_ENTIRE_ASSET(block, pack, asset, category) read_entire_asset(block, pack, ASSET_UID_##asset##_##category)
#else
static inline string read_entire_asset(Memory_Block *block, Datapack_Handle *pack, string name) {
    string result = os_platform.read_entire_file(name);
    
    return result;
}
#define READ_ENTIRE_ASSET(block, pack, asset, category) read_entire_asset(block, pack, STRING(#category"/"#asset"."#category))
#endif