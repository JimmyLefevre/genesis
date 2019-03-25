/*
https://docs.microsoft.com/en-us/windows/desktop/winprog/windows-data-types
HANDLE is void *
HINSTANCE is HANDLE
HWND is HANDLE
HDC is HANDLE
HGDIOBJ is HANDLE
HGLRC is HANDLE
HMODULE is HANDLE
HINSTANCE is HANDLE
HICON is HANDLE
HCURSOR is HANDLE
HBRUSH is HANDLE
HRAWINPUT is HANDLE
HRESULT is HANDLE
BOOL is int
LPCTSTR is wchar_t * for UNICODE, char * otherwise.
LPCSTR is char *.
PURE is =0
*/

#define WINAPI __stdcall
#define INVALID_HANDLE_VALUE ((void *)(-1))
#define FALSE 0
#define TRUE 1
#define CW_USEDEFAULT ((int)0x80000000)
#define MAX_PATH 260

#define IDC_APPSTARTING ((u16 *)32650)
#define IDC_ARROW ((u16 *)32512)
#define IDC_CROSS ((u16 *)32515)
#define IDC_HAND ((u16 *)32649)
#define IDC_HELP ((u16 *)32651)
#define IDC_IBEAM ((u16 *)32513)
#define IDC_ICON ((u16 *)32641)
#define IDC_NO ((u16 *)32648)
#define IDC_SIZE ((u16 *)32640)
#define IDC_SIZEALL ((u16 *)32646)
#define IDC_SIZENESW ((u16 *)32643)
#define IDC_SIZENS ((u16 *)32645)
#define IDC_SIZENWSE ((u16 *)32642)
#define IDC_SIZEWE ((u16 *)32644)
#define IDC_UPARROW ((u16 *)32516)
#define IDC_WAIT ((u16 *)32514)

#define HWND_BOTTOM ((void *)1)
#define HWND_NOTOPMOST ((void *)(-2))
#define HWND_TOP ((void *)0)
#define HWND_TOPMOST ((void *)(-1))

#define GENERIC_EXECUTE ((u32)(1 << 29))
#define GENERIC_WRITE ((u32)(1 << 30))
#define GENERIC_READ ((u32)(1 << 31))

#define INFINITE 0xFFFFFFFF

// @Robustness: How do we add __stdcall to this typedef?
typedef sptr (WINAPI *WNDPROC)(void *, u32, uptr, sptr);

enum Window_Message {
    WM_CREATE = 0x0001,
    WM_DESTROY = 0x0002,
    WM_MOVE = 0x0003,
    WM_SIZE = 0x0005,
    WM_ACTIVATE = 0x0006,
    WM_SETFOCUS = 0x0007,
    WM_KILLFOCUS = 0x0008,
    WM_PAINT = 0x000F,
    WM_CLOSE = 0x0010,
    WM_QUIT = 0x0012,
    WM_SHOWWINDOW = 0x0018,
    WM_ACTIVATEAPP = 0x001C,
    WM_SETCURSOR = 0x0020,
    WM_MOUSEACTIVATE = 0x0021,
    WM_INPUT = 0x00FF,
    WM_KEYDOWN = 0x0100,
    WM_KEYUP = 0x0101,
    WM_CHAR = 0x0102,
    WM_SYSKEYDOWN = 0x0104,
    WM_SYSKEYUP = 0x0105,
    WM_SYSCOMMAND = 0x0112,
};

enum Stock_Object {
    BLACK_BRUSH = 4,
    BLACK_PEN = 7,
};

enum Raw_Input_Data {
    RID_INPUT = 0x10000003,
    RID_HEADER = 0x10000005,
};

enum System_Menu_Commands {
    SC_KEYMENU = 0xF100,
};

enum Window_Resize_Param {
    SIZE_MAXHIDE = 4,
    SIZE_MAXIMIZED = 2,
    SIZE_MAXSHOW = 3,
    SIZE_MINIMIZED = 1,
    SIZE_RESTORED = 0,
};

enum Window_Long_Offset {
    GWL_EXSTYLE = -20,
    GWL_HINSTANCE = -6,
    GWL_ID = -12,
    GWL_STYLE = -16,
    GWL_USERDATA = -21,
    GWL_WNDPROC = -4,
};

