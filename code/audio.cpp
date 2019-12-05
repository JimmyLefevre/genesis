
static void audio_init(Audio_Info *audio_info, Memory_Block *memory_block, s32 output_hz, s32 output_channels, s32 core_count) {
    // This should not necessarily be the total number of cores on the machine. Rather, it
    // should be the number of threads we want working on audio jobs at the same time.
    audio_info->core_count = core_count;
    
    // Processing buffers:
    audio_info->temp_buffer = push_array(memory_block, f32  , output_hz      );
    audio_info->mix_buffers = push_array(memory_block, f32 *, output_channels);
    for(s32 i = 0; i < output_channels; ++i) {
        audio_info->mix_buffers[i] = push_array(memory_block, f32, output_hz);
    }
    audio_info->out_buffer = push_array(memory_block, s16, output_hz * output_channels);
    audio_info->channel_count = (u8)output_channels;
    init(&audio_info->loads, memory_block);
    
    // Chunk buffers:
    Audio_Page_Identifier bad_id;
    bad_id.all = 0xFFFFFFFFFFFFFFFF;
    for(s32 i = 0; i < MAX_SOUND_COUNT * 2; ++i) {
        audio_info->audio_pages[i] = push_array(memory_block, s16, AUDIO_PAGE_SIZE + 1);
        audio_info->audio_page_ids[i] = bad_id;
    }
    
    { // Threaded read bitfield:
        Audio_Load_Queue **queues = push_array(memory_block, Audio_Load_Queue *, core_count);
        FORI(0, core_count) {
            queues[i] = push_struct(memory_block, Audio_Load_Queue, CACHE_LINE_SIZE);
        }
        audio_info->load_queues = queues;
        push_size(memory_block, 0, 64);
        
        audio_info->last_gather_first_frees = push_array(memory_block, s32, core_count);
    }
    
    // We're assuming current_volume was initialised when loading the config.
    audio_info->target_volume.master                            = audio_info->current_volume.master;
    audio_info->target_volume.by_category[AUDIO_CATEGORY_SFX  ] = audio_info->current_volume.by_category[AUDIO_CATEGORY_SFX  ];
    audio_info->target_volume.by_category[AUDIO_CATEGORY_MUSIC] = audio_info->current_volume.by_category[AUDIO_CATEGORY_MUSIC];
    
    // When we get to unloading sounds, be sure to reset these values to -1.
    FORI(0, SOUND_UID_COUNT) {
        audio_info->sounds_by_uid[i] = -1;
    }
    FORI(0, PLAYING_SOUND_HANDLE_BITFIELD_SIZE) {
        audio_info->free_commands[i] = 0xFFFFFFFFFFFFFFFF;
    }
    FORI(0, MAX_SOUND_COUNT) {
        audio_info->play_commands[i].old_volume = push_array(memory_block, f32, output_channels);
        audio_info->play_commands[i].volume = push_array(memory_block, f32, output_channels);
    }
}

// This function does not have to return 0 for vol = 0.
// Instead, silent sounds should be flagged.
static inline f32 linear_to_exp(f32 vol) {
#if 0
    return 0.001f * f_exp(6.90775527898213705205f * vol);
#else
    
    // NOTE(keeba): While the curve above was supposed to be more accurate, it's too quiet.
    // Meanwhile, this works on my current setup.
    return vol*vol;
#endif
}

static inline s16 hash_audio_page(s16 chunk_index, s16 channel_index, s16 load) {
    return (load << 5) ^ (channel_index << 3) ^ chunk_index;
}

static s16 find_audio_page(Audio_Info *audio, s16 chunk_index, s16 channel_index, s16 load, bool *same_id) {
    s16 hash_index = hash_audio_page(chunk_index, channel_index, load) % (MAX_SOUND_COUNT * 2);
    Audio_Page_Identifier id;
    id.chunk_index = chunk_index;
    id.channel_index = channel_index;
    id.load = load;
    for(;;) {
        Audio_Page_Identifier check = audio->audio_page_ids[hash_index];
        
        *same_id = ((id.chunk_index == check.chunk_index) &&
                    (id.channel_index == check.channel_index) &&
                    (id.load == check.load));
        
        if(*same_id) {
            return hash_index;
        } else if(!audio->audio_page_uses[hash_index]) {
            audio->audio_page_ids[hash_index] = id;
            return hash_index;
        } else {
            hash_index = (hash_index + 1) % (MAX_SOUND_COUNT * 2);
            ASSERT(hash_index != (hash_audio_page(chunk_index, channel_index, load) % (MAX_SOUND_COUNT * 2)));
        }
    }
}

