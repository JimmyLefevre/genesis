
// @Hardcoded
#define AUDIO_HZ 44100
#define AUDIO_CHANNELS 2
#define AUDIO_BYTES_PER_SAMPLE (sizeof(s16) * AUDIO_CHANNELS)

typedef s32 WINAPI DirectSoundCreate_t(GUID *lpGuid, IDirectSound **ppDS, IUnknown *pUnkOuter);

static struct {
    IDirectSoundBuffer* secondary_buffer;
    s16 *start_of_buffer;
    s16 *fill_sec_1;
    s16 *fill_sec_2;
    DWORD fill_bytes_1;
    DWORD fill_bytes_2;
    u32 buffer_bytes;
    u32 buffer_samples;
    u32 samples_played;
} dsound;

static void dsound_init(HWND wnd_handle) {
    HMODULE dsound_lib = LoadLibraryW(L"dsound.dll");
    
    if(dsound_lib){
        DirectSoundCreate_t *DirectSoundCreate_ = (DirectSoundCreate_t *)GetProcAddress(dsound_lib, "DirectSoundCreate");
        IDirectSound *audio;
        
        if(DirectSoundCreate_ && (DirectSoundCreate_(0, &audio, 0) == DS_OK)){
            
            WAVEFORMATEX wav_format = {0};
            wav_format.wFormatTag = WAVE_FORMAT_PCM;
            wav_format.nChannels = 2;
            wav_format.wBitsPerSample = 16;
            wav_format.nSamplesPerSec = AUDIO_HZ;
            wav_format.nBlockAlign = wav_format.nChannels*wav_format.wBitsPerSample/8;
            wav_format.nAvgBytesPerSec = wav_format.nSamplesPerSec * wav_format.nBlockAlign;
            
            // Primary buffer:
            if(audio->SetCooperativeLevel(wnd_handle, DSSCL_PRIORITY) == DS_OK){
                DSBUFFERDESC primary_buffer_desc = {0};
                primary_buffer_desc.dwSize = sizeof(DSBUFFERDESC);
                primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                IDirectSoundBuffer *primary_buffer;
                if(audio->CreateSoundBuffer(&primary_buffer_desc, &primary_buffer, 0) == DS_OK){
                    if(primary_buffer->SetFormat(&wav_format) == DS_OK){
                    }
                }
            }
            
            // Secondary buffer:
            DSBUFFERDESC secondary_buffer_desc   = {0};
            secondary_buffer_desc.dwSize         = sizeof(DSBUFFERDESC);
            secondary_buffer_desc.dwFlags        = DSBCAPS_GETCURRENTPOSITION2;
            secondary_buffer_desc.dwBufferBytes  = AUDIO_HZ * sizeof(s16) * 2; // one second of audio buffering.
            secondary_buffer_desc.lpwfxFormat    = &wav_format;
            
            if(audio->CreateSoundBuffer(&secondary_buffer_desc, &dsound.secondary_buffer, 0) == DS_OK){
                dsound.secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);
                
                // Fill the secondary buffer after creating it.
                dsound.buffer_samples = AUDIO_HZ;
                dsound.buffer_bytes   = dsound.buffer_samples * AUDIO_BYTES_PER_SAMPLE;
                
                if(dsound.secondary_buffer->Lock(0, dsound.buffer_bytes,
                                                 (void **)(&dsound.fill_sec_1), &dsound.fill_bytes_1,
                                                 (void **)(&dsound.fill_sec_2), &dsound.fill_bytes_2, 0) == DS_OK) {
                    dsound.start_of_buffer = dsound.fill_sec_1;
                    
                    dsound.secondary_buffer->Unlock(dsound.fill_sec_1, dsound.fill_bytes_1,
                                                    dsound.fill_sec_2, dsound.fill_bytes_2);
                    
                    return;
                } else {
                    UNHANDLED; // Device was disconnected?
                }
            }
        }
    }
    
    UNHANDLED;
}

static OS_BEGIN_SOUND_UPDATE(win_begin_sound_update) {
    DWORD play_cur;
    DWORD write_cur;
    if(dsound.secondary_buffer->GetCurrentPosition(&play_cur, &write_cur) == DS_OK) {
        
        u32 buffer_bytes = dsound.buffer_bytes;
        
        dsound.samples_played = dsound.samples_played % dsound.buffer_samples;
        ASSERT(dsound.samples_played < dsound.buffer_samples);
        
        // @Hack.
        static bool startup_hack = false;
        if(!startup_hack) {
            dsound.samples_played = play_cur/AUDIO_BYTES_PER_SAMPLE;
            startup_hack = true;
        }
        
        u32 lock_byte = dsound.samples_played * AUDIO_BYTES_PER_SAMPLE;
        
        u32 target_cur = (u32)(write_cur + (AUDIO_HZ*AUDIO_BYTES_PER_SAMPLE*0.1))%buffer_bytes;
        // Tested DSound minimum latency (from play to write cursor) was ~30ms.
        u32 write_bytes = 0;
        if(lock_byte > target_cur) {
            
            write_bytes = target_cur + buffer_bytes - lock_byte;
        }
        else if(lock_byte < target_cur) {
            
            write_bytes = target_cur - lock_byte;
        }
        
        s32 dserr = dsound.secondary_buffer->Lock(lock_byte, write_bytes,
                                                  (void **)(&dsound.fill_sec_1), &dsound.fill_bytes_1,
                                                  (void **)(&dsound.fill_sec_2), &dsound.fill_bytes_2, 0);
        if(dserr == DS_OK) {
            ASSERT(write_bytes);
            ASSERT((dsound.fill_bytes_1 % 4) == 0);
            ASSERT((dsound.fill_bytes_2 % 4) == 0);
            
            u32 samples_to_update = (dsound.fill_bytes_1 + dsound.fill_bytes_2) / AUDIO_BYTES_PER_SAMPLE;
            return(samples_to_update);
        } else {
            ASSERT(!write_bytes);
        }
    } else {
        // @Incomplete: We get here when the audio device is disconnected.
        // What can we do about that?
        UNHANDLED;
    }
    
    return(0);
}

static OS_END_SOUND_UPDATE(win_end_sound_update) {
    u32 fill_1 = dsound.fill_bytes_1;
    u32 fill_2 = dsound.fill_bytes_2;
    s16 *out_1 = dsound.fill_sec_1;
    s16 *out_2 = dsound.fill_sec_2;
    s16 *in_1 = input_buffer;
    s16 *in_2 = input_buffer + (fill_1 / sizeof(s16));
    
    if(samples_to_update) {
        mem_copy(in_1, out_1, fill_1);
        mem_copy(in_2, out_2, fill_2);
        dsound.samples_played += samples_to_update;
    }
    
    s32 dserr = dsound.secondary_buffer->Unlock(out_1, fill_1, out_2, fill_2);
    ASSERT(dserr == DS_OK);
}