
static inline f32 midi_key_to_frequency(u8 key) {
    f32 result = 440.0f * f_pow2((CAST(f32, key) - 69.0f) * (1.0f / 12.0f));
    return result;
}

static void init_synth(Synth* synth, Memory_Block* memory_block, u8 core_count, u8 channel_count, s32 output_hz_s) {
    *synth = {};
    synth->core_count    =    core_count;
    synth->channel_count = channel_count;
    synth->output_hz     =     output_hz_s;
    synth->temporary_buffer = push_array(memory_block, f32,      output_hz_s);
    synth->mixing_buffers   = push_array(memory_block, f32*, channel_count);
    synth->output_buffer    = push_array(memory_block, s16,      output_hz_s);
    
    string file = os_platform.read_entire_file(STRING("W:\\genesis\\assets\\latest.synth_input"));
    
    if(file.length) {
        auto header = CAST(Synth_Input_Header*, file.data);
        ASSERT(header->version == SYNTH_INPUT_VERSION);
        
        f32 input_hz = CAST(f32, header->ticks_per_second);
        f32 output_hz = CAST(f32, synth->output_hz);
        f32 ticks_to_samples = output_hz / input_hz;
        
        auto input_track = CAST(Synth_Input_Track*, header + 1);
        
        FORI(0, 1) { //header->track_count) {
            // @Hack
            s16 instrument_index = synth->instrument_count++;
            
            Synth_Track synth_track = {};
            synth_track.instrument = instrument_index;
            synth_track.note_count = input_track->note_count;
            synth_track.notes = push_array(memory_block, Synth_Note, synth_track.note_count);
            
            auto input_notes = CAST(Synth_Input_Note*, input_track + 1);
            
            FORI_NAMED(note_index, 0, input_track->note_count) {
                auto input_note = &input_notes[note_index];
                auto synth_note = &synth_track.notes[note_index];
                
                synth_note->start_sample = f_round_to_s(CAST(f32, input_note->start) * ticks_to_samples);
                // We add -1 because sequential notes overlap.
                synth_note->end_sample = f_round_to_s(CAST(f32, input_note->start + input_note->duration) * ticks_to_samples) - 1;
                synth_note->wavelength = output_hz / midi_key_to_frequency(input_note->midi_key);
                synth_note->velocity = CAST(f32, input_note->velocity);
            }
            
            input_track = CAST(Synth_Input_Track*, input_notes + input_track->note_count);
            
            // @Hardcoded
            Synth_Instrument instrument = {};
            
            instrument.oscillators.mix[0] = 1.0f;
            instrument.oscillators.count = 1;
            instrument.amplitude = 500.0f;
            switch(instrument_index) {
                case 0: {
                    instrument.oscillators.waveforms[0] = SYNTH_WAVEFORM::TRIANGLE;
                } break;
                
                case 1: {
                    instrument.oscillators.waveforms[0] = SYNTH_WAVEFORM::SAWTOOTH;
                } break;
                
                case 2: {
                    instrument.oscillators.waveforms[0] = SYNTH_WAVEFORM::PULSE;
                } break;
            }
            
            f32 attack_t = 0.015f;
            f32 decay_t = 0.030f;
            f32 release_t = 0.035f;
            
            instrument.attack_samples = f_round_to_s(attack_t * synth->output_hz);
            instrument.decay_samples = f_round_to_s(decay_t * synth->output_hz);
            instrument.sustain_factor = 0.5f;
            instrument.release_samples = f_round_to_s(release_t * synth->output_hz);
            
            synth->instruments[instrument_index] = instrument;
            synth->tracks[synth->track_count++] = synth_track;
        }
    }
}