static s16 find_existing_audio_page(Audio_Info *audio, s16 chunk_index, s16 channel_index, s16 load) {
    s16 hash_index = hash_audio_page(chunk_index, channel_index, load) % (MAX_SOUND_COUNT * 2);
    Audio_Page_Identifier id;
    id.chunk_index = chunk_index;
    id.channel_index = channel_index;
    id.load = load;
    for(;;) {
        Audio_Page_Identifier check = audio->audio_page_ids[hash_index];
        
        if((id.chunk_index == check.chunk_index) &&
           (id.channel_index == check.channel_index) &&
           (id.load == check.load)) {
            return hash_index;
        } else {
            hash_index = (hash_index + 1) % (MAX_SOUND_COUNT * 2);
            ASSERT(hash_index != (hash_audio_page(chunk_index, channel_index, load) % (MAX_SOUND_COUNT * 2)));
        }
    }
}

void Implicit_Context::load_chunk(Loaded_Sound *loaded, s16 *samples, s32 chunk_index, s32 channel_index) { SCOPE_MEMORY(&temporary_memory);
    u64      chunk_offset = loaded->chunk_offsets_in_file[chunk_index * loaded->channel_count + channel_index];
    u64 next_chunk_offset = loaded->chunk_offsets_in_file[chunk_index * loaded->channel_count + channel_index + 1];
    u8 *scratch = push_array(&temporary_memory, u8, next_chunk_offset - chunk_offset);
    
    os_platform.read_file(loaded->streaming_data.file_handle, scratch,
                          loaded->streaming_data.offset + chunk_offset,
                          next_chunk_offset - chunk_offset);
    
    { // Decoding
        bitreader reader = make_bitreader(scratch);
        u32 decoded_samples = 0;
        u32 last_samples = 0;
        while(decoded_samples < SAMPLES_PER_CHUNK) {
            s32 run_length      = read_bits(&reader, UVGA_CODING_CHOICE_RUN_LENGTH_BITS).s + 1;
            s32 bits_per_sample = read_bits(&reader, UVGA_CODING_CHOICE_BITS_PER_SAMPLE_BITS).s;
            
            
            s32 bias = (1 << bits_per_sample);
            u32 clamp_mask = bias - 1;
            u32 sign_shift = 16 - bits_per_sample;
            FORI_TYPED(s32, 0, run_length) {
                u32 bits = read_bits(&reader, bits_per_sample).u;
                
                s16 grad = CAST(s16, (bias + bits) & clamp_mask);
                {
                    s16 temp_shift = grad << sign_shift;
                    s16 sign_extended = temp_shift >> sign_shift;
                    grad = sign_extended;
                }
                
                s16 last_sample0 = last_samples & 0xFFFF;
                s16 last_sample1 = CAST(s16, last_samples >> 16);
                
                s16 sample = CAST(s16, grad + last_sample0 * 2 - last_sample1);
                
                samples[decoded_samples + i] = sample;
                last_samples = sll(last_samples, 16) | sample;
            }
            
            decoded_samples += run_length;
            ASSERT(decoded_samples <= SAMPLES_PER_CHUNK);
        }
    }
}