enum Set_Window_Pos {
    SWP_ASYNCWINDOWPOS = 0x4000,
    SWP_DEFERERASE = 0x2000,
    SWP_DRAWFRAME = 0x0020,
    SWP_FRAMECHANGED = 0x0020,
    SWP_HIDEWINDOW = 0x0080,
    SWP_NOACTIVATE = 0x0010,
    SWP_NOCOPYBITS = 0x0100,
    SWP_NOMOVE = 0x0002,
    SWP_NOOWNERZORDER = 0x0200,
    SWP_NOREDRAW = 0x0008,
    SWP_NOREPOSITION = 0x0200,
    SWP_NOSENDCHANGING = 0x0400,
    SWP_NOSIZE = 0x0001,
    SWP_NOZORDER = 0x0004,
    SWP_SHOWWINDOW = 0x0040,
};

enum Memory_Usage {
    // Alloc:
    MEM_COMMIT = 0x00001000,
    MEM_RESERVE = 0x00002000,
    MEM_RESET = 0x00080000,
    MEM_RESET_UNDO = 0x01000000,
    
    // Optional:
    MEM_LARGE_PAGES = 0x20000000,
    MEM_PHYSICAL = 0x00400000,
    MEM_TOP_DOWN = 0x00100000,
    MEM_WRITE_WATCH = 0x00200000,
    
    // Free:
    MEM_DECOMMIT = 0x4000,
    MEM_RELEASE = 0x8000,
};

enum Page_Usage {
    PAGE_EXECUTE = 0x10,
    PAGE_EXECUTE_READ = 0x20,
    PAGE_EXECUTE_READWRITE = 0x40,
    PAGE_EXECUTE_WRITECOPY = 0x80,
    PAGE_NOACCESS = 0x01,
    PAGE_READONLY = 0x02,
    PAGE_READWRITE = 0x04,
    PAGE_WRITECOPY = 0x08,
    PAGE_TARGETS_INVALID = 0x40000000,
    PAGE_TARGETS_NO_UPDATE = 0x40000000,
    
    // Modifiers:
    PAGE_GUARD = 0x100,
    PAGE_NOCACHE = 0x200,
    PAGE_WRITECOMBINE = 0x400,
};

enum Window_Style {
    WS_BORDER = 0x00800000L,
    WS_CAPTION = 0x00C00000L,
    WS_CHILD = 0x40000000L,
    WS_CHILDWINDOW = 0x40000000L,
    WS_CLIPCHILDREN = 0x02000000L,
    WS_CLIPSIBLINGS = 0x04000000L,
    WS_DISABLED = 0x08000000L,
    WS_DLGFRAME = 0x00400000L,
    WS_GROUP = 0x00020000L,
    WS_HSCROLL = 0x00100000L,
    WS_ICONIC = 0x20000000L,
    WS_MAXIMIZE = 0x01000000L,
    WS_MAXIMIZEBOX = 0x00010000L,
    WS_MINIMIZE = 0x20000000L,
    WS_MINIMIZEBOX = 0x00020000L,
    WS_OVERLAPPED = 0x00000000L,
    WS_POPUP = 0x80000000L,
    WS_SIZEBOX = 0x00040000L,
    WS_SYSMENU = 0x00080000L,
    WS_TABSTOP = 0x00010000L,
    WS_THICKFRAME = 0x00040000L,
    WS_TILED = 0x00000000L,
    WS_VISIBLE = 0x10000000L,
    WS_VSCROLL = 0x00200000L,
    WS_POPUPWINDOW = (WS_POPUP|WS_BORDER|WS_SYSMENU),
    WS_OVERLAPPEDWINDOW = (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX),
    WS_TILEDWINDOW = (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX),
};

