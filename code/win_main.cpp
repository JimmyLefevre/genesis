
#include <windows.h>
#include <dsound.h>

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#include      "basic.h"
#include        "sse.h"
#include       "keys.h"
#include       "math.h"
#include      "input.h"
#include     "string.h"
#include "os_context.h"
#include "interfaces.h"

#include    "print.cpp"
#include   "string.cpp"
#include    "input.cpp"

#include    "d3d12.cpp"
#include   "dsound.cpp"

#define UNICODE
#define _UNICODE


//
// CRT bootstrapping
//
extern "C" {
    int _fltused = 0x9875;
    
    void wWinMainCRTStartup() {
        HINSTANCE instance = GetModuleHandleW(0);
        ASSERT_TYPE_SIZES;
        
        int result = wWinMain(instance, 0, 0, 0);
    }
    
    // shlwapi.h:
    s32 PathFileExistsW(const u16 *pszPath);
}

static struct {
    bool                window_was_resized_by_platform;
    bool                input_should_reload;
    bool                cursor_is_being_captured;
    bool                cursor_was_captured_when_focus_was_lost;
    bool                alt_is_down;
    bool                currently_fullscreen;
    
    v2s                 window_dim;
    
    u64                 cpu_freq; // Not actually the cpu frequency; according to QueryPerformanceFrequency, it could be 1024 times less.
    f64                 inv_cpu_freq;
    u64                 base_time;
    
    HWND                window_handle;
} win32;

static Program_State program_state;

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

static OS_MEM_ALLOC(win_mem_alloc) {
    return VirtualAlloc(0, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
}

static inline void win_free(void *data) {
    if(data) VirtualFree(data, 0, MEM_RELEASE);
}

static inline u64 get_cpu_time() {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time.QuadPart;
}

static inline u64 get_cpu_freq() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    
    return(freq.QuadPart);
}

static inline v2s get_window_dim() {
    v2s result = {};
    RECT rect;
    GetClientRect(win32.window_handle, &rect);
    
    result.x = rect.right - rect.left;
    result.y = rect.bottom - rect.top;
    
    return result;
}

static usize win_file_size(void *file_handle) {
    LARGE_INTEGER file_size;
    if(GetFileSizeEx(file_handle, &file_size)) {
        return(file_size.QuadPart);
    }
    return(0);
}

