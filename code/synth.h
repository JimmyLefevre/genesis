
#define MAX_OSCILLATORS_PER_INSTRUMENT  2
#define MAX_INSTRUMENT_COUNT           16

ENUM(SYNTH_WAVEFORM) {
    SAWTOOTH = 1,
    PULSE,
    TRIANGLE,
    SINE,
};

struct Oscillator_SOA {
    u8 count;
    u8 waveforms[MAX_OSCILLATORS_PER_INSTRUMENT];
    f32 mix[MAX_OSCILLATORS_PER_INSTRUMENT];
};

struct Synth_Input_Instrument {
    Oscillator_SOA oscillators;
    
    f32 attack;
    f32 decay;
    f32 sustain_factor;
    f32 release;
    
    f32 amplitude;
};

struct Synth_Instrument {
    Oscillator_SOA oscillators;
    
    s32 attack_samples;
    s32 decay_samples;
    f32 sustain_factor;
    s32 release_samples;
    
    f32 amplitude;
    
    // @Incomplete: filter, delay/reverb, chorus... other effects.
};

struct Synth_Note {
    s32 start_sample;
    s32 end_sample;
    f32 wavelength;
    f32 velocity; // @Incomplete: Convert to amplitude?
};

struct Oscillator_State {
    union {
        struct {
            f32 phase;       // -1.0f to 1.0f
        } sawtooth;
        
        struct {
            f32 phase;       // 0.0f to 1.0f
            f32 pulse_width; // 0.0f to 1.0f; 0.5f for square
        } pulse;
        
        struct {
            bool descending;
            f32 phase;       // -1.0f to 1.0f
        } triangle;
        
        struct {
            v2 phasor;
        } sine;
        
        u64 raw;
    };
};

struct Synth_Note_State {
    s32 index_in_track;
    f32 lerp_y;
    
    Oscillator_State oscillators[MAX_OSCILLATORS_PER_INSTRUMENT];
};

#define MAX_POLYPHONY 16

struct Synth_Track {
    Synth_Note *notes;
    s32 note_count;
    s32 samples_played;
    s32 next_note_to_play;
    s16 instrument;
    
    u8 current_note_count;
    Synth_Note_State current_notes[MAX_POLYPHONY];
};

// @Hardcoded
#define MAX_TRACK_COUNT 256

struct Synth {
    u8  core_count;
    u8  channel_count;
    s32 output_hz;
    
    // Audio_Volumes current_volume;
    // Audio_Volumes target_volume; // ;Settings
    
    f32  *temporary_buffer;  // f32               [audio_buffer_size]
    f32 **mixing_buffers;    // f32[channel_count][audio_buffer_size]
    s16  *output_buffer;     // s16               [audio_buffer_size]
    
    Synth_Instrument instruments[MAX_INSTRUMENT_COUNT];
    s16              instrument_count;
    
    s32 track_count;
    Synth_Track tracks[MAX_TRACK_COUNT];
    
#if GENESIS_DEV
    // This is to make sure the sound update doesn't take up more than a full frame.
    bool being_updated;
#endif
};