enum Extended_Window_Style {
    WS_EX_ACCEPTFILES = 0x00000010L,
    WS_EX_APPWINDOW = 0x00040000L,
    WS_EX_CLIENTEDGE = 0x00000200L,
    WS_EX_COMPOSITED = 0x02000000L,
    WS_EX_CONTEXTHELP = 0x00000400L,
    WS_EX_CONTROLPARENT = 0x00010000L,
    WS_EX_DLGMODALFRAME = 0x00000001L,
    WS_EX_LAYERED = 0x00080000L,
    WS_EX_LAYOUTRTL = 0x00400000L,
    WS_EX_LEFT = 0x00000000L,
    WS_EX_LEFTSCROLLBAR = 0x00004000L,
    WS_EX_LTRREADING = 0x00000000L,
    WS_EX_MDICHILD = 0x00000040L,
    WS_EX_NOACTIVATE = 0x08000000L,
    WS_EX_NOINHERITLAYOUT = 0x00100000L,
    WS_EX_NOPARENTNOTIFY = 0x00000004L,
    WS_EX_NOREDIRECTIONBITMAP = 0x00200000L,
    WS_EX_RIGHT = 0x00001000L,
    WS_EX_RIGHTSCROLLBAR = 0x00000000L,
    WS_EX_RTLREADING = 0x00002000L,
    WS_EX_STATICEDGE = 0x00020000L,
    WS_EX_TOOLWINDOW = 0x00000080L,
    WS_EX_TOPMOST = 0x00000008L,
    WS_EX_TRANSPARENT = 0x00000020L,
    WS_EX_WINDOWEDGE = 0x00000100L,
    WS_EX_OVERLAPPEDWINDOW = (WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE),
    WS_EX_PALETTEWINDOW = (WS_EX_WINDOWEDGE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST),
};

enum Monitor_Info {
    MONITOR_DEFAULTTONULL = 0x00000000,
    MONITOR_DEFAULTTOPRIMARY = 0x00000001,
    MONITOR_DEFAULTTONEAREST = 0x00000002,
    MONITORINFOF_PRIMARY = 0x00000001,
};

enum Get_Device_Caps {
    DRIVERVERSION = 0,    // Device driver version
    TECHNOLOGY = 2,       // Device classification
    HORZSIZE = 4,         // Horizontal size in millimeters
    VERTSIZE = 6,         // Vertical size in millimeters
    HORZRES = 8,          // Horizontal width in pixels
    VERTRES = 10,         // Vertical height in pixels
    BITSPIXEL = 12,       // Number of bits per pixel
    PLANES = 14,          // Number of planes
    NUMBRUSHES = 16,      // Number of brushes the device has
    NUMPENS = 18,         // Number of pens the device has
    NUMMARKERS = 20,      // Number of markers the device has
    NUMFONTS = 22,        // Number of fonts the device has
    NUMCOLORS = 24,       // Number of colors the device supports
    PDEVICESIZE = 26,     // Size required for device descriptor
    CURVECAPS = 28,       // Curve capabilities
    LINECAPS = 30,        // Line capabilities
    POLYGONALCAPS = 32,   // Polygonal capabilities
    TEXTCAPS = 34,        // Text capabilities
    CLIPCAPS = 36,        // Clipping capabilities
    RASTERCAPS = 38,      // Bitblt capabilities
    ASPECTX = 40,         // Length of the X leg
    ASPECTY = 42,         // Length of the Y leg
    ASPECTXY = 44,        // Length of the hypotenuse
    
    LOGPIXELSX = 88,      // Logical pixels/inch in X
    LOGPIXELSY = 90,      // Logical pixels/inch in Y
    
    SIZEPALETTE = 104,    // Number of entries in physical palette
    NUMRESERVED = 106,    // Number of reserved entries in palette
    COLORRES = 108,       // Actual color resolution
    VREFRESH = 116,       // Current vertical refresh rate of the display device (for displays only) in Hz
    DESKTOPVERTRES = 117, // Horizontal width of entire desktop in pixels
    DESKTOPHORZRES = 118, // Vertical height of entire desktop in pixels
    BLTALIGNMENT =   119, // Preferred blt alignment
};

enum File_Sharing {
    FILE_SHARE_DELETE = 0x00000004,
    FILE_SHARE_READ = 0x00000001,
    FILE_SHARE_WRITE = 0x00000002,
};

enum File_Creation {
    CREATE_ALWAYS = 2,
    CREATE_NEW = 1,
    OPEN_ALWAYS = 4,
    OPEN_EXISTING = 3,
    TRUNCATE_EXISTING = 5,
};

enum Pixel_Format_Descriptor_Flags {
    // pixel types
    PFD_TYPE_RGBA = 0,
    PFD_TYPE_COLORINDEX = 1,
    