static void generate_sawtooth(Oscillator_State* state, const s32 sample_count, f32 wavelength, f32 start_volume, f32 end_volume, f32* out) {
    MIRROR_VARIABLE(f32, phase, &state->sawtooth.phase);
    const f32 phase_step = 2.0f / wavelength;
    const f32 volume_step = (end_volume - start_volume) / CAST(f32, sample_count);
    f32 volume = start_volume;
    
    FORI_NAMED(sample, 0, sample_count) {
        phase += phase_step;
        volume += volume_step;
        if(phase > 1.0f) {
            phase -= 2.0f;
        }
        
        f32 v = phase * volume;
        
        out[sample] += v;
    }
}
static void generate_pulse(Oscillator_State* state, const s32 sample_count, f32 wavelength, f32 start_volume, f32 end_volume, f32* out) {
    MIRROR_VARIABLE(f32, phase, &state->pulse.phase);
    const f32 pulse_width = state->pulse.pulse_width;
    const f32 phase_step = 1.0f / wavelength;
    const f32 volume_step = (end_volume - start_volume) / CAST(f32, sample_count);
    f32 volume = start_volume;
    
    FORI_NAMED(sample, 0, sample_count) {
        volume += volume_step;
        phase += phase_step;
        
        f32 v = volume;
        if(phase > pulse_width) {
            v = -v;
        }
        
        if(phase > 1.0f) {
            phase -= 1.0f;
        }
        
        out[sample] += v;
    }
}
static void generate_triangle(Oscillator_State* state, const s32 sample_count, f32 wavelength, f32 start_volume, f32 end_volume, f32* out) {
    MIRROR_VARIABLE(f32, phase, &state->triangle.phase);
    f32 phase_step = 2.0f / (wavelength * 0.5f);
    if(state->triangle.descending) {
        phase_step = -phase_step;
    }
    const f32 volume_step = (end_volume - start_volume) / CAST(f32, sample_count);
    f32 volume = start_volume;
    
    FORI_NAMED(sample, 0, sample_count) {
        volume += volume_step;
        phase += phase_step;
        
        if(phase < -1.0f) {
            phase      = -1.0f + (-1.0f - phase);
            phase_step = -phase_step;
        }
        
        if(phase > 1.0f) {
            phase      = 1.0f - (phase - 1.0f);
            phase_step = -phase_step;
        }
        
        f32 v = phase * volume;
        
        out[sample] += v;
    }
    state->triangle.descending = (phase_step < 0.0f);
}
static void generate_sine(Oscillator_State* state, const s32 sample_count, f32 wavelength, f32 start_volume, f32 end_volume, f32 *out) {
    MIRROR_VARIABLE(v2, phasor, &state->sine.phasor);
    const v2 sine_step = v2_angle_to_complex(TAU / wavelength);
    const f32 volume_step = (end_volume - start_volume) / CAST(f32, sample_count);
    f32 volume = start_volume;
    
    FORI_NAMED(sample, 0, sample_count) {
        volume += volume_step;
        phasor = v2_complex_prod(phasor, sine_step);
        
        f32 v = phasor.y * volume;
        
        out[sample] += v;
    }
    
    // @Robustness: Since sample_count is variable, we might need to normalize inside the loop
    // if we have so many iterations we actually noticeably lose precision.
    phasor = v2_normalize(phasor);
}