#define UNPACK_AUDIO_CHUNK_JOB_VARIABLES \
Load_Audio_Chunk_Payload *payload = (Load_Audio_Chunk_Payload *)data; \
Audio_Info *audio = payload->audio; \
Loaded_Sound *loaded = payload->loaded; \
s16 page = payload->page
THREAD_JOB_PROC(load_unbatched_first_audio_chunk_job) {
    TIME_BLOCK;
    UNPACK_AUDIO_CHUNK_JOB_VARIABLES;
    Audio_Load_Queue *queue = audio->load_queues[thread_index];
    s32 first_free = queue->first_free;
    
    load_chunk(loaded, audio->audio_pages[page], payload->chunk_index, payload->channel_index);
    
    // We're a mono sound; just queue it.
    queue->buffered[first_free] = page;
    WRITE_FENCE;
    queue->first_free = first_free + 1;
}
THREAD_JOB_PROC(load_batched_first_audio_chunk_job) {
    TIME_BLOCK;
    UNPACK_AUDIO_CHUNK_JOB_VARIABLES;
    s16 sync = payload->sync;
    Audio_Load_Queue *queue = audio->load_queues[thread_index];
    s32 first_free = queue->first_free;
    
    load_chunk(loaded, audio->audio_pages[page], payload->chunk_index, payload->channel_index);
    
    s32 other_sounds_to_load = ATOMIC_INC32(CAST(volatile long*, audio->load_syncs + sync));
    if(!other_sounds_to_load) {
        // The whole batch is ready; queue it.
        auto *batch = audio->load_batches + sync;
        
        FORI(0, batch->count) {
            s16 other = batch->buffered[i];
            s32 index = (first_free + i) % MAX_SOUND_COUNT;
            
            queue->buffered[index] = other;
        }
        WRITE_FENCE;
        queue->first_free = first_free + batch->count;
    }
}
THREAD_JOB_PROC(load_unbatched_next_audio_chunk_job) {
    TIME_BLOCK;
    UNPACK_AUDIO_CHUNK_JOB_VARIABLES;
    // We may want to zero_mem the samples first, just in case we don't manage to load in time.
    load_chunk(loaded, audio->audio_pages[page], payload->chunk_index, payload->channel_index);
}
#undef UNPACK_AUDIO_CHUNK_JOB_VARIABLES

static inline void load_sound_info(Loaded_Sound *loaded, Asset_Location *location, UVGA_Header *header, u8 category) {
    ASSERT((header->sample_rate == 44100) &&
           (header->samples_per_chunk == SAMPLES_PER_CHUNK) &&
           (header->bytes_per_sample == 2));
    
    // :UVGARefactor
    // We need to load the file's metadata; in this case, it's only the header and the chunk offsets.
    // Since their size is not known ahead of time, it means we need to actually have some memory
    // allocations in the audio mixer.
    // :DatapackRefactor
    // One way to avoid this is to load all the asset metadata on startup.
    
    loaded->streaming_data = *location;
    loaded->chunks_per_channel = header->chunks_per_channel;
    loaded->channel_count = header->channel_count;
}

static s16 _load_music(Audio_Info *audio, Datapack_Handle *pack, u32 asset_uid, u32 sound_uid) {
    ASSERT(sound_uid != SOUND_UID_unloaded);
    Asset_Location location = get_asset_location(pack, asset_uid);
    
    UVGA_Header header;
    os_platform.read_file(pack->file_handle, &header, location.offset, sizeof(UVGA_Header));
    
    Loaded_Sound loaded = {};
    { // :DatapackRefactor @Duplication We load all of the chunk offsets on startup.
        s32 chunk_count = header.chunks_per_channel * header.channel_count;
        s32 offsets_to_load = chunk_count + 1;
        ASSERT((audio->used_chunk_offsets + chunk_count) < (MAX_SOUND_COUNT * 4096));
        
        loaded.chunk_offsets_in_file = audio->chunk_offset_pool + audio->used_chunk_offsets;
        os_platform.read_file(pack->file_handle, loaded.chunk_offsets_in_file, location.offset + sizeof(UVGA_Header), sizeof(u64) * offsets_to_load);
        
        audio->used_chunk_offsets += offsets_to_load;
    }
    load_sound_info(&loaded, &location, &header, AUDIO_CATEGORY_MUSIC);
    
    Loaded_Sound *added = add(&audio->loads, &loaded);
    
    s16 result = (s16)(added - audio->loads.items);
    audio->sounds_by_uid[sound_uid] = result;
    
    return result;
}
#define LOAD_MUSIC(audio_info, datapack, tag) _load_music(audio_info, datapack, ASSET_UID_##tag##_stereo_uvga, SOUND_UID_##tag)

