
#define UNICODE
#define _UNICODE

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB 0x2093
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_ACCELERATION_ARB 0x2003
#define WGL_FULL_ACCELERATION_ARB 0x2027
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_PIXEL_TYPE_ARB 0x2013
#define WGL_TYPE_RGBA_ARB 0x202B
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB 0x20A9


#include <intrin.h>
#include "basic.h"
#include "win_windows.h"
extern "C" {
#include "win_nocrt.c"
    // shlwapi.h:
    s32 PathFileExistsW(const u16 *pszPath);
}
#include "win_dsound.h"

#include "os_headers.h"
#include "win_main.h"
#include "os_context.h"
#include "interfaces.h"
#include "os_code.cpp"

#define AUDIO_HZ 44100
#define AUDIO_CHANNELS 2
#define AUDIO_BYTES_PER_SAMPLE (sizeof(s16) * AUDIO_CHANNELS)

static Program_State program_state;
static bool window_was_resized_by_platform;
static bool input_should_reload;
static bool cursor_is_being_captured;
static bool cursor_was_captured_when_focus_was_lost;
static bool alt_is_down;
static bool currently_fullscreen;

static s32 window_border_added_width;
static s32 window_border_added_height;

static u64 cpu_freq;
static f64 inv_cpu_freq;
static u64 base_time;
static f64 cur_time;

static void *global_window_handle;
static IDirectSoundBuffer *global_secondary_buffer;
static dsound_buffer global_secondary_buffer_info;

struct thread_job {
    void (*function)(Implicit_Context *_this, void *);
    void *data;
};
static struct {
    s32 thread_count;
    void *semaphore_handle;
    volatile u32 next_available_job;
    u32 next_free_job;
    u32 job_queue_length;
    thread_job *job_queue;
} global_thread_info;

typedef s32 WINAPI DirectSoundCreate_t(GUID *lpGuid, IDirectSound **ppDS, IUnknown *pUnkOuter);
typedef s32 WINAPI wglChoosePixelFormatARB_t(void *hDC, const s32 *piAttribIList, const f32 *pfAttribFList, u32 nMaxFormats, s32 *piFormats, u32 *nNumFormats);
typedef void * WINAPI wglCreateContextAttribsARB_t(void *hDC, void *hShareContext, const s32 *attribList);
typedef s32 WINAPI wglSwapIntervalEXT_t(s32 interval);
typedef void *WINAPI wglGetProcAddress_t(const char *lpszProc);
typedef s32 WINAPI wglMakeCurrent_t(void *hdc, void *hglrc);
typedef s32 WINAPI wglDeleteContext_t(void *hglrc);
typedef void *WINAPI wglCreateContext_t(void *hdc);
typedef const u8 *wglGetExtensionsStringEXT_t(void);
// Extended:
GLOBAL_FUNCTION_POINTER(wglChoosePixelFormatARB);
GLOBAL_FUNCTION_POINTER(wglCreateContextAttribsARB);
GLOBAL_FUNCTION_POINTER(wglSwapIntervalEXT);
GLOBAL_FUNCTION_POINTER(wglGetExtensionsStringEXT);
// Base:
GLOBAL_FUNCTION_POINTER(wglGetProcAddress);
GLOBAL_FUNCTION_POINTER(wglMakeCurrent);
GLOBAL_FUNCTION_POINTER(wglDeleteContext);
GLOBAL_FUNCTION_POINTER(wglCreateContext);
static void *opengl_lib;