    // layer types
    PFD_MAIN_PLANE = 0,
    PFD_OVERLAY_PLANE = 1,
    PFD_UNDERLAY_PLANE = (-1),
    
    // PIXELFORMATDESCRIPTOR flags
    PFD_DOUBLEBUFFER = 0x00000001,
    PFD_STEREO = 0x00000002,
    PFD_DRAW_TO_WINDOW = 0x00000004,
    PFD_DRAW_TO_BITMAP = 0x00000008,
    PFD_SUPPORT_GDI = 0x00000010,
    PFD_SUPPORT_OPENGL = 0x00000020,
    PFD_GENERIC_FORMAT = 0x00000040,
    PFD_NEED_PALETTE = 0x00000080,
    PFD_NEED_SYSTEM_PALETTE = 0x00000100,
    PFD_SWAP_EXCHANGE = 0x00000200,
    PFD_SWAP_COPY = 0x00000400,
    PFD_SWAP_LAYER_BUFFERS = 0x00000800,
    PFD_GENERIC_ACCELERATED = 0x00001000,
    PFD_SUPPORT_DIRECTDRAW = 0x00002000,
    PFD_DIRECT3D_ACCELERATED = 0x00004000,
    PFD_SUPPORT_COMPOSITION = 0x00008000,
    
    // PIXELFORMATDESCRIPTOR flags for use in ChoosePixelFormat only
    PFD_DEPTH_DONTCARE = 0x20000000,
    PFD_DOUBLEBUFFER_DONTCARE = 0x40000000,
    PFD_STEREO_DONTCARE = 0x80000000,
};

enum Class_Style {
    CS_VREDRAW = 0x0001,
    CS_HREDRAW = 0x0002,
    CS_DBLCLKS = 0x0008,
    CS_OWNDC = 0x0020,
    CS_CLASSDC = 0x0040,
    CS_PARENTDC = 0x0080,
    CS_NOCLOSE = 0x0200,
    CS_SAVEBITS = 0x0800,
    CS_BYTEALIGNCLIENT = 0x1000,
    CS_BYTEALIGNWINDOW = 0x2000,
    CS_GLOBALCLASS = 0x4000,
    
    CS_IME = 0x00010000,
};

enum Peek_Message_Options {
    PM_NOREMOVE = 0x0000,
    PM_REMOVE = 0x0001,
    PM_NOYIELD = 0x0002,
};

enum Raw_Input_Message_Type {
    RIM_TYPEMOUSE = 0,
    RIM_TYPEKEYBOARD = 1,
    RIM_TYPEHID = 2,
    RIM_TYPEMAX = 2,
};

enum Mouse_Move {
    MOUSE_MOVE_RELATIVE = 0,
    MOUSE_MOVE_ABSOLUTE = 1,
    MOUSE_VIRTUAL_DESKTOP = 0x02,
    MOUSE_ATTRIBUTES_CHANGED = 0x04,
    MOUSE_MOVE_NOCOALESCE = 0x08,
};

enum Raw_Input_Mouse_Message {
    RI_MOUSE_LEFT_BUTTON_DOWN = 0x0001,
    RI_MOUSE_LEFT_BUTTON_UP = 0x0002,
    RI_MOUSE_RIGHT_BUTTON_DOWN = 0x0004,
    RI_MOUSE_RIGHT_BUTTON_UP = 0x0008,
    RI_MOUSE_MIDDLE_BUTTON_DOWN = 0x0010,
    RI_MOUSE_MIDDLE_BUTTON_UP = 0x0020,
    
    // Aliases
    RI_MOUSE_BUTTON_1_DOWN = RI_MOUSE_LEFT_BUTTON_DOWN,
    RI_MOUSE_BUTTON_1_UP = RI_MOUSE_LEFT_BUTTON_UP,
    RI_MOUSE_BUTTON_2_DOWN = RI_MOUSE_RIGHT_BUTTON_DOWN,
    RI_MOUSE_BUTTON_2_UP = RI_MOUSE_RIGHT_BUTTON_UP,
    RI_MOUSE_BUTTON_3_DOWN = RI_MOUSE_MIDDLE_BUTTON_DOWN,
    RI_MOUSE_BUTTON_3_UP = RI_MOUSE_MIDDLE_BUTTON_UP,
    