static s16 _load_sound(Audio_Info *audio, Datapack_Handle *pack, u32 asset_uid, u32 sound_uid) {
    ASSERT(sound_uid != SOUND_UID_unloaded);
    Asset_Location location = get_asset_location(pack, asset_uid);
    
    UVGA_Header header;
    os_platform.read_file(pack->file_handle, &header, location.offset, sizeof(UVGA_Header));
    
    // Sounds are mono.
    ASSERT(header.channel_count == 1);
    Loaded_Sound loaded = {};
    { // :DatapackRefactor @Duplication We load all of the chunk offsets on startup.
        s32 chunk_count = header.chunks_per_channel * header.channel_count;
        ASSERT((audio->used_chunk_offsets + chunk_count) < (MAX_SOUND_COUNT * 4096));
        loaded.chunk_offsets_in_file = audio->chunk_offset_pool + audio->used_chunk_offsets;
        audio->used_chunk_offsets += chunk_count;
        os_platform.read_file(pack->file_handle, &header, location.offset + sizeof(UVGA_Header), sizeof(u64) * chunk_count);
    }
    load_sound_info(&loaded, &location, &header, AUDIO_CATEGORY_SFX);
    
    Loaded_Sound *added = add(&audio->loads, &loaded);
    
    s16 result = (s16)(added - audio->loads.items);
    audio->sounds_by_uid[sound_uid] = result;
    
    return result;
}
#define LOAD_SOUND(audio_info, datapack, tag) _load_sound(audio_info, datapack, ASSET_UID_##tag##_facs, SOUND_UID_##tag)

static inline s16 _find_sound_id(Audio_Info *audio, u32 sound_uid) {
    return audio->sounds_by_uid[sound_uid];
}
#define FIND_SOUND_ID(audio_info, tag) _find_sound_id(audio_info, SOUND_UID_##tag)

static s32 find_first_free_and_unset_bit(u64 *bitfields, ssize count) {
    s32 result = -1;
    
    FORI_TYPED(s32, 0, count) {
        u64 field = bitfields[i];
        if(bitscan_forward(field, &result)) {
            field &= ~(1 << result);
            bitfields[i] = field;
            
            result += i * 64;
            break;
        }
    }
    
    return result;
}

static void _play_sound(Audio_Info *audio, s16 loaded_id, s32 channel_index, s16 handle, s16 sync) {
    ASSERT(loaded_id != -1);
    Loaded_Sound *loaded = audio->loads.items + loaded_id;
    s32 chunk_count = loaded->chunks_per_channel;
    bool same_id0 = false;
    bool same_id1 = false;
    
    u32 found0 = find_audio_page(audio, 0, (s16)channel_index, loaded_id, &same_id0);
    audio->audio_page_uses[found0] += 1;
    u32 found1 = 0xFFFF;
    if(chunk_count > 1) {
        found1 = find_audio_page(audio, 1, (s16)channel_index, loaded_id, &same_id1);
        audio->audio_page_uses[found1] += 1;
    }
    u32 pages = (found0 << 16) | found1;
    
    Audio_Buffered_Play buffered;
    buffered.load = loaded_id;
    buffered.pages = pages;
    buffered.channel_index = (s8)channel_index;
    buffered.commands = handle;
    
    audio->buffered_plays[handle] = buffered;
    
    Load_Audio_Chunk_Payload payload;
    payload.loaded = loaded;
    payload.audio = audio;
    payload.channel_index = (s8)channel_index;
    
    if(!same_id0) {
        payload.chunk_index = 0;
        payload.page = (s16)found0;
        payload.sync = (s8)sync;
        audio->load_payloads[found0] = payload;
        
        if(sync < MAX_MUSIC_COUNT) {
            os_platform.add_thread_job(&Implicit_Context::load_batched_first_audio_chunk_job, (void *)(audio->load_payloads + found0));
        } else {
            os_platform.add_thread_job(&Implicit_Context::load_unbatched_first_audio_chunk_job, (void *)(audio->load_payloads + found0));
        }
    } else {
        Audio_Load_Queue *queue = audio->load_queues[0];
        queue->buffered[queue->first_free % MAX_SOUND_COUNT] = (s16)found0;
        queue->first_free = queue->first_free + 1;
    }
    
    if((!same_id1) && (chunk_count > 1)) {
        payload.chunk_index = 1;
        payload.page = (s16)found1;
        payload.sync = MAX_MUSIC_COUNT;
        audio->load_payloads[found1] = payload;
        
        os_platform.add_thread_job(&Implicit_Context::load_unbatched_next_audio_chunk_job, (void *)(audio->load_payloads + found1));
    }
}

