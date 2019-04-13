
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

static s16 find_audio_page(Audio_Info *audio, s16 chunk_index, s16 load, bool *same_id) {
    s16 hash_index = ((load << 5) | chunk_index) % (MAX_SOUND_COUNT * 2);
    s32 id = (load << 16) | chunk_index;
    for(;;) {
        s32 check = audio->audio_page_ids[hash_index];
        
        *same_id = (check == id);
        
        if(*same_id) {
            return hash_index;
        } else if(!audio->audio_page_uses[hash_index]) {
            audio->audio_page_ids[hash_index] = id;
            return hash_index;
        } else {
            hash_index = (hash_index + 1) % (MAX_SOUND_COUNT * 2);
            ASSERT(hash_index != (((load << 5) | chunk_index) % (MAX_SOUND_COUNT * 2)));
        }
    }
}

static s16 find_existing_audio_page(Audio_Info *audio, s16 chunk_index, s16 load) {
    s16 hash_index = ((load << 5) | chunk_index) % (MAX_SOUND_COUNT * 2);
    s32 id = (load << 16) | chunk_index;
    for(;;) {
        s32 check = audio->audio_page_ids[hash_index];
        
        if(id == check) {
            return hash_index;
        } else {
            hash_index = (hash_index + 1) % (MAX_SOUND_COUNT * 2);
            ASSERT(hash_index != (((load << 5) | chunk_index) % (MAX_SOUND_COUNT * 2)));
        }
    }
}

static void load_chunk(Asset_Location location, s16 *samples, s32 chunk_index, s32 channel_count, s32 channel_index) {
    const s32 bytes_per_chunk = SAMPLES_PER_CHUNK * sizeof(s16);
    os_platform.read_file(location.file_handle, samples,
                          location.offset + sizeof(FACS_Header) +
                          bytes_per_chunk * channel_count * chunk_index +
                          bytes_per_chunk * channel_index,
                          bytes_per_chunk);
}

THREAD_JOB_PROC(load_audio_chunk_job) {
    TIME_BLOCK;
    // In the load_for_existing_sound case, we may want to zero_mem the samples first,
    // just in case we don't manage to load in time.
    Load_Audio_Chunk_Payload *payload = (Load_Audio_Chunk_Payload *)data;
    load_chunk(payload->location, payload->samples, payload->chunk_index, payload->channel_count, payload->channel_index);
    
    if(payload->completed_reads) {
        u64 bit_index = payload->read_bit_index;
        u64 bitfield = bit_index / 64;
        u64 mod = bit_index % 64;
        payload->completed_reads[thread_index][bitfield] |= ((u64)1 << mod);
    }
}

static void load_sound_info(Loaded_Sound *loaded, Asset_Location *location, FACS_Header *facs, u8 category) {
    loaded->streaming_data = *location;
    loaded->chunk_count = facs->chunk_count;
    loaded->channel_count = facs->channel_count;
}

static s16 _load_music(Audio_Info *audio, Datapack_Handle *pack, u32 asset_uid, u32 sound_uid) {
    ASSERT(sound_uid != SOUND_UID_unloaded);
    Asset_Location location = get_asset_location(pack, asset_uid);
    
    FACS_Header header;
    os_platform.read_file(pack->file_handle, &header, location.offset, sizeof(FACS_Header));
    
    Loaded_Sound loaded = {};
    load_sound_info(&loaded, &location, &header, AUDIO_CATEGORY_MUSIC);
    
    Loaded_Sound *added = add(&audio->loads, &loaded);
    
    s16 result = (s16)(added - audio->loads.items);
    audio->sounds_by_uid[sound_uid] = result;
    
    return result;
}
#define LOAD_MUSIC(audio_info, datapack, tag) _load_music(audio_info, datapack, ASSET_UID_##tag##_stereo_facs, SOUND_UID_##tag)