    RI_MOUSE_BUTTON_4_DOWN = 0x0040,
    RI_MOUSE_BUTTON_4_UP = 0x0080,
    RI_MOUSE_BUTTON_5_DOWN = 0x0100,
    RI_MOUSE_BUTTON_5_UP = 0x0200,
    RI_MOUSE_WHEEL = 0x0400,
    RI_MOUSE_HWHEEL = 0x0800,
};

enum Show_Window {
    SW_HIDE = 0,
    SW_SHOWNORMAL = 1,
    SW_NORMAL = 1,
    SW_SHOWMINIMIZED = 2,
    SW_SHOWMAXIMIZED = 3,
    SW_MAXIMIZE = 3,
    SW_SHOWNOACTIVATE = 4,
    SW_SHOW = 5,
    SW_MINIMIZE = 6,
    SW_SHOWMINNOACTIVE = 7,
    SW_SHOWNA = 8,
    SW_RESTORE = 9,
    SW_SHOWDEFAULT = 10,
    SW_FORCEMINIMIZE = 11,
    SW_MAX = 11,
};

union LARGE_INTEGER {
    struct {
        u32 LowPart;
        s32 HighPart;
    } u;
    s64 QuadPart;
};

struct OVERLAPPED {
    uptr Internal;
    uptr InternalHigh;
    union {
        struct {
            u32 Offset;
            u32 OffsetHigh;
        };
        void *Pointer;
    };
    void *hEvent;
};

struct GUID {
    u32 Data1;
    u16 Data2;
    u16 Data3;
    u8  Data4[8];
};

struct POINT {
    s32 x;
    s32 y;
};

struct RECT {
    s32 left;
    s32 top;
    s32 right;
    s32 bottom;
};

struct MSG {
    void *hwnd;
    u32 message;
    uptr wParam;
    sptr lParam;
    u32 time;
    POINT  pt;
};

struct RAWINPUTHEADER {
    u32  dwType;
    u32  dwSize;
    void *hDevice;
    uptr wParam;
};

struct RAWMOUSE {
    u16 usFlags;
    union {
        u32  ulButtons;
        struct {
            u16 usButtonFlags;
            u16 usButtonData;
        };
    };
    u32  ulRawButtons;
    s32   lLastX;
    s32   lLastY;
    u32  ulExtraInformation;
};

struct RAWKEYBOARD {
    u16 MakeCode;
    u16 Flags;
    u16 Reserved;
    u16 VKey;
    u32 Message;
    u32 ExtraInformation;
};

struct RAWHID {
    u32 dwSizeHid;
    u32 dwCount;
    u8 bRawData[1];
};

struct RAWINPUT {
    RAWINPUTHEADER header;
    union {
        RAWMOUSE mouse;
        RAWKEYBOARD keyboard;
        RAWHID hid;
    } data;
};

struct RAWINPUTDEVICE {
    u16 usUsagePage;
    u16 usUsage;
    u32 dwFlags;
    void *hwndTarget;
};

struct PAINTSTRUCT {
    void *hdc;
    s32 fErase;
    RECT rcPaint;
    s32 fRestore;
    s32 fIncUpdate;
    u8 rgbReserved[32];
};

struct MONITORINFO {
    u32 cbSize;
    RECT  rcMonitor;
    RECT  rcWork;
    u32 dwFlags;
};

struct WINDOWPLACEMENT {
    u32  length;
    u32  flags;
    u32  showCmd;
    POINT ptMinPosition;
    POINT ptMaxPosition;
    RECT  rcNormalPosition;
};

struct WNDCLASS {
    u32 style;
    WNDPROC lpfnWndProc;
    s32 cbClsExtra;
    s32 cbWndExtra;
    void *hInstance;
    void *hIcon;
    void *hCursor;
    void *hbrBackground;
    u16 *lpszMenuName;
    u16 *lpszClassName;
};

struct IUnknown {
    virtual s32 QueryInterface(const GUID &riid, void **ppvObject) = 0;
    virtual u32 AddRef() = 0;
    virtual u32 Release() = 0;
    
    /*
    template<class Q> s32 QueryInterface(Q** pp) {
        return QueryInterface(__uuidof(Q), (void **)pp);
    }
    */
};