static Audio_Play_Commands *play_sound(Audio_Info *audio, s16 loaded_id) {
    if(loaded_id != -1) {
        s16 free_index = (s16)find_first_free_and_unset_bit(audio->free_commands, PLAYING_SOUND_HANDLE_BITFIELD_SIZE);
        ASSERT(free_index != -1);
        
        _play_sound(audio, loaded_id, 0, free_index, MAX_MUSIC_COUNT);
        
        Audio_Play_Commands *commands = audio->play_commands + free_index;
        commands->pitch = 1.0f;
        commands->category = AUDIO_CATEGORY_SFX;
        
        return commands;
    }
    return 0;
}

static Audio_Music_Commands *play_music(Audio_Info *audio, s16 loaded_id) {
    if(loaded_id != -1) {
        Audio_Music_Commands music;
        Loaded_Sound *loaded = audio->loads.items + loaded_id;
        u8 channel_count = loaded->channel_count;
        s16 music_index = audio->music_count++;
        ASSERT(music_index < MAX_MUSIC_COUNT);
        
        audio->load_syncs[music_index] = -channel_count;
        auto *batch = audio->load_batches + music_index;
        s16 batch_count = 0;
        
        FORI_TYPED(s32, 0, channel_count) {
            s16 handle = (s16)find_first_free_and_unset_bit(audio->free_commands, PLAYING_SOUND_HANDLE_BITFIELD_SIZE);
            _play_sound(audio, loaded_id, i, handle, music_index);
            
            Audio_Play_Commands *commands = audio->play_commands + handle;
            commands->pitch = 1.0f;
            commands->category = AUDIO_CATEGORY_MUSIC;
            music.play_commands[i] = handle;
            
            batch->buffered[batch_count++] = handle;
        }
        
        batch->count = batch_count;
        music.volume = 1.0f;
        music.channel_count = channel_count;
        audio->music_commands[music_index] = music;
        
        return audio->music_commands + music_index;
    }
    return 0;
}

static inline void compute_volumes_for_position(f32 *volume, s32 channel_count, v2 position) {
    ASSERT(channel_count == 2);
    
    f32 t = (position.x + 1.0f) * 0.5f;
    t     = f_clamp(t, 0.0f, 1.0f);
    
    volume[0] = 1.0f - t;
    volume[1] =        t;
}

