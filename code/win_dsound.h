
#define DS_OK 0
enum DSSCL_Flags {
    DSSCL_PRIORITY = 0x00000002,
};
enum DSBCAPS_Flags {
    DSBCAPS_PRIMARYBUFFER = 0x00000001,
    DSBCAPS_GETCURRENTPOSITION2 = 0x00010000,
};
enum DSBPLAY_Flags {
    DSBPLAY_LOOPING = 0x00000001,
};

enum Wave_Format {
    WAVE_FORMAT_PCM = 0x0001,
};

struct WAVEFORMATEX {
    u16  wFormatTag;
    u16  nChannels;
    u32 nSamplesPerSec;
    u32 nAvgBytesPerSec;
    u16  nBlockAlign;
    u16  wBitsPerSample;
    u16  cbSize;
};

struct DSCAPS {
    u32           dwSize;
    u32           dwFlags;
    u32           dwMinSecondarySampleRate;
    u32           dwMaxSecondarySampleRate;
    u32           dwPrimaryBuffers;
    u32           dwMaxHwMixingAllBuffers;
    u32           dwMaxHwMixingStaticBuffers;
    u32           dwMaxHwMixingStreamingBuffers;
    u32           dwFreeHwMixingAllBuffers;
    u32           dwFreeHwMixingStaticBuffers;
    u32           dwFreeHwMixingStreamingBuffers;
    u32           dwMaxHw3DAllBuffers;
    u32           dwMaxHw3DStaticBuffers;
    u32           dwMaxHw3DStreamingBuffers;
    u32           dwFreeHw3DAllBuffers;
    u32           dwFreeHw3DStaticBuffers;
    u32           dwFreeHw3DStreamingBuffers;
    u32           dwTotalHwMemBytes;
    u32           dwFreeHwMemBytes;
    u32           dwMaxContigFreeHwMemBytes;
    u32           dwUnlockTransferRateHwBuffers;
    u32           dwPlayCpuOverheadSwBuffers;
    u32           dwReserved1;
    u32           dwReserved2;
};

struct DSBCAPS {
    u32           dwSize;
    u32           dwFlags;
    u32           dwBufferBytes;
    u32           dwUnlockTransferRate;
    u32           dwPlayCpuOverhead;
};

struct DSBUFFERDESC
{
    u32           dwSize;
    u32           dwFlags;
    u32           dwBufferBytes;
    u32           dwReserved;
    WAVEFORMATEX  *lpwfxFormat;
    // @Incomplete ???
#if DIRECTSOUND_VERSION >= 0x0700
    GUID            guid3DAlgorithm;
#endif
};

struct IDirectSoundBuffer;
struct IDirectSound {
    // IUnknown methods
    virtual s32 QueryInterface       (const GUID &riid, void **ppvObject) = 0;
    virtual u32 AddRef        () = 0;
    virtual u32 Release       () = 0;
    
    // IDirectSound methods
    virtual s32 CreateSoundBuffer    (DSBUFFERDESC *pcDSBufferDesc, IDirectSoundBuffer **ppDSBuffer, IUnknown *pUnkOuter) = 0;
    virtual s32 GetCaps              (DSCAPS *pDSCaps) = 0;
    virtual s32 DuplicateSoundBuffer (IDirectSoundBuffer *pDSBufferOriginal, IDirectSoundBuffer **ppDSBufferDuplicate) = 0;
    virtual s32 SetCooperativeLevel  (void *hwnd, u32 dwLevel) = 0;
    virtual s32 Compact              () = 0;
    virtual s32 GetSpeakerConfig     (u32 *pdwSpeakerConfig) = 0;
    virtual s32 SetSpeakerConfig     (u32 dwSpeakerConfig) = 0;
    virtual s32 Initialize           (GUID *pcGuidDevice) = 0;
};

struct IDirectSoundBuffer {
    // IUnknown methods
    virtual s32 QueryInterface       (const GUID &riid, void **ppvObject) = 0;
    virtual u32 AddRef        () = 0;
    virtual u32 Release       () = 0;
    
    // IDirectSoundBuffer methods
    virtual s32 GetCaps              (DSBCAPS *pDSBufferCaps) = 0;
    virtual s32 GetCurrentPosition   (u32 *pdwCurrentPlayCursor, u32 *pdwCurrentWriteCursor) = 0;
    virtual s32 GetFormat            (WAVEFORMATEX *pwfxFormat, u32 dwSizeAllocated,
                                          u32 *pdwSizeWritten) = 0;
    virtual s32 GetVolume            (s32 *plVolume) = 0;
    virtual s32 GetPan               (s32 *plPan) = 0;
    virtual s32 GetFrequency         (u32 *pdwFrequency) = 0;
    virtual s32 GetStatus            (u32 *pdwStatus) = 0;
    virtual s32 Initialize           (IDirectSound *pDirectSound, DSBUFFERDESC *pcDSBufferDesc) = 0;
    virtual s32 Lock                 (u32 dwOffset, u32 dwBytes,
                                          void **ppvAudioPtr1, u32 *pdwAudioBytes1,
                                          void **ppvAudioPtr2, u32 *pdwAudioBytes2, u32 dwFlags) = 0;
    virtual s32 Play                 (u32 dwReserved1, u32 dwPriority, u32 dwFlags) = 0;
    virtual s32 SetCurrentPosition   (u32 dwNewPosition) = 0;
    virtual s32 SetFormat            (WAVEFORMATEX *pcfxFormat) = 0;
    virtual s32 SetVolume            (s32 lVolume) = 0;
    virtual s32 SetPan               (s32 lPan) = 0;
    virtual s32 SetFrequency         (u32 dwFrequency) = 0;
    virtual s32 Stop                 () = 0;
    virtual s32 Unlock               (void *pvAudioPtr1, u32 dwAudioBytes1,
                                          void *pvAudioPtr2, u32 dwAudioBytes2) = 0;
    virtual s32 Restore              () = 0;
};