struct PIXELFORMATDESCRIPTOR {
    u16 nSize;
    u16 nVersion;
    u32 dwFlags;
    u8 iPixelType;
    u8 cColorBits;
    u8 cRedBits;
    u8 cRedShift;
    u8 cGreenBits;
    u8 cGreenShift;
    u8 cBlueBits;
    u8 cBlueShift;
    u8 cAlphaBits;
    u8 cAlphaShift;
    u8 cAccumBits;
    u8 cAccumRedBits;
    u8 cAccumGreenBits;
    u8 cAccumBlueBits;
    u8 cAccumAlphaBits;
    u8 cDepthBits;
    u8 cStencilBits;
    u8 cAuxBuffers;
    u8 iLayerType;
    u8 bReserved;
    u32 dwLayerMask;
    u32 dwVisibleMask;
    u32 dwDamageMask;
};

struct SECURITY_ATTRIBUTES {
    u32 nLength;
    void *lpSecurityDescriptor;
    s32 bInheritHandle;
};

struct SYSTEM_INFO {
    union {
        u32 dwOemId;
        struct {
            u16 wProcessorArchitecture;
            u16 wReserved;
        };
    };
    u32 dwPageSize;
    void *lpMinimumApplicationAddress;
    void *lpMaximumApplicationAddress;
    u32 *dwActiveProcessorMask;
    u32 dwNumberOfProcessors;
    u32 dwProcessorType;
    u32 dwAllocationGranularity;
    u16 wProcessorLevel;
    u16 wProcessorRevision;
};