s16* Implicit_Context::update_synth(Synth* synth, const s32 samples_to_update) {
    TIME_BLOCK;
    SCOPE_MEMORY(&temporary_memory);
    zero_mem(synth->temporary_buffer, sizeof(s16) * synth->channel_count * samples_to_update);
    
    u8 *notes_to_retire = push_array(&temporary_memory, u8, MAX_POLYPHONY);
    s32 *tracks_to_retire = push_array(&temporary_memory, s32, synth->track_count);
    s32 tracks_to_retire_count = 0;
    
    FORI_TYPED_NAMED(s32, track_index, 0, synth->track_count) {
        auto track = &synth->tracks[track_index];
        auto instrument = &synth->instruments[track->instrument];
        
        {
            FORI_TYPED_NAMED(s32, note_index, track->next_note_to_play, track->note_count) {
                auto note = &track->notes[note_index];
                
                if(note->start_sample <= (track->samples_played + samples_to_update)) {
                    // Start playing the note.
                    ASSERT(track->current_note_count < MAX_POLYPHONY);
                    
                    auto note_state = &track->current_notes[track->current_note_count++];
                    *note_state = {};
                    
                    note_state->index_in_track = note_index;
                    
                    // The sine is the only oscillator that we can't 0-initialise.
                    FORI(0, MAX_OSCILLATORS_PER_INSTRUMENT) {
                        if(instrument->oscillators.waveforms[i] == SYNTH_WAVEFORM::SINE) {
                            note_state->oscillators[i].sine.phasor.x = 1.0f;
                        }
                    }
                    
                    track->next_note_to_play += 1;
                } else {
                    break;
                }
            }
        }
        
        s32 notes_to_retire_count = 0;
        FORI_NAMED(note_index, 0, track->current_note_count) {
            auto state = &track->current_notes[note_index];
            auto note = &track->notes[state->index_in_track];
            
            MIRROR_VARIABLE(f32, from_y, &state->lerp_y);
            
            const s32 start_offset = note->start_sample - track->samples_played;
            const s32 start_offset_clipped = s_max(0, start_offset);
            
            // We just model the envelope explicitly as an array of points.
            // We may cache these if generating them on the spot ever gets too slow.
            s32 lerp_points_x[5] = {
                start_offset,
                lerp_points_x[0] + instrument->attack_samples,
                lerp_points_x[1] + instrument->decay_samples,
                note->end_sample - track->samples_played,
                lerp_points_x[3] + instrument->release_samples,
            };
            
            f32 lerp_points_y[5] = {
                lerp_points_y[0] = 0.0f,
                lerp_points_y[1] = instrument->amplitude,
                lerp_points_y[2] = instrument->amplitude * instrument->sustain_factor,
                lerp_points_y[3] = lerp_points_y[2],
                lerp_points_y[4] = 0.0f,
            };
            
            s32 first_lerp = 4;
            s32 last_lerp = 4;
            FORI_TYPED(s32, 0, 5) {
                if(lerp_points_x[i] > start_offset_clipped) {
                    first_lerp = i;
                    break;
                } else if(lerp_points_x[i] == start_offset_clipped) {
                    // Skip over if we have 0-length attack and/or decay.
                    from_y = lerp_points_y[i];
                }
            }
            
            FORI_TYPED(s32, first_lerp, 5) {
                s32 x = lerp_points_x[i];
                if(x >= samples_to_update) {
                    last_lerp = i;
                    lerp_points_x[i] = samples_to_update;
                    f32 lerp_factor = CAST(f32, samples_to_update - lerp_points_x[i-1]) / CAST(f32, x - lerp_points_x[i-1]);
                    lerp_points_y[i] = f_lerp(lerp_points_y[i-1], lerp_points_y[i], lerp_factor);
                    break;
                }
            }
            
            bool note_ending = (note->end_sample + instrument->release_samples - track->samples_played) <= samples_to_update;
            if(note_ending) {
                // The note ends on this frame.
                // With monophonic tracks, we could just store an array of notes sorted by start_sample, but
                // with polyphony, we're allowing a note to both start later than _AND_ end before another. So
                // we have to do some bookkeeping here.
                notes_to_retire[notes_to_retire_count++] = CAST(u8, note_index);
            }
            
            FORI_NAMED(oscillator_index, 0, instrument->oscillators.count) {
                f32 mix = instrument->oscillators.mix[oscillator_index];
                u8 waveform = instrument->oscillators.waveforms[oscillator_index];
                
                switch(waveform) {
                    
#define GENERATE_WAVEFORM_CASE(waveform, proc) \
                    case waveform: {\
                        s32 from_x = start_offset_clipped;\
                        \
                        FORI(first_lerp, last_lerp + 1) {\
                            s32 to_x = lerp_points_x[i];\
                            f32 to_y = lerp_points_y[i];\
                            \
                            if(from_x != to_x) {\
                                proc(&state->oscillators[oscillator_index], to_x - from_x, note->wavelength, from_y, to_y, synth->temporary_buffer + from_x);\
                            }\
                            \
                            from_x = to_x;\
                            from_y = to_y;\
                        }\
                    } break
                    
                    GENERATE_WAVEFORM_CASE(SYNTH_WAVEFORM::SAWTOOTH, generate_sawtooth);
                    GENERATE_WAVEFORM_CASE(SYNTH_WAVEFORM::TRIANGLE, generate_triangle);
                    GENERATE_WAVEFORM_CASE(SYNTH_WAVEFORM::PULSE, generate_pulse);
                    GENERATE_WAVEFORM_CASE(SYNTH_WAVEFORM::SINE, generate_sine);
                    
#undef GENERATE_WAVEFORM_CASE
                    
                    default: UNHANDLED;
                    
                }
            }
        }
        
        {
            MIRROR_VARIABLE(u8, current_note_count, &track->current_note_count);
            FORI_REVERSE(notes_to_retire_count - 1, 0) {
                track->current_notes[notes_to_retire[i]] = track->current_notes[--current_note_count];
            }
        }
        
        if(!track->current_note_count && (track->next_note_to_play == track->note_count)) {
            // We can retire the track altogether.
            tracks_to_retire[tracks_to_retire_count++] = track_index;
        }
        
        track->samples_played += samples_to_update;
    }
    
    {
        MIRROR_VARIABLE(s32, track_count, &synth->track_count);
        FORI_REVERSE(tracks_to_retire_count - 1, 0) {
            synth->tracks[tracks_to_retire[i]] = synth->tracks[--track_count];
        }
    }
    
    // @Temporary: We're going straight from the temporary_buffer to the output_buffer.
    FORI_NAMED(sample, 0, samples_to_update) {
        f32 volume = synth->temporary_buffer[sample];
        
        volume = f_min(32767.0f, f_max(-32768.0f, volume));
        
        FORI_NAMED(channel, 0, synth->channel_count) {
            synth->output_buffer[(sample * synth->channel_count) + channel] = CAST(s16, volume + 0.5f);
        }
    }
    
    return synth->output_buffer;
}