// Raymond Chen's fullscreen toggle.
WINDOWPLACEMENT wnd_placement = {sizeof(wnd_placement)};
static void toggle_fullscreen(void * wnd_handle) {
    u32 wnd_style    = GetWindowLongW(wnd_handle, GWL_STYLE);
    if(wnd_style & WS_OVERLAPPEDWINDOW){ // switch to fullscreen
        MONITORINFO mi = {sizeof(mi)};
        if(GetWindowPlacement(wnd_handle, &wnd_placement) &&
           GetMonitorInfoW(MonitorFromWindow(wnd_handle, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongW(wnd_handle, GWL_STYLE  , wnd_style    &    ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(wnd_handle, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
        }
    } else {                              // switch to windowed
        SetWindowLongW(wnd_handle, GWL_STYLE  , wnd_style    |    WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(wnd_handle, &wnd_placement);
        SetWindowPos(wnd_handle, 0, 0, 0, 0, 0,
                     SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|
                     SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
    }
}

static OS_MEM_ALLOC(win_mem_alloc) {
    return VirtualAlloc(0, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
}

inline void win_free(void *data) {
    if(data) VirtualFree(data, 0, MEM_RELEASE);
}

inline u64 get_cpu_time() {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart;
}

inline u64 get_cpu_freq() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    
    return(freq.QuadPart);
}

static OS_GET_SECS(win_get_seconds) {
    u64 time = get_cpu_time();
    time -= base_time;
    // It turns out that using __rdtsc *AND* QueryPerformanceCounter is a bad
    // idea because, while __rdtsc always returns the CPU's timestamp, QueryPerformanceCounter
    // compensates for QueryPerformanceFrequency sometimes being 1024 times less than it should.
    return ((f64)time) * inv_cpu_freq;
}

// @Incomplete: I don't know what's better.
#if 0
static void get_hardware_information(void * dc) {
#if 1
    s32 resolution_x = GetDeviceCaps(dc, HORZRES);
    s32 resolution_y = GetDeviceCaps(dc, VERTRES);
    s32 refresh_rate = GetDeviceCaps(dc, VREFRESH);
#else
    DEVMODE device;
    device.dmSize = sizeof(DEVMODE);
    EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &device);
#endif
}
#endif

inline f64 set_seconds() {
    cur_time = win_get_seconds();
    return(cur_time);
}

inline void get_window_dim(void * handle, s32 dim[2]) {
    RECT rect;
    GetClientRect(handle, &rect);
    dim[0] = rect.right - rect.left;
    dim[1] = rect.bottom - rect.top;
}

static usize win_file_size(void *file_handle) {
    LARGE_INTEGER file_size;
    if(GetFileSizeEx(file_handle, &file_size)) {
        return(file_size.QuadPart);
    }
    return(0);
}

static void *win_open_file_utf16(u16 *file_name) {
    void * file_handle = CreateFileW(file_name,
                                     GENERIC_READ,
                                     FILE_SHARE_READ,
                                     0,
                                     OPEN_EXISTING,
                                     0,
                                     0);
    
    return(file_handle);
}

static void *win_open_file_utf8(string file_name) {
    ASSERT(file_name.length < 100);
    u16 utf16filename[256];
    s32 written = utf8_to_utf16(file_name, utf16filename);
    utf16filename[written] = '\0';
    
    return win_open_file_utf16(utf16filename);
}

OS_CLOSE_FILE(win_close_file) {
    CloseHandle(handle);
}

// From MSDN:
// "If the function fails, or is completing asynchronously, the return value is zero (FALSE)."
// ...so we can't really make use of the return value right now.
static OS_READ_FILE(win_read_file) {
    ASSERT(length <= 0xFFFFFFFF);
    u32 bytes_read = 0;
    OVERLAPPED overlapped = {0};
    overlapped.Offset = offset & 0xFFFFFFFF;
    overlapped.OffsetHigh = (u32)(offset >> 32);
    ReadFile(handle, out, (u32)length, &bytes_read, &overlapped);
    return (bytes_read == (u32)length);
}

static OS_READ_ENTIRE_FILE(win_read_entire_file) {
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
        if(ReadFile(file_handle, file_data, (u32)file_size, &bytes_read, 0) &&
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

static bool win_write_entire_file(string file_name, string file_data) {
    ASSERT(file_name.length < 100);
    u16 utf16filename[100] = {0};
    utf8_to_utf16(file_name, utf16filename);
    
    bool success = false;
    
    void * file_handle = CreateFileW(utf16filename,
                                     GENERIC_READ|GENERIC_WRITE,
                                     FILE_SHARE_READ|FILE_SHARE_WRITE,
                                     0,
                                     CREATE_ALWAYS,
                                     0,
                                     0);
    
    if(file_handle != INVALID_HANDLE_VALUE) {
        
        u32 bytes_written;
        if(WriteFile(file_handle, file_data.data, (u32)file_data.length, &bytes_written, 0) &&
           (bytes_written == file_data.length)) {
            success = true;
        }
        
        CloseHandle(file_handle);
    }
    
    return(success);
}

static void center_cursor(void * wnd_handle){
    
    RECT rect;
    GetWindowRect(wnd_handle, &rect);
    int centerx = (rect.right + rect.left) / 2;
    int centery = (rect.bottom + rect.top) / 2;
    
    SetCursorPos(centerx, centery);
}

static void get_cursor_position_in_pixels(s32 *out_x, s32 *out_y) {
    POINT cursor_p;
    
    GetCursorPos(&cursor_p);
    ScreenToClient(global_window_handle, &cursor_p);
    
    *out_x = cursor_p.x - program_state.draw_rect[0];
    *out_y = (program_state.draw_rect[3] - program_state.draw_rect[1]) - (cursor_p.y - program_state.draw_rect[1]);
}

static void win_capture_cursor() {
    while(ShowCursor(FALSE) >= 0)
        ;
    cursor_is_being_captured = true;
}

static void win_release_cursor() {
    while(ShowCursor(TRUE) < 0)
        ;
    cursor_is_being_captured = false;
}

static sptr WINAPI wnd_proc(void * handle,
                            u32 msg,
                            uptr w_param,
                            sptr l_param) {
    
    switch(msg) {
        
        case WM_SIZE: {
            switch(w_param) {
                
                case SIZE_MAXIMIZED:
                case SIZE_RESTORED: {
                    window_was_resized_by_platform = true;
                    program_state.should_render = true;
                    input_should_reload = true;
                } break;
                
                case SIZE_MINIMIZED: {
                    program_state.should_render = false;
                } break;
            }
            
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT paint;
            BeginPaint(handle, &paint);
            EndPaint(handle, &paint);
            return 0;
        }
        
        case WM_SYSCOMMAND: {
            // getting rid of alt+space
            if(w_param == SC_KEYMENU) return 0;
            return DefWindowProcW(handle, msg, w_param, l_param);
        }
        
        case WM_SETFOCUS: {
            program_state.should_render = true;
            if(cursor_was_captured_when_focus_was_lost) {
                win_capture_cursor();
            }
            return 0;
        }
        
        case WM_KILLFOCUS: {
            program_state.should_render = false;
            input_should_reload = true;
            cursor_was_captured_when_focus_was_lost = cursor_is_being_captured;
            win_release_cursor();
            return 0;
        }
        
        case WM_MOVE: {
            input_should_reload = true;
            return 0;
        }
        
        case WM_QUIT:
        case WM_CLOSE:
        case WM_DESTROY: {
            program_state.should_close = true;
        }
        
        default: {
            return(DefWindowProcW(handle, msg, w_param, l_param));
        }
    }
}

static void dsound_init(void * wnd_handle){
    void * dsound_lib = LoadLibraryW((u16 *)L"dsound.dll");
    if(dsound_lib){
        DirectSoundCreate_t *DirectSoundCreate_ = (DirectSoundCreate_t *)GetProcAddress(dsound_lib, "DirectSoundCreate");
        IDirectSound *dsound;
        
        if(DirectSoundCreate_ && (DirectSoundCreate_(0, &dsound, 0) == DS_OK)){
            
            WAVEFORMATEX wav_format = {0};
            wav_format.wFormatTag = WAVE_FORMAT_PCM;
            wav_format.nChannels = 2;
            wav_format.wBitsPerSample = 16;
            wav_format.nSamplesPerSec = AUDIO_HZ;
            wav_format.nBlockAlign = wav_format.nChannels*wav_format.wBitsPerSample/8;
            wav_format.nAvgBytesPerSec = wav_format.nSamplesPerSec * wav_format.nBlockAlign;
            
            // Primary buffer:
            if(dsound->SetCooperativeLevel(wnd_handle, DSSCL_PRIORITY) == DS_OK){
                DSBUFFERDESC primary_buffer_desc = {0};
                primary_buffer_desc.dwSize = sizeof(DSBUFFERDESC);
                primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                IDirectSoundBuffer *primary_buffer;
                if(dsound->CreateSoundBuffer(&primary_buffer_desc, &primary_buffer, 0) == DS_OK){
                    if(primary_buffer->SetFormat(&wav_format) == DS_OK){
                    }
                }
            }
            
            // Secondary buffer:
            DSBUFFERDESC secondary_buffer_desc = {0};
            secondary_buffer_desc.dwSize = sizeof(DSBUFFERDESC);
            secondary_buffer_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
            secondary_buffer_desc.dwBufferBytes = AUDIO_HZ * sizeof(s16) * 2; // one second of audio buffering.
            secondary_buffer_desc.lpwfxFormat = &wav_format;
            if(dsound->CreateSoundBuffer(&secondary_buffer_desc, &global_secondary_buffer, 0) == DS_OK){
            }
        }
    }
}

static int old_pf_index(void * dc) {
    
    PIXELFORMATDESCRIPTOR desired_pf = {0};
    desired_pf.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
    desired_pf.nVersion     = 1;
    desired_pf.dwFlags      = PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    desired_pf.iPixelType   = PFD_TYPE_RGBA;
    desired_pf.cColorBits   = 32;
    desired_pf.cAlphaBits   = 8;
    desired_pf.cAccumBits   = 0;
    desired_pf.cDepthBits   = 0;
    desired_pf.cStencilBits = 0;
    desired_pf.cAuxBuffers  = 0;
    desired_pf.iLayerType   = PFD_MAIN_PLANE;
    
    int pf_index = ChoosePixelFormat(dc, &desired_pf);
    return(pf_index);
}

static void *old_glrc(void * dc) {
    
    int pf_index = old_pf_index(dc);
    PIXELFORMATDESCRIPTOR given_pf;
    DescribePixelFormat(dc,
                        pf_index,
                        sizeof(PIXELFORMATDESCRIPTOR),
                        &given_pf);
    SetPixelFormat(dc, pf_index, &given_pf);
    
    void * glrc = wglCreateContext(dc);
    return(glrc);
}

static void opengl_preinit(void *instance) {
    // Get the basic wgl functions first:
    opengl_lib = LoadLibraryW((u16 *)L"opengl32.dll");
    if(opengl_lib) {
        wglGetProcAddress = (wglGetProcAddress_t *)GetProcAddress(opengl_lib, "wglGetProcAddress");
        wglMakeCurrent = (wglMakeCurrent_t *)GetProcAddress(opengl_lib, "wglMakeCurrent");
        wglDeleteContext = (wglDeleteContext_t *)GetProcAddress(opengl_lib, "wglDeleteContext");
        wglCreateContext = (wglCreateContext_t *)GetProcAddress(opengl_lib, "wglCreateContext");
    }
    
    // We're creating a dummy window to request GL extensions and get a 3.0 context.
    WNDCLASS wnd_class = {0};
    wnd_class.style = CS_OWNDC;
    wnd_class.lpfnWndProc = DefWindowProcW;
    wnd_class.hInstance = instance;
    wnd_class.hbrBackground = 0;
    wnd_class.lpszClassName = (u16 *)L"dummy wndclass";
    
    if(RegisterClassW(&wnd_class)) {
        
        void *wnd_handle = CreateWindowExW(0,
                                           wnd_class.lpszClassName,
                                           (u16 *)L"a genesis thing",
                                           WS_OVERLAPPEDWINDOW,
                                           CW_USEDEFAULT, CW_USEDEFAULT,
                                           CW_USEDEFAULT, CW_USEDEFAULT,
                                           0, 0,
                                           instance,
                                           0);
        
        if(wnd_handle) {
            void * dc = GetDC(wnd_handle);
            
            { // Setting the dummy pixel format, creating the dummy context.
                
                void * glrc = old_glrc(dc);
                wglMakeCurrent(dc, glrc);
                
                wglGetExtensionsStringEXT = (wglGetExtensionsStringEXT_t *)wglGetProcAddress("wglGetExtensionsStringEXT");
                const u8 *extensions_string = wglGetExtensionsStringEXT();
                bool pixel_format_available = false;
                bool swap_control_available = false;
                bool framebuffer_srgb_available = false;
                string wgl_arb_pixel_format_string = STRING("WGL_ARB_pixel_format");
                string wgl_ext_swap_control_string = STRING("WGL_EXT_swap_control");
                string wgl_ext_framebuffer_srgb_string = STRING("WGL_EXT_framebuffer_sRGB");
                while(*extensions_string) {
                    if(str_is_at_start_of(wgl_arb_pixel_format_string, (char *)extensions_string)) {
                        pixel_format_available = true;
                    } else if(str_is_at_start_of(wgl_ext_swap_control_string, (char *)extensions_string)) {
                        swap_control_available = true;
                    } else if(str_is_at_start_of(wgl_ext_framebuffer_srgb_string, (char *)extensions_string)) {
                        framebuffer_srgb_available = true;
                    }
                    
                    while(extensions_string[1] && (*extensions_string != ' ')) {
                        ++extensions_string;
                    }
                    ++extensions_string;
                }
                
                if(pixel_format_available) {
                    wglChoosePixelFormatARB = (wglChoosePixelFormatARB_t *)wglGetProcAddress("wglChoosePixelFormatARB");
                    wglCreateContextAttribsARB = (wglCreateContextAttribsARB_t *)wglGetProcAddress("wglCreateContextAttribsARB");
                }
                if(swap_control_available) {
                    wglSwapIntervalEXT = (wglSwapIntervalEXT_t *)wglGetProcAddress("wglSwapIntervalEXT");
                }
                /* Relevant extensions:
                - WGL_ARB_pixel_format: GetPixelFormatAttrib, ChoosePixelFormat.
                - WGL_EXT_swap_control: wglSwapIntervalEXT, wglGetSwapIntervalEXT.
                - WGL_EXT_framebuffer_sRGB: GL_FRAMEBUFFER_SRGB, FRAMEBUFFER_SRGB_CAPABLE.
                */
                
                wglMakeCurrent(0, 0);
                wglDeleteContext(glrc);
            }
            ReleaseDC(wnd_handle, dc);
            DestroyWindow(wnd_handle);
        }
    }
}

static void opengl_init(void * dc) {
    
    void * glrc = 0;
    s32 pf_index = 0;
    u32 pf_count = 0;
    if(wglChoosePixelFormatARB) {
        
        s32 i_attribs[] = {
            WGL_DRAW_TO_WINDOW_ARB, TRUE,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_SUPPORT_OPENGL_ARB, TRUE,
            WGL_DOUBLE_BUFFER_ARB, TRUE,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, TRUE,
            0,
        };
        wglChoosePixelFormatARB(dc, i_attribs, 0, 1, &pf_index, &pf_count);
    }
    else {
        pf_index = old_pf_index(dc);
    }
    PIXELFORMATDESCRIPTOR given_pf;
    DescribePixelFormat(dc,
                        pf_index,
                        sizeof(PIXELFORMATDESCRIPTOR),
                        &given_pf);
    SetPixelFormat(dc, pf_index, &given_pf);
    
    if(wglCreateContextAttribsARB) {
        
        s32 context_attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 0,
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#if GENESIS_DEV
                |WGL_CONTEXT_DEBUG_BIT_ARB
#endif
                ,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, //WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            0, // This terminates the list
        };
        glrc = wglCreateContextAttribsARB(dc, 0, context_attribs);
    }
    
    if(!glrc) {
        
        glrc = old_glrc(dc);
    }
    wglMakeCurrent(dc, glrc);
    
    wglSwapIntervalEXT(1);
    
    window_was_resized_by_platform = true;
}

static int win_game_key_from_scancode(sptr l_param) {
    
    int game_key = GK_UNHANDLED;
    u16 scancode = (l_param >> 16) & 0xFF;
    bool extended = (l_param & (1 << 24)) != 0;
    if(!extended && (scancode < ARRAY_LENGTH(winsc_to_gk))) {
        ASSERT(scancode < ARRAY_LENGTH(winsc_to_gk));
        game_key = winsc_to_gk[scancode];
    } else {
        switch(scancode) {
            case WINSC_ENTER: {game_key = GK_NUMERICENTER;} break;
            case WINSC_LCTRL: {game_key = GK_RCTRL;} break;
            case WINSC_NUMERICASTERISK: {game_key = GK_PRINTSCREEN;} break;
            case WINSC_LALT: {game_key = GK_RALT;} break;
            case WINSC_NUMERIC7: {game_key = GK_HOME;} break;
            case WINSC_NUMERIC8: {game_key = GK_UPARROW;} break;
            case WINSC_NUMERIC9: {game_key = GK_PAGEUP;} break;
            case WINSC_NUMERIC4: {game_key = GK_LEFTARROW;} break;
            case WINSC_NUMERIC6: {game_key = GK_RIGHTARROW;} break;
            case WINSC_NUMERIC1: {game_key = GK_END;} break;
            case WINSC_NUMERIC2: {game_key = GK_DOWNARROW;} break;
            case WINSC_NUMERIC3: {game_key = GK_PAGEDOWN;} break;
            case WINSC_NUMERIC0: {game_key = GK_INSERT;} break;
            case WINSC_NUMERICPERIOD: {game_key = GK_DELETE;} break;
            case WINSC_IGNORED5: {game_key = GK_LSUPER;} break;
            case WINSC_IGNORED6: {game_key = GK_RSUPER;} break;
            case WINSC_IGNORED7: {game_key = GK_APPLICATION;} break;
            case WINSC_NUMLOCK: {game_key = GK_NUMLOCK;} break;
            default: UNHANDLED;
        }
    }
    return(game_key);
}

static void win_sample_input(void * wnd_handle) {
    MSG msg;
    s32 dm[2] = {};
    
    {
        program_state.last_key_pressed = GK_UNHANDLED;
        // We never get events to tell us these aren't down anymore, so we need
        // to handle them explicitly.
        process_key(program_state.input_settings.bindings, &program_state.input, GK_MOUSEWHEELDOWN, false);
        process_key(program_state.input_settings.bindings, &program_state.input, GK_MOUSEWHEELUP  , false);
    }
    
    while(PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
        switch(msg.message) {
            case WM_INPUT: {
                u32 size = 0;
                GetRawInputData((void *)msg.lParam,
                                RID_INPUT,
                                0,
                                &size,
                                sizeof(RAWINPUTHEADER));
                RAWINPUT rawin;
                GetRawInputData((void *)msg.lParam,
                                RID_INPUT,
                                &rawin,
                                &size,
                                sizeof(RAWINPUTHEADER));
                if(rawin.header.dwType == RIM_TYPEMOUSE) {
                    bool down;
                    u32 game_key = GK_UNHANDLED;
                    RAWMOUSE mouse = rawin.data.mouse;
                    u16 button = mouse.usButtonFlags;
                    if(button) {
                        if(button < RI_MOUSE_BUTTON_2_DOWN) {
                            game_key = GK_LEFTCLICK;
                            down = (button == RI_MOUSE_BUTTON_1_DOWN);
                        } else if(button < RI_MOUSE_BUTTON_3_DOWN) {
                            game_key = GK_RIGHTCLICK;
                            down = (button == RI_MOUSE_BUTTON_2_DOWN);
                        } else if(button < RI_MOUSE_BUTTON_4_DOWN) {
                            game_key = GK_MIDDLECLICK;
                            down = (button == RI_MOUSE_BUTTON_3_DOWN);
                        } else if(button < RI_MOUSE_BUTTON_5_DOWN) {
                            game_key = GK_MOUSE4;
                            down = (button == RI_MOUSE_BUTTON_4_DOWN);
                        } else if(button < RI_MOUSE_WHEEL) {
                            game_key = GK_MOUSE5;
                            down = (button == RI_MOUSE_BUTTON_5_DOWN);
                        } else {
                            // @Untested: Check which one is which!
                            down = true;
                            s32 wheel_delta = mouse.usButtonData;
                            if(wheel_delta < 0) {
                                game_key = GK_MOUSEWHEELDOWN;
                            } else {
                                ASSERT (wheel_delta > 0);
                                game_key = GK_MOUSEWHEELUP;
                            }
                        }
                        
                        if(game_key != GK_UNHANDLED) {
                            process_key(program_state.input_settings.bindings, &program_state.input, game_key, down);
                            if(down) {
                                program_state.last_key_pressed = game_key;
                            }
                        }
                    }
                    
                    if(mouse.usFlags == MOUSE_MOVE_RELATIVE) {
                        dm[0] += mouse.lLastX;
                        dm[1] -= mouse.lLastY;
                    } else {
                        UNHANDLED;
                    }
                }
                else {
                    UNHANDLED;
                }
            } break;
            
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                s32 game_key = GK_UNHANDLED;
                u16 scancode = (msg.lParam >> 16) & 0xFF;
                // We haven't checked for extension yet, so WINSC_RALT is also handled here.
                if(scancode == WINSC_LALT) {
                    alt_is_down = true;
                }
                // Same here: WINSC_NUMERICENTER is handled by not extending.
                if((scancode == WINSC_ENTER) && alt_is_down) {
                    program_state.should_be_fullscreen = !program_state.should_be_fullscreen;
                } else {
                    game_key = win_game_key_from_scancode(msg.lParam);
                    if(game_key != GK_UNHANDLED) {
                        process_key(program_state.input_settings.bindings, &program_state.input, game_key, true);
                        program_state.last_key_pressed = game_key;
                    }
                }
            } break;
            
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                u16 scancode = (msg.lParam >> 16) & 0xFF;
                if(scancode == WINSC_LALT) {
                    alt_is_down = false;
                }
                
                u32 game_key = win_game_key_from_scancode(msg.lParam);
                if(game_key != GK_UNHANDLED) {
                    process_key(program_state.input_settings.bindings, &program_state.input, game_key, false);
                }
            } break;
            
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    move_mouse(program_state.input_settings.mouse_sensitivity, &program_state.input, dm);
    
    get_cursor_position_in_pixels(&program_state.cursor_position_in_pixels[0], &program_state.cursor_position_in_pixels[1]);
}

static bool file_exists(u16 *name) {
    s32 result = PathFileExistsW(name);
    return (result != 0);
}

static void win_unload_game_code(Game_Interface *gi) {
    if(gi->os_handle) {
        FreeLibrary((void *)gi->os_handle);
        gi->run_frame = 0;
    }
}

// @Port to string.cpp?
static void concatenate_utf16(u16 *a, u32 a_count,
                              u16 *b, u32 b_count,
                              u16 *dest, u32 dest_max_length) {
    
    if(a_count + b_count < dest_max_length) {
        u32 i;
        for(i = 0; i < a_count; ++i) {
            *dest++ = *a++;
        }
        for(i = 0; i < b_count; ++i) {
            *dest++ = *b++;
        }
        *dest = 0;
    }
}

static OS_BEGIN_SOUND_UPDATE(win_begin_sound_update) {
    
    u32 play_cur;
    u32 write_cur;
    if(global_secondary_buffer->GetCurrentPosition(&play_cur, &write_cur) == DS_OK) {
        
        u32 buffer_bytes = global_secondary_buffer_info.buffer_bytes;
        
        global_secondary_buffer_info.samples_played = global_secondary_buffer_info.samples_played % global_secondary_buffer_info.buffer_samples;
        ASSERT(global_secondary_buffer_info.samples_played < global_secondary_buffer_info.buffer_samples);
        
        // @Hack.
        static bool startup_hack = false;
        if(!startup_hack) {
            global_secondary_buffer_info.samples_played = play_cur/AUDIO_BYTES_PER_SAMPLE;
            startup_hack = true;
        }
        
        u32 lock_byte = global_secondary_buffer_info.samples_played * AUDIO_BYTES_PER_SAMPLE;
        
        u32 target_cur = (u32)(write_cur + (AUDIO_HZ*AUDIO_BYTES_PER_SAMPLE*0.1))%buffer_bytes;
        // Tested DSound minimum latency (from play to write cursor) was ~30ms.
        u32 write_bytes = 0;
        if(lock_byte > target_cur) {
            
            write_bytes = target_cur + buffer_bytes - lock_byte;
        }
        else if(lock_byte < target_cur) {
            
            write_bytes = target_cur - lock_byte;
        }
        
        s32 dserr = global_secondary_buffer->Lock(lock_byte, write_bytes,
                                                  (void **)(&global_secondary_buffer_info.fill_sec_1), &global_secondary_buffer_info.fill_bytes_1,
                                                  (void **)(&global_secondary_buffer_info.fill_sec_2), &global_secondary_buffer_info.fill_bytes_2, 0);
        if(dserr == DS_OK) {
            ASSERT(write_bytes);
            ASSERT((global_secondary_buffer_info.fill_bytes_1 % 4) == 0);
            ASSERT((global_secondary_buffer_info.fill_bytes_2 % 4) == 0);
            
            u32 samples_to_update = (global_secondary_buffer_info.fill_bytes_1 + global_secondary_buffer_info.fill_bytes_2) / AUDIO_BYTES_PER_SAMPLE;
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
    u32 fill_1 = global_secondary_buffer_info.fill_bytes_1;
    u32 fill_2 = global_secondary_buffer_info.fill_bytes_2;
    s16 *out_1 = global_secondary_buffer_info.fill_sec_1;
    s16 *out_2 = global_secondary_buffer_info.fill_sec_2;
    s16 *in_1 = input_buffer;
    s16 *in_2 = input_buffer + (fill_1 / sizeof(s16));
    
    if(samples_to_update) {
        mem_copy(in_1, out_1, fill_1);
        mem_copy(in_2, out_2, fill_2);
        global_secondary_buffer_info.samples_played += samples_to_update;
    }
    
    s32 dserr = global_secondary_buffer->Unlock(out_1, fill_1, out_2, fill_2);
    ASSERT(dserr == DS_OK);
}

static OS_PRINT(win_print) {
    text.data[text.length] = 0;
    OutputDebugStringA((char *)text.data);
}

static u32 WINAPI thread_entry(void *_context) {
    Implicit_Context *context = (Implicit_Context *)_context;
    
    for(;;) {
        WaitForSingleObject(global_thread_info.semaphore_handle, INFINITE);
        
        // If semaphore operations guarantee that we'll release exactly as many threads as
        // we have available jobs, then we don't need to check the validity of this increment.
        // _InterlockedIncrement is ++arg, not arg++.
        u32 ijob = ((u32)_InterlockedIncrement((volatile long *)&global_thread_info.next_available_job) - 1) % global_thread_info.job_queue_length;
        {
            ASSERT(ijob < global_thread_info.next_free_job);
            thread_job job = global_thread_info.job_queue[ijob];
            job.function(context, job.data);
        }
    }
}

static OS_ADD_THREAD_JOB(win_add_thread_job) {
    // We do need to check this increment for possibly overrunning the circular job buffer.
    u32 ijob = ((u32)_InterlockedIncrement((volatile long *)&global_thread_info.next_free_job) - 1) % global_thread_info.job_queue_length;
    {
        // We use a one slot safety window.
        ASSERT((ijob + 1) != global_thread_info.next_available_job);
        thread_job job;
        job.function = function;
        job.data = data;
        global_thread_info.job_queue[ijob] = job;
        _WriteBarrier();
        _mm_sfence();
        ReleaseSemaphore(global_thread_info.semaphore_handle, 1, 0);
    }
}

static Game_Interface win_load_game_code(u16 *built_dll_name,
                                         u16 *loaded_dll_name) {
    
    Game_Interface result = {0};
    
    // We can load the newly-built DLL normally.
    DeleteFileW(loaded_dll_name);
    MoveFileW(built_dll_name, loaded_dll_name);
    void *os_handle = (void *)LoadLibraryW(loaded_dll_name);
    if(os_handle) {
        
        game_get_api get_api_proc = (game_get_api)GetProcAddress((void *)os_handle, 
                                                                 "g_get_api");
        if(get_api_proc) {
            
            OS_Export win_export;
            win_export.os.get_seconds = win_get_seconds;
            win_export.os.mem_alloc = win_mem_alloc;
            win_export.os.open_file = win_open_file_utf8;
            win_export.os.close_file = win_close_file;
            win_export.os.read_file = win_read_file;
            win_export.os.file_size = win_file_size;
            win_export.os.read_entire_file = win_read_entire_file;
            win_export.os.write_entire_file = win_write_entire_file;
            win_export.os.begin_sound_update = win_begin_sound_update;
            win_export.os.end_sound_update = win_end_sound_update;
            win_export.os.capture_cursor = win_capture_cursor;
            win_export.os.release_cursor = win_release_cursor;
            win_export.os.print = win_print;
            win_export.os.add_thread_job = win_add_thread_job;
            
            //
            // @Temporary @Debug
            //
            
#if GENESIS_BUILD_ASSETS_ON_STARTUP
            win_export.os.debug_asset_directory = STRING("W:\\genesis\\assets\\");
#endif
            
            if(opengl_lib) {
                u32 i;
                for(i = 0; i < ARRAY_LENGTH(opengl_base_functions); ++i) {
                    win_export.opengl.base_functions[i] = GetProcAddress(opengl_lib, opengl_base_functions[i]);
                }
                for(i = 0; i < ARRAY_LENGTH(opengl_extended_functions); ++i) {
                    win_export.opengl.extended_functions[i] = wglGetProcAddress(opengl_extended_functions[i]);
                }
            }
            
            result = get_api_proc(&win_export);
            result.os_handle = os_handle;
        }
    }
    return(result);
}

static Implicit_Context *make_implicit_context(Memory_Block *block, u32 thread_index) {
    Implicit_Context *context = push_struct(block, Implicit_Context, 64);
    context->thread_index = thread_index;
    // @Hardcoded: Cache line size.
    sub_block(&context->temporary_memory, block, KiB(64), 64);
    return context;
}

static Memory_Block allocate_memory_block(usize size) {
    Memory_Block block;
    
    block.used = 0;
    block.size = size;
    block.mem = (u8 *)win_mem_alloc(size);
    
    return block;
}

s32 WINAPI WinMain(void *hInstance, void *hPrevInstance, char *lpCmdLine, int nShowCmd) {
    timeBeginPeriod(1);
    
    u16 exe_directory[MAX_PATH] = {};
    u16 exe_full_path[MAX_PATH] = {};
    u32 exe_full_path_length = GetModuleFileNameW(0, exe_full_path, ARRAY_LENGTH(exe_full_path));
    u32 current_directory_name_length = 0;
    for(u32 i = exe_full_path_length - 1; i >= 0; --i) {
        if(exe_full_path[i] == '\\') {
            current_directory_name_length = i + 1;
            break;
        }
    }
    mem_copy(exe_full_path, exe_directory, sizeof(u16) * current_directory_name_length);
    u16 *built_dll_suffix = (u16 *)L"genesis.dll";
    u16 *loaded_dll_suffix = (u16 *)L"genesis_loaded.dll";
    u16 built_dll_full_path[MAX_PATH];
    u16 loaded_dll_full_path[MAX_PATH];
    concatenate_utf16(exe_full_path, current_directory_name_length,
                      built_dll_suffix, ARRAY_LENGTH(L"genesis.dll"),
                      built_dll_full_path, MAX_PATH);
    concatenate_utf16(exe_full_path, current_directory_name_length,
                      loaded_dll_suffix, ARRAY_LENGTH(L"genesis_loaded.dll"),
                      loaded_dll_full_path, MAX_PATH);
    
    {
        s32 success = SetCurrentDirectoryW(exe_directory);
        ASSERT(success);
    }
    
    cpu_freq = get_cpu_freq();
    inv_cpu_freq = 1.0 / (f64)cpu_freq;
    base_time = get_cpu_time();
    
    // @Incomplete: We should use WNDCLASSEX when we can provide a small icon.
    WNDCLASS wnd_class = {0};
    wnd_class.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wnd_class.lpfnWndProc = wnd_proc;
    wnd_class.hInstance = hInstance;
    wnd_class.hCursor = LoadCursorW(0, IDC_CROSS);
    wnd_class.hbrBackground = GetStockObject(BLACK_BRUSH);
    wnd_class.lpszClassName = (u16 *)L"genesis wndclass";
    
    if(RegisterClassW(&wnd_class)) {
        void *wnd_handle = CreateWindowExW(0, //WS_EX_OVERLAPPEDWINDOW,
                                           wnd_class.lpszClassName,
                                           (u16 *)L"a genesis thing",
                                           WS_OVERLAPPEDWINDOW,
                                           CW_USEDEFAULT, CW_USEDEFAULT,
                                           CW_USEDEFAULT, CW_USEDEFAULT,
                                           0, 0,
                                           hInstance,
                                           0);
        
        global_window_handle = wnd_handle;
        if(wnd_handle) {
            //
            // Sound:
            //
            
            { // @Hardcoded :HardwareConfig
                program_state.audio_output_rate     = AUDIO_HZ;
                program_state.audio_output_channels = AUDIO_CHANNELS;
            }
            
            dsound_init(wnd_handle);
            global_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);
            
            dsound_buffer audio = {0};
            global_secondary_buffer_info.buffer_samples = AUDIO_HZ;
            global_secondary_buffer_info.buffer_bytes = global_secondary_buffer_info.buffer_samples * AUDIO_BYTES_PER_SAMPLE;
            
            if(global_secondary_buffer->Lock(0, global_secondary_buffer_info.buffer_bytes,
                                             (void **)(&global_secondary_buffer_info.fill_sec_1), &global_secondary_buffer_info.fill_bytes_1,
                                             (void **)(&global_secondary_buffer_info.fill_sec_2), &global_secondary_buffer_info.fill_bytes_2, 0) == DS_OK) {
                global_secondary_buffer_info.start_of_buffer = global_secondary_buffer_info.fill_sec_1;
                global_secondary_buffer->Unlock(global_secondary_buffer_info.fill_sec_1, global_secondary_buffer_info.fill_bytes_1,
                                                global_secondary_buffer_info.fill_sec_2, global_secondary_buffer_info.fill_bytes_2);
            } else {
                UNHANDLED; // Device was disconnected?
            }
            
            void * dc = GetDC(wnd_handle);
            
            //
            // OpenGL:
            //
            
            {
                opengl_preinit(hInstance);
                opengl_init(dc);
            }
            
            //
            // Raw input:
            //
            
            RAWINPUTDEVICE rawinput;
            
            // mouse:
            rawinput.usUsagePage = 0x01;
            rawinput.usUsage = 0x02;
            rawinput.dwFlags = 0;
            rawinput.hwndTarget = wnd_handle;
            
            /* gamepad:
            rawinput[2].usUsagePage = 0x01;
            rawinput[2].usUsage = 0x05;
            rawinput[2].dwFlags = 0;
            rawinput[2].hwndTarget = wnd_handle; */
            
            RegisterRawInputDevices(&rawinput, 1, sizeof(rawinput));
            
            //
            // Memory:
            //
            
            Memory_Block main_block = allocate_memory_block(GiB(2));
            
            //
            // Threads:
            //
            
            { // Get the number of cores on this machine:
                SYSTEM_INFO system_info = {};
                // @Incomplete: This might not work with WOW64 processes?
                GetSystemInfo(&system_info);
                program_state.core_count = system_info.dwNumberOfProcessors;
            }
            
            Implicit_Context *main_thread_context = make_implicit_context(&main_block, 0);
            
            { // Thread init.
                // Semaphores:
                // WaitForSingleObject makes the thread sleep until count is >0.
                // Every time a thread wakes up, count is atomically decremented.
                
                s32 initial_count = 0;
                s32 thread_count = program_state.core_count;
                s32 job_queue_length = 64;
                thread_job *job_queue = push_array(&main_block, thread_job, job_queue_length);
                void *semaphore_handle = CreateSemaphoreW(0, initial_count, job_queue_length, 0);
                ASSERT(semaphore_handle);
                
                global_thread_info.thread_count = thread_count;
                global_thread_info.job_queue_length = job_queue_length;
                global_thread_info.job_queue = job_queue;
                global_thread_info.semaphore_handle = semaphore_handle;
                
                for(s32 i = 1; i < thread_count; ++i) {
                    Implicit_Context *context = make_implicit_context(&main_block, i);
                    void *thread_handle = CreateThread(0, 0x100000, thread_entry, context, 0, 0);
                    CloseHandle(thread_handle);
                }
            }
            
            Memory_Block game_block;
            sub_block(&game_block, &main_block, GiB(1));
            
            Memory_Block render_queue;
            sub_block(&render_queue, &main_block, MiB(512));
            
            //
            // Game code loading:
            //
            
            Game_Interface g_interface;
            if(!file_exists(built_dll_full_path)) {
                if(file_exists(loaded_dll_full_path)) {
                    MoveFileW(loaded_dll_full_path, built_dll_full_path);
                } else UNHANDLED;
            }
            g_interface = win_load_game_code(built_dll_full_path, loaded_dll_full_path);
            g_interface.init_mem(main_thread_context, &game_block, &program_state);
            
            //
            // Finishing up init:
            //
            
            {
                RECT client_rect;
                RECT window_rect;
                
                GetClientRect(wnd_handle, &client_rect);
                GetWindowRect(wnd_handle, &window_rect);
                
                window_border_added_width  = window_rect. right - window_rect.left - client_rect. right;
                window_border_added_height = window_rect.bottom - window_rect. top - client_rect.bottom;
            }
            
            get_window_dim(wnd_handle, program_state.window_size);
            ShowWindow(wnd_handle, SW_SHOW);
            
            // First frame timing:
            LARGE_INTEGER frame_start_time;
            QueryPerformanceCounter(&frame_start_time);
            for(;;) {
                set_seconds();
                if(file_exists(built_dll_full_path)) {
                    win_unload_game_code(&g_interface);
                    g_interface = win_load_game_code(built_dll_full_path, loaded_dll_full_path);
                }
                
                // Updating the window size:
                if(program_state.should_render) { // If we're minimized, don't bother.
                    bool window_was_resized_at_all = window_was_resized_by_platform;
                    
                    if(currently_fullscreen != program_state.should_be_fullscreen) {
                        toggle_fullscreen(wnd_handle);
                        window_was_resized_by_platform = true;
                        window_was_resized_at_all = true;
                        
                        currently_fullscreen = program_state.should_be_fullscreen;
                    }
                    
                    if(!window_was_resized_by_platform) {
                        s32 window_dim[2];
                        get_window_dim(wnd_handle, window_dim);
                        if(mem_compare(window_dim, program_state.window_size, sizeof(s32[2]))) {
                            // The game changed the window size.
                            program_state.should_be_fullscreen = false;
                            
                            s32 desired_width  = program_state.window_size[0] + window_border_added_width;
                            s32 desired_height = program_state.window_size[1] + window_border_added_height;
                            
                            SetWindowPos(wnd_handle, 0,
                                         0, 0, desired_width, desired_height,
                                         SWP_DRAWFRAME|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_SHOWWINDOW);
                            window_was_resized_at_all = true;
                        }
                    }
                    
                    if(window_was_resized_at_all) {
                        // Our window has already been resized, so just update the program state.
                        get_window_dim(wnd_handle, program_state.window_size);
                        window_was_resized_by_platform = false;
                    }
                }
                
                if(input_should_reload) {
                    clear_input_state(&program_state.input);
                    input_should_reload = false;
                }
                
                win_sample_input(wnd_handle);
                
                if(g_interface.run_frame) {
                    g_interface.run_frame(main_thread_context, &game_block, &program_state);
                }
                
                if(cursor_is_being_captured) {
                    center_cursor(wnd_handle);
                }
                
                SwapBuffers(dc);
                
                {
                    // Frame timing
                    LARGE_INTEGER frame_end_time;
                    QueryPerformanceCounter(&frame_end_time);
                    f64 frame_ms = (((f64)(frame_end_time.QuadPart - frame_start_time.QuadPart) * 1000.0) *
                                    inv_cpu_freq);
                    
                    QueryPerformanceCounter(&frame_end_time);
                    frame_ms = (((f64)(frame_end_time.QuadPart - frame_start_time.QuadPart) * 1000.0) *
                                inv_cpu_freq);
                    char log[256];
                    print(log, 256, "[OS] Frame time: %fms\n", frame_ms);
                    OutputDebugStringA(log);
                    
                    frame_start_time = frame_end_time;
                }
                
                if(program_state.should_close) {
                    ExitProcess(0);
                }
            }
        }
    }
    
    // If we get here, that means we couldn't initialise properly.
    ExitProcess(0);
}