static s16 _load_sound(Audio_Info *audio, Datapack_Handle *pack, u32 asset_uid, u32 sound_uid) {
    ASSERT(sound_uid != SOUND_UID_unloaded);
    Asset_Location location = get_asset_location(pack, asset_uid);
    
    FACS_Header header;
    os_platform.read_file(pack->file_handle, &header, location.offset, sizeof(FACS_Header));
    
    // Sounds are mono.
    ASSERT(header.channel_count == 1);
    Loaded_Sound loaded = {};
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

static s16 find_free_playing_sound_handle(Audio_Info *audio) {
    s32 result = -1;
    
    FORI_TYPED(s16, 0, PLAYING_SOUND_HANDLE_BITFIELD_SIZE) {
        u64 bits = audio->free_commands[i];
        if(bitscan_forward(bits, &result)) {
            bits &= ~(1 << result);
            audio->free_commands[i] = bits;
            
            result += i * 64;
            break;
        }
    }
    
    return (s16)result;
}

static s32 find_first_free_and_unset_bit(u64 *bitfields, ssize count) {
    s32 result = -1;
    
    FORI_TYPED(s16, 0, count) {
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

#if 0
// Synchronous.
static void _play_sound(Audio_Info *audio, s16 loaded_id, s32 channel_index, s16 handle) {
    ASSERT(loaded_id != -1);
    Loaded_Sound *loaded = audio->loads.items + loaded_id;
    s32 play = audio->plays.count++;
    s32 chunk_count = loaded->chunk_count;
    
    bool same_id0 = false;
    bool same_id1 = false;
    u32 found0 = find_audio_page(audio, 0, loaded_id, &same_id0);
    audio->audio_page_uses[found0] += 1;
    u32 found1 = 0xFFFF;
    if(chunk_count > 1) {
        found1 = find_audio_page(audio, 1, loaded_id, &same_id1);
        audio->audio_page_uses[found1] += 1;
    }
    u32 pages = (found0 << 16) | found1;
    
    audio->plays.loads[play] = loaded_id;
    audio->plays.samples_played[play] = 0;
    audio->plays.sample_offset[play] = 0.0f;
    audio->plays.audio_pages[play] = pages;
    audio->plays.channel_indices[play] = (s8)channel_index;
    audio->plays.commands[play] = handle;
    
    if(!same_id0) {
        load_chunk(loaded->streaming_data, audio->audio_pages[found0], 0, loaded->channel_count, channel_index);
    }
    if((!same_id1) && (chunk_count > 1)) {
        load_chunk(loaded->streaming_data, audio->audio_pages[found1], 1, loaded->channel_count, channel_index);
    }
}
#else
static void _play_sound(Audio_Info *audio, s16 loaded_id, s32 channel_index, s16 handle) {
    ASSERT(loaded_id != -1);
    Loaded_Sound *loaded = audio->loads.items + loaded_id;
    s16 buffered_index = (s16)find_first_free_and_unset_bit(audio->free_buffered_plays, PLAYING_SOUND_HANDLE_BITFIELD_SIZE);
    s32 chunk_count = loaded->chunk_count;
    s16 read_index = (buffered_index * 2) / 64;
    s16 read_mod = (buffered_index * 2) % 64;
    bool same_id0 = false;
    bool same_id1 = false;
    
    u32 found0 = find_audio_page(audio, 0, loaded_id, &same_id0);
    audio->audio_page_uses[found0] += 1;
    u32 found1 = 0xFFFF;
    if(chunk_count > 1) {
        found1 = find_audio_page(audio, 1, loaded_id, &same_id1);
        audio->audio_page_uses[found1] += 1;
    }
    u32 pages = (found0 << 16) | found1;
    
    Audio_Buffered_Play buffered;
    buffered.load = loaded_id;
    buffered.pages = pages;
    buffered.channel_index = (s8)channel_index;
    buffered.commands = handle;
    
    audio->buffered_plays[buffered_index] = buffered;
    
    if(!same_id0) {
        s16 payload_index = buffered_index * 2;
        
        Load_Audio_Chunk_Payload payload;
        payload.location = loaded->streaming_data;
        payload.samples = audio->audio_pages[found0];
        payload.chunk_index = 0;
        payload.channel_count = loaded->channel_count;
        payload.completed_reads = audio->completed_reads;
        payload.read_bit_index = payload_index;
        payload.channel_index = (s8)channel_index;
        audio->load_for_new_sound_payloads[payload_index] = payload;
        
        os_platform.add_thread_job(&Implicit_Context::load_audio_chunk_job, (void *)(audio->load_for_new_sound_payloads + (payload_index)));
    } else {
        audio->completed_reads[0][read_index] |= ((u64)1 << read_mod);
    }
    if((!same_id1) && (chunk_count > 1)) {
        // @Duplication
        s16 payload_index = buffered_index * 2 + 1;
        
        Load_Audio_Chunk_Payload payload;
        payload.location = loaded->streaming_data;
        payload.samples = audio->audio_pages[found1];
        payload.chunk_index = 1;
        payload.channel_count = loaded->channel_count;
        payload.completed_reads = audio->completed_reads;
        payload.read_bit_index = payload_index;
        payload.channel_index = (s8)channel_index;
        audio->load_for_new_sound_payloads[payload_index] = payload;
        
        os_platform.add_thread_job(&Implicit_Context::load_audio_chunk_job, (void *)(audio->load_for_new_sound_payloads + (payload_index)));
    } else {
        audio->completed_reads[0][read_index] |= ((u64)1 << (read_mod + 1));
    }
}
#endif

static Audio_Play_Commands *play_sound(Audio_Info *audio, s16 loaded_id) {
    if(loaded_id != -1) {
        s16 free_index = find_free_playing_sound_handle(audio);
        ASSERT(free_index != -1);
        
        _play_sound(audio, loaded_id, 0, free_index);
        
        Audio_Play_Commands *commands = audio->play_commands + free_index;
        commands->pitch = 1.0f;
        commands->live_plays = 1;
        commands->category = AUDIO_CATEGORY_SFX;
        
        return commands;
    }
    return 0;
}

static Audio_Play_Commands *play_music(Audio_Info *audio, s16 loaded_id) {
    if(loaded_id != -1) {
        Loaded_Sound *loaded = audio->loads.items + loaded_id;
        u8 channel_count = loaded->channel_count;
        
        s16 handle = find_free_playing_sound_handle(audio);
        FORI_TYPED(s32, 0, 1) {
            _play_sound(audio, loaded_id, i, handle);
        }
        
        Audio_Play_Commands *commands = audio->play_commands + handle;
        commands->pitch = 1.0f;
        commands->live_plays = channel_count;
        commands->category = AUDIO_CATEGORY_MUSIC;
        
        return commands;
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
    const f32 * const volumes_by_category = audio->current_volume.by_category;
    f32 *start_volume      = push_array(&temporary_memory, f32, audio->channel_count);
    f32 *end_volume        = push_array(&temporary_memory, f32, audio->channel_count);
    f32 *next_start_volume = push_array(&temporary_memory, f32, audio->channel_count);
    const u8 output_channel_count = audio->channel_count;
    
    for(s32 i = 0; i < output_channel_count; ++i) {
        zero_mem(audio->mix_buffers[i], sizeof(s16) * AUDIO_BUFFER_SIZE);
    }
    for(s32 i = 0; i < output_channel_count; ++i) {
        zero_mem(audio->out_buffer, sizeof(s16) * AUDIO_BUFFER_SIZE);
    }
    
    
    { // Buffered load gather:
        FORI_NAMED(thread, 0, core_count) {
            u64 *reads = audio->completed_reads[thread];
            
            FORI(0, PAGE_LOAD_BITFIELD_SIZE) {
                if(reads[i]) {
                    audio->buffered_play_loaded_pages[i] |= reads[i];
                    reads[i] = 0;
                }
            }
        }
        
        // @Refactor
        // We might be better off checking the modified bits when gathering them in the loop above
        // instead of doing this.
        FORI_TYPED_NAMED(s16, free_bitfield, 0, PLAYING_SOUND_HANDLE_BITFIELD_SIZE) {
            u64 free = audio->free_buffered_plays[free_bitfield];
            u64 present = free ^ 0xFFFFFFFFFFFFFFFF;
            
            if(present) {
                FORI_TYPED_NAMED(s16, bit_index, 0, 64) {
                    u64 bit = (u64)1 << bit_index;
                    if(present & bit) {
                        s16 i = 64 * free_bitfield + bit_index;
                        s16 page0_bit_index = i * 2;
                        s16 page1_bit_index = i * 2 + 1;
                        
                        u64 bitfield = page0_bit_index / 64;
                        u64 page0_mod = page0_bit_index % 64;
                        u64 page1_mod = page1_bit_index % 64;
                        
                        u64 page0_bit = ((u64)1 << page0_mod);
                        u64 page1_bit = ((u64)1 << page1_mod);
                        
                        u64 loaded = audio->buffered_play_loaded_pages[bitfield];
                        
                        if((loaded & page0_bit) && (loaded & page1_bit)) {
                            Audio_Buffered_Play *buffered = audio->buffered_plays + i;
                            s16 add = audio->plays.count++;
                            
                            audio->plays.loads[add] = buffered->load;
                            audio->plays.samples_played[add] = 0;
                            audio->plays.sample_offset[add] = 0.0f;
                            audio->plays.audio_pages[add] = buffered->pages;
                            audio->plays.channel_indices[add] = buffered->channel_index;
                            audio->plays.commands[add] = buffered->commands;
                            
                            free |= bit;
                            if(free == 0xFFFFFFFFFFFFFFFF) break;
                        }
                    }
                }
            }
            
            audio->free_buffered_plays[free_bitfield] = free;
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
        
        s32 end_samples_played = samples_played + ceil_s(samples_to_play_f / pitch);
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
    }
    audio->current_volume = audio->target_volume;
    
    {
        FORI(0, audio->page_discard_count) {
            s16 play = audio->page_discards[i];
            s16 load = audio->plays.loads[play];
            u32 pages = audio->plays.audio_pages[play];
            s16 commands = audio->plays.commands[play];
            s32 samples_played = audio->plays.samples_played[play];
            Loaded_Sound *loaded = audio->loads.items + load;
            s8 channel_index = audio->plays.channel_indices[play];
            s16 chunk_index = (s16)(samples_played / AUDIO_PAGE_SIZE) - 1;
            ASSERT(chunk_index >= 0);
            
            s32 found = find_existing_audio_page(audio, chunk_index, load);
            audio->audio_page_uses[found] -= 1;
            
            s16 next_chunk_to_load = chunk_index + 2;
            s16 next_page_to_load = 0;
            
            if(next_chunk_to_load < loaded->chunk_count) {
                bool same_id;
                next_page_to_load = find_audio_page(audio, next_chunk_to_load, load, &same_id);
                audio->audio_page_uses[next_page_to_load] += 1;
                
                Load_Audio_Chunk_Payload payload;
                payload.location = loaded->streaming_data;
                payload.samples = audio->audio_pages[next_page_to_load];
                payload.chunk_index = next_chunk_to_load;
                payload.channel_count = loaded->channel_count;
                payload.completed_reads = 0;
                payload.channel_index = channel_index;
                audio->load_for_existing_sound_payloads[commands] = payload;
                os_platform.add_thread_job(&Implicit_Context::load_audio_chunk_job, (void *)(audio->load_for_existing_sound_payloads + commands));
                
                // load_chunk(loaded->streaming_data, audio->audio_pages[next_page_to_load], next_chunk_to_load, loaded->channel_count, channel_index);
            } else if(next_chunk_to_load > loaded->chunk_count) {
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
            
            commands->live_plays -= 1;
            if(!commands->live_plays) {
                u64 bitfield = icommands / 64;
                u64 mod = icommands % 64;
                
                audio->free_commands[bitfield] |= ((u64)1 << mod);
            }
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