void Implicit_Context::mix_samples(const s16 *samples, const s32 pitched_sample_count, f32 sample_offset, const f32 pitch,
                                   f32 *pitched_samples, s32 write_offset,
                                   f32 *start_volumes, f32 *end_volumes, f32 **mix_channels, const u8 channel_count) {
    // Check if merging these two loops is better.
    // That is, compute the pitched sample and immediately write it out.
    //
    // On second thought, having this pre-pass means that pitched samples
    // end up being tightly packed together, which is probably much better
    // for SIMD.
    for(s32 isample = 0; isample < pitched_sample_count; ++isample) {
        const f32 read_cursor    = (f32)isample * pitch;
        const s32 trunc_cursor   = (s32)read_cursor;
        const f32 tlerp          = read_cursor - (f32)trunc_cursor;
        const f32 sample0        = samples[trunc_cursor];
        const f32 sample1        = samples[trunc_cursor + 1];
        
        pitched_samples[isample] = f_lerp(sample0, sample1, tlerp);
    }
    
    {
        TIME_BLOCK;
        for(s32 ichannel = 0; ichannel < channel_count; ++ichannel) {
            f32 * const mix = mix_channels[ichannel] + write_offset;
            const f32 start_volume = start_volumes[ichannel];
            const f32 end_volume = end_volumes[ichannel];
            const f32 dvolume = (end_volume - start_volume) / (f32)pitched_sample_count;
            
#if 0
            // Legible version:
            for(s32 isample = 0; isample < pitched_sample_count; ++isample) {
                const f32 tlerp = (f32)isample / (f32)pitched_sample_count;
                f32 vol = f_lerp(start_volume, end_volume, tlerp);
                vol = linear_to_exp(vol);
                
                mix[isample] += pitched_samples[isample] * vol;
            }
#else
            // SIMD version:
            __m128 vol = _mm_set_ps(start_volume                 ,
                                    start_volume +        dvolume,
                                    start_volume + 2.0f * dvolume,
                                    start_volume + 3.0f * dvolume);
            const __m128  dvol = _mm_set1_ps(dvolume * 4.0f);
            const __m128  linear_to_exp_k0 = _mm_set1_ps((f32)(1 << 23) / 0.69314718f);
            const __m128  linear_to_exp_k1 = _mm_cvtepi32_ps(_mm_set1_epi32((127 << 23) - 361007));
            const __m128  linear_to_exp_k2 = _mm_set1_ps(6.90775527898213705205f);
            const __m128  linear_to_exp_k3 = _mm_set1_ps(0.001f);
            
            const s32 wide_iters = pitched_sample_count / 4;
            const s32 wide_iter_samples = wide_iters * 4;
            const s32 remainder = pitched_sample_count - wide_iter_samples;
            
            for(s32 isample = 0; isample < wide_iter_samples; isample += 4) {
                const __m128 samps = _mm_load_ps(pitched_samples + isample);
                const __m128 buffer_samps = _mm_load_ps(mix + isample);
                
                // Volume:
                // Translation of linear_to_exp.
                const __m128 vol_exp = _mm_mul_ps(linear_to_exp_k3,
                                                  _mm_castsi128_ps(_mm_cvtps_epi32(_mm_add_ps(linear_to_exp_k1,
                                                                                              _mm_mul_ps(linear_to_exp_k0,
                                                                                                         _mm_mul_ps(linear_to_exp_k2,
                                                                                                                    vol))))));
                
                // Writing:
                _mm_store_ps(mix + isample, _mm_add_ps(_mm_mul_ps(samps, vol_exp), buffer_samps));
                
                // Incrementing:
                vol = _mm_add_ps(vol, dvol);
            }
            
            // Finishing with a masked write:
            const __m128 out_of_bounds = _mm_cmpge_ps(_mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f), _mm_set1_ps((f32)remainder));
            const __m128 last_buffer_samps = _mm_load_ps(mix + wide_iter_samples);
            const __m128 last_write = _mm_andnot_ps(out_of_bounds, _mm_load_ps(pitched_samples + wide_iter_samples));
            const __m128 vol_exp = _mm_mul_ps(linear_to_exp_k3,
                                              _mm_castsi128_ps(_mm_cvtps_epi32(_mm_add_ps(linear_to_exp_k1,
                                                                                          _mm_mul_ps(linear_to_exp_k0,
                                                                                                     _mm_mul_ps(linear_to_exp_k2,
                                                                                                                vol))))));
            _mm_store_ps(mix + wide_iter_samples, _mm_add_ps(_mm_mul_ps(last_write, vol_exp), last_buffer_samps));
#endif
        }
    }
}