extern "C" {
    void *WINAPI GetModuleHandleW(u16 *lpModuleName);
    s32 AdjustWindowRectEx(RECT *lpRect, u32 dwStyle, s32 bMenu, u32 dwExStyle);
    s32 SetCurrentDirectoryW(u16 *lpPathName);
    s32 WINAPI WinMain(void *hInstance, void *hPrevInstance, char *lpCmdLine, s32 nCmdShow);
    void WINAPI ExitProcess(u32 uExitCode);
    s32 WINAPI GetWindowLongW(void *hWnd, s32 nIndex);
    s32 WINAPI SetWindowLongW(void *hWnd, s32 nIndex, s32 dwNewLong);
    s32 WINAPI GetWindowPlacement(void *hWnd, WINDOWPLACEMENT *lpwndpl);
    s32 WINAPI SetWindowPos(void *hWnd, void *hWndInsertAfter, s32 X, s32 Y, s32 cx, s32 cy, u32 uFlags);
    s32 WINAPI SetWindowPlacement(void *hWnd, const WINDOWPLACEMENT *lpwndpl);
    void *WINAPI VirtualAlloc(void *lpAddress, usize dwSize, u32 flAllocationType, u32 flProtect);
    s32 WINAPI VirtualFree(void *lpAddress, usize dwSize, u32 dwFreeType);
    s32 WINAPI QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
    s32 WINAPI QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
    s32 GetDeviceCaps(void *hdc, s32 nIndex);
    s32 WINAPI GetClientRect(void *hWnd, RECT *lpRect);
    s32 WINAPI GetWindowRect(void *hWnd,RECT *lpRect);
    s32 WINAPI GetFileSizeEx(void *hFile, LARGE_INTEGER *lpFileSize);
    void *WINAPI CreateFileW(const u16 *lpFileName, u32 dwDesiredAccess, u32 dwShareMode, SECURITY_ATTRIBUTES *lpSecurityAttributes, u32 dwCreationDisposition, u32 dwFlagsAndAttributes, void *hTemplateFile);
    s32 WINAPI ReadFile(void *hFile, void *lpBuffer, u32 nNumberOfBytesToRead, u32 *lpNumberOfBytesRead, OVERLAPPED *lpOverlapped);
    s32 WINAPI CloseHandle(void *hObject);
    s32 WINAPI WriteFile(void *hFile, const void *lpBuffer, u32 nNumberOfBytesToWrite, u32 *lpNumberOfBytesWritten, OVERLAPPED *lpOverlapped);
    s32 WINAPI SetCursorPos(s32 X, s32 Y);
    s32 WINAPI GetCursorPos(POINT *lpPoint);
    s32 ScreenToClient(void *hWnd, POINT *lpPoint);
    s32 WINAPI ShowCursor(s32 bShow);
    void *BeginPaint(void *hwnd, PAINTSTRUCT *lpPaint);
    s32 EndPaint(void *hWnd, const PAINTSTRUCT *lpPaint);
    sptr WINAPI DefWindowProcW(void *hWnd, u32 Msg, uptr wParam, sptr lParam);
    void *WINAPI LoadLibrary(const u16 *lpFileName);
    WNDPROC WINAPI GetProcAddress(void *hModule, const char *lpProcName);
    s32 WINAPI ChoosePixelFormat(void *hdc, const PIXELFORMATDESCRIPTOR *ppfd);
    s32 WINAPI SetPixelFormat(void *hdc, s32 iPixelFormat, const PIXELFORMATDESCRIPTOR *ppfd);
    s32 WINAPI DescribePixelFormat(void *hdc, s32 iPixelFormat, u32 nBytes, PIXELFORMATDESCRIPTOR *ppfd);
    void *GetDC(void *hWnd);
    s32 WINAPI DestroyWindow(void *hWnd);
    s32 ReleaseDC(void *hWnd, void *hDC);
    s32 WINAPI PeekMessageW(MSG *lpMsg, void *hWnd, u32 wMsgFilterMin, u32 wMsgFilterMax, u32 wRemoveMsg);
    u32 WINAPI GetRawInputData(void *hRawInput, u32 uiCommand, void *pData, u32 *pcbSize, u32 cbSizeHeader);
    s32 WINAPI TranslateMessage(const MSG *lpMsg);
    sptr WINAPI DispatchMessageW(const MSG *lpmsg);
    s32 WINAPI FreeLibrary(void *hModule);
    void WINAPI OutputDebugStringA(const char *lpOutputString);
    s32 WINAPI DeleteFileW(const u16 *lpFileName);
    s32 WINAPI MoveFileW(const u16 *lpExistingFileName, const u16 *lpNewFileName);
    u32 timeBeginPeriod(u32 uPeriod);
    void *WINAPI LoadLibraryW(const u16 *lpFileName);
    void *WINAPI LoadCursorW(void *hInstance, const u16 *lpCursorName);
    u32 WINAPI GetModuleFileNameW(void *hModule, u16 *lpFilename, u32  nSize);
    u16 WINAPI RegisterClassW(const WNDCLASS *lpWndClass);
    s32 WINAPI RegisterRawInputDevices(const RAWINPUTDEVICE *pRawInputDevices, u32 uiNumDevices, u32 cbSize);
    s32 WINAPI ShowWindow(void *hWnd, s32 nCmdShow);
    s32 WINAPI SetForegroundWindow(void *hWnd);
    s32 WINAPI SwapBuffers(void *hdc);
    void WINAPI Sleep(u32 dwMilliseconds);
    void *MonitorFromWindow(void *hwnd, u32 dwFlags);
    s32 GetMonitorInfoW(void *hMonitor, MONITORINFO *lpmi);
    void *WINAPI CreateWindowExW(u32 dwExStyle, const u16 *lpClassName, const u16 *lpWindowName, u32 dwStyle, s32 X, s32 Y, s32 nWidth, s32 nHeight, void * hWndParent, void *hMenu, void *hInstance, void *lpParam);
    void *CreateThread(SECURITY_ATTRIBUTES *lpThreadAttributes, u32 swStackSize, u32 (*lpStartAddress)(void *), void *lpParameter, u32 dwCreationFlags, u32 *lpThreadId);
    u32 WaitForSingleObject(void *hHandle, u32 dwMilliseconds);
    void *CreateSemaphoreW(SECURITY_ATTRIBUTES *lpSemaphoreAttributes, s32 lInitialCount, s32 lMaximumCount, u16 *lpName);
    s32 ReleaseSemaphore(void *hSemaphore, s32 lReleaseCount, s32 *lpPreviousCount);
    u32 GetLastError();
    void GetSystemInfo(SYSTEM_INFO *lpSystemInfo);
    void *SelectObject(void *hdc, void *h);
    void *GetStockObject(s32 i);
    s32 Rectangle(void *hdc, int left, int top, int right, int bottom);
}
