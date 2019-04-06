
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

static void load_chunk(Asset_Location location, s16 *samples, s32 chunk_index, s32 channel_count, s32 channel_index) {
    const s32 bytes_per_chunk = SAMPLES_PER_CHUNK * sizeof(s16);
    os_platform.read_file(location.file_handle, samples,
                          location.offset + sizeof(FACS_Header) +
                          bytes_per_chunk * channel_count * chunk_index +
                          bytes_per_chunk * channel_index,
                          bytes_per_chunk);
}

static void load_sound_info(Loaded_Sound *loaded, Asset_Location *location, FACS_Header *facs, u8 category) {
    loaded->streaming_data = *location;
    loaded->chunk_count = facs->chunk_count;
    loaded->channel_count = facs->channel_count;
    loaded->category = category;
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

static v2 position_for_channel(s32 channel, s32 sound_channel_count, s32 output_channel_count) {
    v2 result = V2();
    
    if(output_channel_count == 2) {
        if(channel == 0) {
            result = V2(-1.0f, 0.0f);
        } else {
            ASSERT(channel == 1);
            result = V2(1.0f, 0.0f);
        }
    } else {
        UNHANDLED;
    }
    
    return result;
}

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


static void play_sound(Audio_Info *audio, s16 loaded_id, v2 position, s32 channel_index = 0) {
    if(loaded_id != -1) {
        Loaded_Sound *loaded = audio->loads.items + loaded_id;
        Playing_Sound *sound = reserve(&audio->sounds, 1);
        
        sound->cursor.pitch = 1.0f;
        sound->loaded = loaded_id;
        sound->next_chunk_to_load = 0;
        sound->chunk_count = loaded->chunk_count;
        sound->position = position;
        // @Temporary: Update this separately.
        sound->new_position = position;
        sound->channel_index = (u8)channel_index;
        sound->silent = false;
        sound->category = loaded->category;
        ASSERT(sound->current_samples);
        ASSERT(sound->next_samples);
        ASSERT(sound->current_samples != sound->next_samples);
        
        // @Speed: Right now, we're reading from disk for every sound.
        // We may want to make a chunk cache in order to avoid doing so.
        load_chunk(loaded->streaming_data, sound->current_samples, sound->next_chunk_to_load++, loaded->channel_count, sound->channel_index);
        if(sound->next_chunk_to_load < sound->chunk_count) {
            load_chunk(loaded->streaming_data, sound->next_samples, sound->next_chunk_to_load++, loaded->channel_count, sound->channel_index);
        }
    }
}

static void play_music(Audio_Info *audio, s16 loaded_id) {
    u8 channel_count = audio->loads.items[loaded_id].channel_count;
    
    for(s32 i = 0; i < channel_count; ++i) {
        play_sound(audio, loaded_id, position_for_channel(i, channel_count, audio->channel_count), i);
    }
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

static void schedule_for_destruction(Audio_Info *audio, Playing_Sound *sound) {
    s32 index = (s32)(sound - audio->sounds.items);
    add(&audio->sounds_to_destroy, index);
}

s16 *Implicit_Context::update_audio(Audio_Info *audio, const s32 samples_to_play, const f32 dt) {
    TIME_BLOCK;
    
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
    const u8 channel_count = audio->channel_count;
    
    for(s32 i = 0; i < channel_count; ++i) {
        zero_mem(audio->mix_buffers[i], sizeof(s16) * AUDIO_BUFFER_SIZE);
    }
    for(s32 i = 0; i < channel_count; ++i) {
        zero_mem(audio->out_buffer, sizeof(s16) * AUDIO_BUFFER_SIZE);
    }
    
    FOR(&audio->sounds) {
        compute_volumes_for_position(start_volume, channel_count, it->position);
        compute_volumes_for_position(end_volume, channel_count, it->new_position);
        const f32 global_volume = master_volume * volumes_by_category[it->category];
        for(s32 channel = 0; channel < channel_count; ++channel) {
            start_volume[channel] *= global_volume;
            end_volume  [channel] *= global_volume;
        }
        
        // @Note: This (!global_volume) is incorrect if we change to a volume function
        // that doesn't get to zero.
        bool should_silence_this_frame = (!global_volume) || it->silent;
        bool should_silence_next_frame = true;
        for(s32 channel = 0; channel < channel_count; ++channel) {
            // Should we use a threshold here?
            if(end_volume[channel]) {
                should_silence_this_frame = false;
                should_silence_next_frame = false;
                break;
            }
        }
        
        const f32 pitch = it->cursor.pitch;
        const f32 sample_offset = it->cursor.sample_offset;
        
        s32 write_offset = 0;
        
        s32 pitched_samples = ceil_s(((f32)samples_to_play - sample_offset) / pitch);
        ASSERT(pitched_samples < SAMPLES_PER_CHUNK);
        const s32 samples_left = SAMPLES_PER_CHUNK - it->cursor.samples_played;
        const s32 pitched_samples_left = ceil_s(((f32)samples_left - sample_offset) / pitch);
        if(pitched_samples >= pitched_samples_left) {
            // End this chunk and replace it with the next.
            const f32 next_play_offset = sample_offset + ((f32)pitched_samples_left * pitch) - (f32)samples_left;
            const s32 next_samples_played = (s32)next_play_offset;
            const f32 next_sample_offset = next_play_offset - (f32)next_samples_played;
            
            const f32 t_volume = (f32)samples_left / (f32)samples_to_play;
            for(s32 i = 0; i < channel_count; ++i) {
                next_start_volume[i] = f_lerp(start_volume[i], end_volume[i], t_volume);
            }
            
            if(!should_silence_this_frame) {
                mix_samples(it->current_samples + it->cursor.samples_played, pitched_samples_left, sample_offset, pitch,
                            audio->temp_buffer, 0,
                            start_volume, next_start_volume, audio->mix_buffers, channel_count);
            }
            
            SWAP(it->current_samples, it->next_samples);
            const s32 next_chunk = it->next_chunk_to_load;
            Loaded_Sound *loaded = audio->loads.items + it->loaded;
            if(next_chunk < it->chunk_count) {
                load_chunk(loaded->streaming_data, it->next_samples, next_chunk, loaded->channel_count, it->channel_index);
            } else {
                schedule_for_destruction(audio, it);
            }
            
            // Copying things so the next mix mixes the next chunk.
            SWAP(start_volume, next_start_volume);
            it->cursor.samples_played = next_samples_played;
            ASSERT(it->cursor.samples_played < SAMPLES_PER_CHUNK);
            it->cursor.sample_offset = next_sample_offset;
            ++it->next_chunk_to_load;
            pitched_samples -= pitched_samples_left;
            write_offset = pitched_samples_left;
        }
        
        if(!should_silence_this_frame) {
            mix_samples(it->current_samples + it->cursor.samples_played, pitched_samples, it->cursor.sample_offset, pitch,
                        audio->temp_buffer, write_offset,
                        start_volume, end_volume, audio->mix_buffers, channel_count);
        }
        
        const f32 sample_advance = (f32)pitched_samples * pitch;
        const s32 sample_advance_trunc = (s32)sample_advance;
        it->cursor.samples_played += sample_advance_trunc;
        ASSERT(it->cursor.samples_played < SAMPLES_PER_CHUNK);
        it->cursor.sample_offset = sample_advance - (f32)sample_advance_trunc;
        it->silent = should_silence_next_frame;
    }
    
    // @Speed: The final mixing loop is difficult to make fast because of the parametric interleaving.
    // If speed ever becomes an issue here, we could either write hardcoded interleaving loops
    // for every channel count, or we could try to ask for non-interleaved device buffers.
    s16 * const out = audio->out_buffer;
    for(s32 ichannel = 0; ichannel < channel_count; ++ichannel) {
        const f32 * const mix = audio->mix_buffers[ichannel];
        for(s32 isample = 0; isample < samples_to_play; ++isample) {
            f32 in = mix[isample];
            
            if       (in > (f32)S16_MAX) {
                in = (f32)S16_MAX;
            } else if(in < (f32)S16_MIN) {
                in = (f32)S16_MIN;
            }
            
            out[isample * channel_count + ichannel] = (s16)in;
        }
    }
    
    {
        s32_dumb_sort_descending(audio->sounds_to_destroy.items, audio->sounds_to_destroy.count);
        
        FOR(&audio->sounds_to_destroy) {
            remove(&audio->sounds, *it);
        }
        
        audio->sounds_to_destroy.count = 0;
    }
    
    return audio->out_buffer;
}