s16 *Implicit_Context::update_audio(Audio_Info *audio, const s32 samples_to_play, const f32 dt) {
    TIME_BLOCK;
    SCOPE_MEMORY(&temporary_memory);
    
    const s32 core_count = audio->core_count;
    
    { // Changing master volume:
        const f32 t_for_full_shift = 0.5f;
        const f32 volume_shift = (1.0f / t_for_full_shift) * dt;
        
        for(s32 i = 0; i < AUDIO_CATEGORY_COUNT + 1; ++i) {
            f32 current = audio->current_volume.all[i];
            f32 target  = audio-> target_volume.all[i];
            
            bool snap = false;
            if(current > target) {
                current -= volume_shift;
                snap = (current < target);
            } else if(current < target) {
                current += volume_shift;
                snap = (current > target);
            }
            
            if(snap) {
                current = target;
            }
            
            audio->current_volume.all[i] = current;
        }
    }
    
    const f32 master_volume = audio->current_volume.master;
    f32 *start_volume      = push_array(&temporary_memory, f32, audio->channel_count);
    f32 *end_volume        = push_array(&temporary_memory, f32, audio->channel_count);
    const u8 output_channel_count = audio->channel_count;
    
    for(s32 i = 0; i < output_channel_count; ++i) {
        zero_mem(audio->mix_buffers[i], sizeof(s16) * AUDIO_BUFFER_SIZE);
    }
    for(s32 i = 0; i < output_channel_count; ++i) {
        zero_mem(audio->out_buffer, sizeof(s16) * AUDIO_BUFFER_SIZE);
    }
    
    { // Updating music volume.
        FORI(0, audio->music_count) {
            Audio_Music_Commands *music = audio->music_commands + i;
            f32 volume = music->volume;
            s32 channel_count = music->channel_count;
            
            switch(channel_count) {
                case 2: {
                    Audio_Play_Commands *left = audio->play_commands + music->play_commands[0];
                    Audio_Play_Commands *right = audio->play_commands + music->play_commands[1];
                    
                    switch(output_channel_count) {
                        case 2: {
                            left->volume[0] = 1.0f;
                            left->volume[1] = 0.0f;
                            
                            right->volume[0] = 0.0f;
                            right->volume[1] = 1.0f;
                        } break;
                        
                        case 6: {
                        } break;
                        
                        case 8: {
                        } break;
                        
                        default: UNHANDLED;
                    }
                } break;
                
                default: UNHANDLED;
            }
            
            music->old_volume = volume;
        }
    }
    
    
    { // Buffered load gather:
        FORI_NAMED(thread, 0, core_count) {
            Audio_Load_Queue *queue = audio->load_queues[thread];
            s32 first_free = queue->first_free;
            s32 last_first_free = audio->last_gather_first_frees[thread];
            
            FORI(last_first_free, first_free) {
                s16 ibuffered = queue->buffered[i % MAX_SOUND_COUNT];
                Audio_Buffered_Play *buffered = audio->buffered_plays + ibuffered;
                
                s16 play = audio->plays.count++;
                audio->plays.loads[play] = buffered->load;
                audio->plays.samples_played[play] = 0;
                audio->plays.sample_offset[play] = 0.0f;
                audio->plays.audio_pages[play] = buffered->pages;
                audio->plays.channel_indices[play] = buffered->channel_index;
                audio->plays.commands[play] = buffered->commands;
            }
            
            audio->last_gather_first_frees[thread] = first_free;
        }
    }
    
    f32 samples_to_play_f = (f32)samples_to_play;
    FORI_TYPED(s16, 0, audio->plays.count) {
        s16 icommands = audio->plays.commands[i];
        s32 samples_played = audio->plays.samples_played[i];
        f32 sample_offset = audio->plays.sample_offset[i];
        
        Audio_Play_Commands *commands = audio->play_commands + icommands;
        f32 pitch = commands->pitch;
        
        {
            u8 cat = commands->category;
            f32 old_master = audio->current_volume.master;
            f32 master = audio->target_volume.master;
            f32 old_category = audio->current_volume.by_category[cat];
            f32 category = audio->target_volume.by_category[cat];
            FORI_NAMED(_i, 0, output_channel_count) {
                start_volume[_i] = old_master * old_category * commands->old_volume[_i];
                end_volume[_i] = master * category * commands->volume[_i];
            }
        }
        
        s32 end_samples_played = samples_played + f_ceil_s(samples_to_play_f / pitch);
        s32 write_offset = 0;
        u32 pages = audio->plays.audio_pages[i];
        s16 *page = audio->audio_pages[pages >> 16];
        
        if((end_samples_played / AUDIO_PAGE_SIZE) > (samples_played / AUDIO_PAGE_SIZE)) {
            s32 sample_end_0 = end_samples_played - (end_samples_played % AUDIO_PAGE_SIZE);
            s32 samples_for_first_write = sample_end_0 - samples_played;
            
            mix_samples(page + (samples_played % AUDIO_PAGE_SIZE),
                        samples_for_first_write, sample_offset, pitch,
                        audio->temp_buffer, 0,
                        start_volume, end_volume, audio->mix_buffers, output_channel_count);
            
            audio->page_discards[audio->page_discard_count++] = i;
            samples_played = sample_end_0;
            write_offset = samples_for_first_write;
            page = audio->audio_pages[pages & 0xFFFF];
            sample_offset = sample_offset + f_mod((f32)(sample_end_0 - samples_played) / pitch, 1.0f);
        }
        
        mix_samples(page + (samples_played % AUDIO_PAGE_SIZE), end_samples_played - samples_played, sample_offset, pitch,
                    audio->temp_buffer, write_offset,
                    start_volume, end_volume, audio->mix_buffers, output_channel_count);
        
        audio->plays.samples_played[i] = end_samples_played;
        audio->plays.sample_offset[i] = sample_offset + f_mod(samples_to_play_f / pitch, 1.0f);
        mem_copy(commands->volume, commands->old_volume, sizeof(f32[2]));
    }
    audio->current_volume = audio->target_volume;
    
    {
        FORI(0, audio->page_discard_count) {
            s16 play = audio->page_discards[i];
            s16 load = audio->plays.loads[play];
            u32 pages = audio->plays.audio_pages[play];
            s32 samples_played = audio->plays.samples_played[play];
            Loaded_Sound *loaded = audio->loads.items + load;
            s8 channel_index = audio->plays.channel_indices[play];
            s16 chunk_index = (s16)(samples_played / AUDIO_PAGE_SIZE) - 1;
            ASSERT(chunk_index >= 0);
            
            s32 found = find_existing_audio_page(audio, chunk_index, channel_index, load);
            audio->audio_page_uses[found] -= 1;
            
            s16 next_chunk_to_load = chunk_index + 2;
            s16 next_page_to_load = 0;
            
            if(next_chunk_to_load < loaded->chunks_per_channel) {
                bool same_id;
                next_page_to_load = find_audio_page(audio, next_chunk_to_load, channel_index, load, &same_id);
                audio->audio_page_uses[next_page_to_load] += 1;
                
                if(!same_id) {
                    Load_Audio_Chunk_Payload payload;
                    payload.loaded = loaded;
                    payload.audio = audio;
                    payload.chunk_index = next_chunk_to_load;
                    payload.page = next_page_to_load;
                    payload.channel_index = channel_index;
                    audio->load_payloads[next_page_to_load] = payload;
                    os_platform.add_thread_job(&Implicit_Context::load_unbatched_next_audio_chunk_job, (void *)(audio->load_payloads + next_page_to_load));
                }
            } else if(next_chunk_to_load > loaded->chunks_per_channel) {
                s16 add = audio->play_retire_count;
                audio->plays_to_retire[add] = play;
                audio->play_retire_count += 1;
            }
            
            pages <<= 16;
            pages |= next_page_to_load;
            audio->plays.audio_pages[play] = pages;
        }
        audio->page_discard_count = 0;
    }
    
    {
        // For now, we know these will be added in ascending order, so we can just
        // loop backwards and avoid sorting. @Temporary?
        
        s32 retire_count = audio->play_retire_count;
        s16 play_count = audio->plays.count;
        for(s32 i = retire_count - 1; i >= 0; --i) {
            s16 play = audio->plays_to_retire[i];
            s32 sub = --play_count;
            s16 icommands = audio->plays.commands[play];
            Audio_Play_Commands *commands = audio->play_commands + icommands;
            
            audio->plays.loads[play] = audio->plays.loads[sub];
            audio->plays.samples_played[play] = audio->plays.samples_played[sub];
            audio->plays.sample_offset[play] = audio->plays.sample_offset[sub];
            audio->plays.audio_pages[play] = audio->plays.audio_pages[sub];
            audio->plays.channel_indices[play] = audio->plays.channel_indices[sub];
            audio->plays.commands[play] = audio->plays.commands[sub];
            
            if(commands->category == AUDIO_CATEGORY_MUSIC) {
                FORI_NAMED(imusic, 0, audio->music_count) {
                    Audio_Music_Commands *music = audio->music_commands + imusic;
                    if(music->play_commands[0] == icommands) {
                        s16 sub_music = --audio->music_count;
                        audio->music_commands[imusic] = audio->music_commands[sub_music];
                        break;
                    }
                }
            }
            
            u64 bitfield = icommands / 64;
            u64 mod = icommands % 64;
            
            audio->free_commands[bitfield] |= ((u64)1 << mod);
        }
        
        audio->plays.count = play_count;
        audio->play_retire_count = 0;
    }
    
    // @Speed: The final mixing loop is difficult to make fast because of the parametric interleaving.
    // If speed ever becomes an issue here, we could either write hardcoded interleaving loops
    // for every channel count, or we could try to ask for non-interleaved device buffers.
    s16 * const out = audio->out_buffer;
    for(s32 ichannel = 0; ichannel < output_channel_count; ++ichannel) {
        const f32 * const mix = audio->mix_buffers[ichannel];
        for(s32 isample = 0; isample < samples_to_play; ++isample) {
            f32 in = mix[isample];
            
            if(in > (f32)S16_MAX) {
                in = (f32)S16_MAX;
            } else if(in < (f32)S16_MIN) {
                in = (f32)S16_MIN;
            }
            
            out[isample * output_channel_count + ichannel] = (s16)in;
        }
    }
    
    return audio->out_buffer;
}