static void *win_open_file_utf16(u16 *file_name) {
    HANDLE file_handle = CreateFileW(CAST(LPCWSTR, file_name), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    
    return file_handle;
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
// Should we even consider making this an asynchronous read on the platform's side instead of ours?
static OS_READ_FILE(win_read_file) {
    ASSERT(length <= 0xFFFFFFFF);
    u32 bytes_read = 0;
    OVERLAPPED overlapped = {0};
    overlapped.Offset = offset & 0xFFFFFFFF;
    overlapped.OffsetHigh = (u32)(offset >> 32);
    ReadFile(handle, out, (u32)length, CAST(LPDWORD, &bytes_read), &overlapped);
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
        if(ReadFile(file_handle, file_data, (u32)file_size, CAST(LPDWORD, &bytes_read), 0) &&
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
    
    HANDLE file_handle = CreateFileW(CAST(LPCWSTR, utf16filename),
                                     GENERIC_READ|GENERIC_WRITE,
                                     FILE_SHARE_READ|FILE_SHARE_WRITE,
                                     0,
                                     CREATE_ALWAYS,
                                     0,
                                     0);
    
    if(file_handle != INVALID_HANDLE_VALUE) {
        
        u32 bytes_written;
        if(WriteFile(file_handle, file_data.data, (u32)file_data.length, CAST(LPDWORD, &bytes_written), 0) &&
           (bytes_written == file_data.length)) {
            success = true;
        }
        
        CloseHandle(file_handle);
    }
    
    return(success);
}

static void center_cursor(HWND wnd_handle){
    RECT rect;
    GetWindowRect(wnd_handle, &rect);
    s32 centerx = (rect.right + rect.left) / 2;
    s32 centery = (rect.bottom + rect.top) / 2;
    
    SetCursorPos(centerx, centery);
}

static void win_capture_cursor() {
    while(ShowCursor(FALSE) >= 0)
        ;
    
    win32.cursor_is_being_captured = true;
}

static void win_release_cursor() {
    while(ShowCursor(TRUE) < 0)
        ;
    
    win32.cursor_is_being_captured = false;
}

static sptr WINAPI wnd_proc(HWND handle, u32 msg, uptr w_param, sptr l_param) {
    switch(msg) {
        case WM_SIZE: {
            switch(w_param) {
                case SIZE_MAXIMIZED:
                case SIZE_RESTORED: {
                    win32.window_was_resized_by_platform = true;
                    program_state.should_render = true;
                    win32.input_should_reload = true;
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
            if(win32.cursor_was_captured_when_focus_was_lost) {
                win_capture_cursor();
            }
            return 0;
        }
        
        case WM_KILLFOCUS: {
            program_state.should_render = false;
            win32.input_should_reload = true;
            win32.cursor_was_captured_when_focus_was_lost = win32.cursor_is_being_captured;
            win_release_cursor();
            return 0;
        }
        
        case WM_MOVE: {
            win32.input_should_reload = true;
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

static void win_sample_input(HWND wnd_handle) {
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
                GetRawInputData(CAST(HRAWINPUT, msg.lParam),
                                RID_INPUT,
                                0,
                                &size,
                                sizeof(RAWINPUTHEADER));
                RAWINPUT rawin;
                GetRawInputData(CAST(HRAWINPUT, msg.lParam),
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
                        // UNHANDLED;
                        // This happens sometimes when we have multiple monitors active.
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
                    win32.alt_is_down = true;
                }
                // Same here: WINSC_NUMERICENTER is handled by not extending.
                if((scancode == WINSC_ENTER) && win32.alt_is_down) {
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
                    win32.alt_is_down = false;
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
    
    {
        POINT cursor_p;
        
        GetCursorPos(&cursor_p);
        ScreenToClient(win32.window_handle, &cursor_p);
        
        s32 draw_rect_width = program_state.draw_rect.right - program_state.draw_rect.left;
        s32 draw_rect_height = program_state.draw_rect.top - program_state.draw_rect.bottom;
        
        program_state.cursor_position_barycentric.x = CAST(f32, cursor_p.x - program_state.draw_rect.left) / CAST(f32, draw_rect_width);
        program_state.cursor_position_barycentric.y = CAST(f32, draw_rect_height - (cursor_p.y - program_state.draw_rect.bottom)) / CAST(f32, draw_rect_height);
    }
}

static bool file_exists(u16 *name) {
    s32 result = PathFileExistsW(name);
    return (result != 0);
}

static void win_unload_game_code(Game_Interface *gi) {
    if(gi->os_handle) {
        FreeLibrary(CAST(HMODULE, gi->os_handle));
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

static OS_PRINT(win_print) {
    text.data[text.length] = 0;
    OutputDebugStringA((char *)text.data);
}

DWORD WINAPI thread_entry(void *_context) {
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

static Game_Interface win_load_game_code(u16 *built_dll_name, u16 *loaded_dll_name) {
    Game_Interface result = {0};
    
    // We can load the newly-built DLL normally.
    DeleteFileW(CAST(LPCWSTR, loaded_dll_name));
    MoveFileW(CAST(LPCWSTR, built_dll_name), CAST(LPCWSTR, loaded_dll_name));
    HMODULE os_handle = LoadLibraryW(CAST(LPCWSTR, loaded_dll_name));
    if(os_handle) {
        game_get_api get_api_proc = (game_get_api)GetProcAddress(os_handle,  "g_get_api");
        if(get_api_proc) {
            
            OS_Export win_export;
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
            
            win_export.os.begin_frame_and_clear = begin_frame_and_clear;
            win_export.os.end_frame = end_frame;
            win_export.os.make_command_lists = make_command_lists;
            win_export.os.make_vertex_buffers = make_vertex_buffers;
            win_export.os.set_transform = set_transform;
            win_export.os.submit_commands = submit_commands;
            win_export.os.draw_mesh_instances = draw_mesh_instances;
            win_export.os.make_editable_mesh = make_editable_mesh;
            win_export.os.update_editable_mesh = update_editable_mesh;
            
            //
            // @Temporary @Debug
            //
            
#if GENESIS_BUILD_ASSETS_ON_STARTUP
            win_export.os.debug_asset_directory = STRING("W:\\genesis\\assets\\");
#endif
            
            result = get_api_proc(&win_export);
            result.os_handle = os_handle;
        }
    }
    return(result);
}

static Implicit_Context *make_implicit_context(Memory_Block *block, u32 thread_index) {
    Implicit_Context *context = push_struct(block, Implicit_Context, CACHE_LINE_SIZE);
    context->thread_index = thread_index;
    sub_block(&context->temporary_memory, block, KiB(256), CACHE_LINE_SIZE);
    return context;
}

static Memory_Block allocate_memory_block(usize size) {
    Memory_Block block;
    
    block.used = 0;
    block.size = size;
    block.mem = (u8 *)win_mem_alloc(size);
    
    return block;
}

s32 WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
    timeBeginPeriod(1);
    
    // @Cleanup: Just set the working directory.
    u16 exe_directory[MAX_PATH] = {};
    u16 exe_full_path[MAX_PATH] = {};
    u32 exe_full_path_length = GetModuleFileNameW(0, CAST(LPWSTR, exe_full_path), ARRAY_LENGTH(exe_full_path));
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
#if USE_DATAPACK
        s32 success = SetCurrentDirectoryW(CAST(LPWSTR, exe_directory));
#else
        s32 success = SetCurrentDirectoryW(L"W:\\genesis\\assets\\");
#endif
        ASSERT(success);
    }
    
    win32.cpu_freq = get_cpu_freq();
    win32.inv_cpu_freq = 1.0 / (f64)win32.cpu_freq;
    win32.base_time = get_cpu_time();
    
    WNDCLASSEXW wnd_class = {};
    wnd_class.cbSize = sizeof(WNDCLASSEXW);
    wnd_class.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wnd_class.lpfnWndProc = wnd_proc;
    wnd_class.hInstance = hInstance;
    wnd_class.hCursor = LoadCursorA(0, IDC_CROSS);
    wnd_class.hbrBackground = CAST(HBRUSH, GetStockObject(BLACK_BRUSH));
    wnd_class.lpszClassName = L"genesis wndclass";
    
    if(RegisterClassExW(&wnd_class)) {
        HWND wnd_handle = CreateWindowExW(0, //WS_EX_OVERLAPPEDWINDOW,
                                          wnd_class.lpszClassName,
                                          L"a genesis thing",
                                          WS_OVERLAPPEDWINDOW,
                                          CW_USEDEFAULT, CW_USEDEFAULT,
                                          CW_USEDEFAULT, CW_USEDEFAULT,
                                          0, 0,
                                          hInstance,
                                          0);
        
        win32.window_handle = wnd_handle;
        if(wnd_handle) {
            //
            // Sound:
            //
            
            { // @Hardcoded :HardwareConfig
                program_state.audio_output_rate     = AUDIO_HZ;
                program_state.audio_output_channels = AUDIO_CHANNELS;
            }
            
            dsound_init(wnd_handle);
            
            d3d_init(wnd_handle);
            
            /////////////////////////////////////////////////
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
            
            Game_Interface g_interface = {};
#if 1
            if(!file_exists(built_dll_full_path)) {
                if(file_exists(loaded_dll_full_path)) {
                    MoveFileW(CAST(LPCWSTR, loaded_dll_full_path), CAST(LPCWSTR, built_dll_full_path));
                } else UNHANDLED;
            }
            g_interface = win_load_game_code(built_dll_full_path, loaded_dll_full_path);
            g_interface.init_mem(main_thread_context, &game_block, &program_state);
#endif
            
            //
            // Finishing up init:
            //
            
            program_state.window_size = get_window_dim();
            ShowWindow(wnd_handle, SW_SHOW);
            
            // First frame timing:
            LARGE_INTEGER frame_start_time;
            QueryPerformanceCounter(&frame_start_time);
            for(;;) {
                
                { // @@ We might want to move this closer to run_frame.
                    // It turns out that using __rdtsc *AND* QueryPerformanceCounter together is a bad
                    // idea because, while __rdtsc always returns the CPU's timestamp, QueryPerformanceCounter
                    // compensates for QueryPerformanceFrequency sometimes being 1024 times less than it should,
                    // so we always use QueryPerformanceFrequency and QueryPerformanceCounter. Ultimately this
                    // also works better on CPUs that are likely to periodically over/underclock:
                    
                    // "The performance counter frequency that QueryPerformanceFrequency returns is determined
                    //  during system initialization and doesn't change while the system is running."
                    // See https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
                    // for more info.
                    u64 time = get_cpu_time();
                    time -= win32.base_time;
                    program_state.time = CAST(f64, time) * win32.inv_cpu_freq;
                }
                
                // Updating the window size:
                if(program_state.should_render) { // If we're minimized, don't bother.
                    if(win32.currently_fullscreen != program_state.should_be_fullscreen) {
                        d3d_set_fullscreen(program_state.should_be_fullscreen);
                        
                        win32.window_was_resized_by_platform = true;
                        
                        win32.currently_fullscreen = program_state.should_be_fullscreen;
                    }
                    
                    if(!win32.window_was_resized_by_platform) {
                        if(mem_compare(&win32.window_dim, &program_state.window_size, sizeof(v2s))) {
                            // The game changed the window size.
                            program_state.should_be_fullscreen = false;
                            
                            RECT rect;
                            
                            rect.left = 0;
                            rect.top = 0;
                            rect.right = program_state.window_size.x;
                            rect.bottom = program_state.window_size.y;
                            
                            AdjustWindowRect(&rect, wnd_class.style, 0);
                            
                            SetWindowPos(wnd_handle, 0,
                                         0, 0, rect.right - rect.left, rect.bottom - rect.top,
                                         SWP_DRAWFRAME|SWP_NOMOVE|SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_SHOWWINDOW);
                            
                            win32.window_dim = program_state.window_size;
                        }
                    } else {
                        // Our window has already been resized by the OS, so just update the program state.
                        v2s new_window_dim = get_window_dim();
                        
                        program_state.window_size = new_window_dim;
                        win32.window_dim = new_window_dim;
                        win32.window_was_resized_by_platform = false;
                        
                        d3d_resize_backbuffers(program_state.window_size);
                    }
                }
#if 1
                if(file_exists(built_dll_full_path)) {
                    win_unload_game_code(&g_interface);
                    g_interface = win_load_game_code(built_dll_full_path, loaded_dll_full_path);
                }
#endif
                
                if(win32.input_should_reload) {
                    win32.input_should_reload = false;
                    clear_input_state(&program_state.input);
                }
                
                win_sample_input(wnd_handle);
                
                if(g_interface.run_frame) {
                    g_interface.run_frame(main_thread_context, &game_block, &program_state);
                }
                
                if(win32.cursor_is_being_captured) {
                    center_cursor(wnd_handle);
                }
                
                {
                    // Frame timing
                    LARGE_INTEGER frame_end_time;
                    QueryPerformanceCounter(&frame_end_time);
                    f64 frame_ms = (((f64)(frame_end_time.QuadPart - frame_start_time.QuadPart) * 1000.0) *
                                    win32.inv_cpu_freq);
                    
#if 0
                    // @Hardcoded milliseconds per frame
                    if(CAST(u32, frame_ms) < 16) {
                        Sleep(16 - CAST(u32, frame_ms));
                    }
#endif
                    
                    QueryPerformanceCounter(&frame_end_time);
                    frame_ms = (((f64)(frame_end_time.QuadPart - frame_start_time.QuadPart) * 1000.0) *
                                win32.inv_cpu_freq);
                    
#if 1
                    char log[256];
                    print(log, 256, "[OS] Frame time: %fms\n", frame_ms);
                    OutputDebugStringA(log);
#endif
                    
                    frame_start_time = frame_end_time;
                }
                
                if(program_state.should_close) {
                    if(win32.currently_fullscreen) {
                        d3d_set_fullscreen(false);
                    }
                    
                    ExitProcess(0);
                }
            }
        }
    }
    
    // If we get here, that means we couldn't initialise properly.
    ExitProcess(1);
}
