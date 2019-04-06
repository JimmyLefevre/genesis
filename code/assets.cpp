
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

static inline string read_entire_asset(Memory_Block *block, Datapack_Handle *pack, s32 asset_uid) {
    Asset_Metadata metadata = get_asset_metadata(pack, asset_uid);
    string result = push_string(block, metadata.size);
    os_platform.read_file(pack->file_handle, result.data, metadata.location.offset, result.length);
    
    return result;
}
