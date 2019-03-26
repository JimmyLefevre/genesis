
/* Linux platform todo:
   - Remove #include asoundlib.h.
   - Potentially load the remaining glX functions dynamically: glXCreateContextAttribsARB, glXChooseFBConfig
   - Game code loading */

// We don't need this is we're not linking to libc:
// #define _LARGEFILE64_SOURCE // Useless? __USE_LARGEFILE64 instead maybe?

// #include <x86intrin.h> // __rdtsc, might not need to include this after we include SIMD headers

#include "basic.h"

#include "os_headers.h"
#include "linux_syscall.h"
#include "os_context.h"
#include "interfaces.h"
#include "os_code.cpp"

//
// System:
//

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

// Should these be 0x?
#define O_WRONLY 01
#define O_RDONLY 00
#define O_CREAT 0100
#define O_NONBLOCK 04000
#define O_LARGEFILE 0100000

// Resource limits:
#define RLIMIT_AS 9
#define RLIMIT_STACK 3

typedef long clockid_t;
typedef u64 dev_t;
typedef unsigned long ino_t;
typedef u32 mode_t;
typedef unsigned long nlink_t;
typedef u32 uid_t;
typedef u32 gid_t;
typedef long blksize_t;
typedef long blkcnt_t;
typedef long off_t;
typedef unsigned long rlim_t;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

struct timespec {
    long tv_sec;
    long tv_nsec;
};

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    
    timespec st_atim; // Last access
    timespec st_mtime; // Last modification
    timespec st_ctim; // Last status change
    long padding[3];
};

extern "C" {
    void *dlopen(const char *__file, int __mode);
    int dlclose(void *__handle);
    void *dlsym(void *__restrict __handle, const char *__restrict __name);
    long ulimit(int cmd, long newlimit);
}

#define SEEK_SET 0
// Syscalls:
extern "C" {
    inline s64 linux_syscall0(s64 syscall_code) {
        s64 error;
        __asm__("movq %1, %%rax \n"
                "syscall \n"
                "movq %%rax, %0"
                : "=r" (error)
                : "r" (syscall_code)
                : "%rax");
        return error;
    }
    
    inline s64 linux_syscall1(s64 syscall_code, u64 arg0) {
        s64 error;
        __asm__("movq %1, %%rax \n"
                "movq %2, %%rdi \n"
                "syscall \n"
                "movq %%rax, %0"
                : "=r" (error)
                : "r" (syscall_code), "r" (arg0)
                : "%rax", "%rdi");
        return error;
    }
    
    inline s64 linux_syscall2(s64 syscall_code, u64 arg0, u64 arg1) {
        s64 error;
        __asm__("movq %1, %%rax \n"
                "movq %2, %%rdi \n"
                "movq %3, %%rsi \n"
                "syscall \n"
                "movq %%rax, %0"
                : "=r" (error)
                : "r"(syscall_code), "r" (arg0), "r" (arg1)
                : "%rax", "%rdi", "%rsi");
        return error;
    }
    
    inline s64 linux_syscall3(s64 syscall_code, u64 arg0, u64 arg1, u64 arg2) {
        s64 error;
        __asm__("movq %1, %%rax \n"
                "movq %2, %%rdi \n"
                "movq %3, %%rsi \n"
                "movq %4, %%rdx \n"
                "syscall \n"
                "movq %%rax, %0"
                : "=r" (error)
                : "r"(syscall_code), "r" (arg0), "r" (arg1), "r" (arg2)
                : "%rax", "%rdi", "%rsi", "%rdx");
        return error;
    }
    
    inline s64 linux_syscall6(s64 syscall_code, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {
        s64 error;
        __asm__("movq %1, %%rax \n"
                "movq %2, %%rdi \n"
                "movq %3, %%rsi \n"
                "movq %4, %%rdx \n"
                "movq %5, %%r10 \n"
                "movq %6, %%r8 \n"
                "movq %7, %%r9 \n"
                "syscall \n"
                "movq %%rax, %0"
                : "=r" (error)
                : "r"(syscall_code), "r" (arg0), "r" (arg1), "r" (arg2), "r"(arg3), "r"(arg4), "m"(arg5)
                : "%rax", "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9");
        return error;
    }
}
inline void linux_exit(int error) {
    // exit_group retires all threads.
    linux_syscall1(SYSCALL_exit_group, error);
}
// @Incomplete:
/*

void *__mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
 long ret;
 if (off & OFF_MASK) {
  errno = EINVAL;
  return MAP_FAILED;
 }
 if (len >= PTRDIFF_MAX) {
  errno = ENOMEM;
  return MAP_FAILED;
 }
 if (flags & MAP_FIXED) {
  __vm_wait();
 }
#ifdef SYS_mmap2
 ret = __syscall(SYS_mmap2, start, len, prot, flags, fd, off/UNIT);
#else
 ret = __syscall(SYS_mmap, start, len, prot, flags, fd, off);
#endif
 // Fixup incorrect EPERM from kernel.
if (ret == -EPERM && !start && (flags&MAP_ANON) && !(flags&MAP_FIXED))
ret = -ENOMEM;
return (void *)__syscall_ret(ret);
}
*/
inline void *linux_mmap(void *addr, usize length, int prot, int flags, int fd, off_t offset) {
    return (void *)linux_syscall6(SYSCALL_mmap, (u64)addr, (u64)length, (u64)prot, (u64)flags, (s64)fd, (u64)offset);
}
inline void linux_rename(const char *oldname, const char *newname) {
    linux_syscall2(SYSCALL_rename, (u64)oldname, (u64)newname);
}
inline void linux_unlink(const char *pathname) {
    linux_syscall1(SYSCALL_unlink, (u64)pathname);
}
inline int linux_clock_gettime(clockid_t clk_id, timespec *tp) {
    return linux_syscall2(SYSCALL_clock_gettime, (u64)clk_id, (u64)tp);
}
inline int linux_access(const char *filename, int mode) {
    return linux_syscall2(SYSCALL_access, (u64)filename, (u64)mode);
}
inline int linux_open(const char *filename, int flags, mode_t mode) {
    return linux_syscall3(SYSCALL_open, (u64)filename, (u64)flags, (u64)mode);
}
inline int linux_close(int fd) {
    return linux_syscall1(SYSCALL_close, (u64)fd);
}
inline ssize_t linux_read(int fd, char *buf, usize count) {
    return linux_syscall3(SYSCALL_read, (u64)fd, (u64)buf, (u64)count);
}
inline ssize_t linux_write(int fd, const char *buf, usize count) {
    return linux_syscall3(SYSCALL_write, (u64)fd, (u64)buf, (u64)count);
}
inline int linux_nanosleep(timespec *rqtp, timespec *rmtp) {
    return linux_syscall2(SYSCALL_nanosleep, (u64)rqtp, (u64)rmtp);
}
inline int fstat(unsigned int fd, stat *statbuf) {
    return linux_syscall2(SYSCALL_fstat, (u64)fd, (u64)statbuf);
}
inline int linux_munmap(void *addr, usize len) {
    return linux_syscall2(SYSCALL_munmap, (u64)addr, (u64)len);
}
inline ssize_t linux_readlink(const char *pathname, char *buf, usize bufsiz) {
    return linux_syscall3(SYSCALL_readlink, (u64)pathname, (u64)buf, (u64)bufsiz);
}
inline int linux_getrlimit(int resource, struct rlimit *rlim) {
    return linux_syscall2(SYSCALL_getrlimit, (u64)resource, (u64)rlim);
}
inline int linux_setrlimit(int resource, const struct rlimit *rlim) {
    return linux_syscall2(SYSCALL_setrlimit, (u64)resource, (u64)rlim);
}
inline int linux_lseek(int fd, off_t offset) {
    return linux_syscall3(SYSCALL_lseek, (u64)fd, (u64)offset, SEEK_SET);
}
inline int linux_fstat(int fd, stat *statbuf) {
    return linux_syscall2(SYSCALL_fstat, (u64)fd, (u64)statbuf);
}
inline int linux_chdir(const char *path) {
    return linux_syscall1(SYSCALL_chdir, (u64)path);
}

enum File_Protection {
    PROT_READ = 0x1,
    PROT_WRITE = 0x2,
    PROT_EXEC = 0x4,
    PROT_NONE = 0x0,
};

enum Memory_Map_Flags {
    MAP_SHARED = 0x01,
    MAP_PRIVATE = 0x02,
    MAP_ANONYMOUS = 0x20,
};

enum dlopen_Mode {
    RTLD_LAZY = 0x00001,
    RTLD_NOW = 0x00002,
    RTLD_BINDING_MASK = 0x3,
    RTLD_NOLOAD = 0x00004,
    RTLD_DEEPBIND = 0x00008,
};

//
// X:
//
#define AllocNone 0
#define InputOutput 1
#define InputOnly 2
#define ForgetGravity 0
#define StaticGravity 10
#define CWBorderPixel (1L << 3)
#define CWBitGravity (1L << 4)
#define CWWinGravity (1L << 5)
#define CWEventMask (1L << 11)
#define CWColormap (1L << 13)
#define StructureNotifyMask (1L << 17)
#define FocusChangeMask (1L << 21)
#define XISetMask(ptr, event) (((unsigned char *)(ptr))[(event)>>3] |= (1 << ((event) & 7)))
#define XIAllDevices 0
#define XIAllMasterDevices 1
#define True 1
#define False 0
#define BadRequest 1
#define GenericEvent 35
#define DefaultScreen(dpy) (((_XPrivDisplay)dpy)->default_screen)
#define DefaultRootWindow(dpy) (ScreenOfDisplay(dpy, DefaultScreen(dpy))->root)
#define ScreenOfDisplay(dpy, scr) (&((_XPrivDisplay)dpy)->screens[scr])
#define DestroyNotify 17
#define UnmapNotify 18
#define MapNotify 19
#define FocusIn 9
#define FocusOut 10
#define ClientMessage 33
#define KeyPress 2
#define KeyRelease 3

typedef unsigned long Window;
typedef unsigned long Cursor;
typedef unsigned long Time;
typedef unsigned long Atom;
typedef unsigned long Barrier;
typedef unsigned long Pixmap;
typedef unsigned long Colormap;
typedef int Status;
typedef int Bool;
// These are either u32s or u64s.
typedef unsigned int Drawable;
typedef unsigned int VisualID;
typedef char *XPointer;

struct XIEventMask {
    int deviceid;
    int mask_len;
    unsigned char *mask;
};

struct XIButtonState {
    int mask_len;
    unsigned char *mask;
};


struct XSetWindowAttributes {
    Pixmap background_pixmap;	/* background or None or ParentRelative */
    unsigned long background_pixel;	/* background pixel */
    Pixmap border_pixmap;	/* border of the window */
    unsigned long border_pixel;	/* border pixel value */
    int bit_gravity;		/* one of bit gravity values */
    int win_gravity;		/* one of the window gravity values */
    int backing_store;		/* NotUseful, WhenMapped, Always */
    unsigned long backing_planes;/* planes to be preseved if possible */
    unsigned long backing_pixel;/* value to use in restoring planes */
    Bool save_under;		/* should bits under be saved? (popups) */
    long event_mask;		/* set of events that should be saved */
    long do_not_propagate_mask;	/* set of events that should not propagate */
    Bool override_redirect;	/* boolean value for override-redirect */
    Colormap colormap;		/* color map to be associated with window */
    Cursor cursor;		/* cursor to be displayed (or None) */
};
struct Screen;
struct Depth;
struct Visual;
struct ScreenFormat;
struct _XPrivate;
struct XExtData {
    int number;
    XExtData *next;
    int (*free_private)(XExtData *extension);
    XPointer private_data;
};
struct Display {
    XExtData *ext_data;
    struct _XPrivate *private1;
    int fd;
    int private2;
    int proto_major_version;
    int proto_minor_version;
    char *vendor;
    unsigned long private3;
    unsigned long private4;
    unsigned long private5;
    int private6;
    unsigned long (*resource_alloc)(struct _XDisplay *);
    int byte_order;
    int bitmap_unit;
    int bitmap_pad;
    int bitmap_bit_order;
    int nformats;
    ScreenFormat *pixmap_format;
    int private8;
    int release;
    struct _XPrivate *private9, *private10;
    int qlen;
    unsigned long last_request_read;
    unsigned long request;
    XPointer private11;
    XPointer private12;
    XPointer private13;
    XPointer private14;
    unsigned max_request_size;
    struct _XrmHashBucketRec *db;
    int (*private15)(struct _XDisplay *);
    char *display_name;
    int default_screen;
    int nscreens;
    Screen *screens;
    unsigned long motion_buffer;
    unsigned long private16;
    int min_keycode;
    int max_keycode;
    XPointer private17;
    XPointer private18;
    int private19;
    char *xdefaults;
};
typedef Display *_XPrivDisplay;

struct _XGC {
    XExtData *ext_data;
    unsigned long gid;
};
typedef _XGC *GC;

struct Screen {
    XExtData *ext_data;	/* hook for extension to hang data */
    struct _XDisplay *display;/* back pointer to display structure */
    Window root;		/* Root window id. */
    int width, height;	/* width and height of screen */
    int mwidth, mheight;	/* width and height of  in millimeters */
    int ndepths;		/* number of depths possible */
    Depth *depths;		/* list of allowable depths on the screen */
    int root_depth;		/* bits per pixel */
    Visual *root_visual;	/* root visual */
    GC default_gc;		/* GC for the root root visual */
    Colormap cmap;		/* default color map */
    unsigned long white_pixel;
    unsigned long black_pixel;	/* White and Black pixel values */
    int max_maps, min_maps;	/* max and min color maps */
    int backing_store;	/* Never, WhenMapped, Always */
    Bool save_unders;
    long root_input_mask;	/* initial root input mask */
};

struct Visual {
    XExtData *ext_data;
    VisualID visualid;
    int c_class;
    unsigned long red_mask, green_mask, blue_mask;
    int bits_per_rgb;
    int map_entries;
};

struct XVisualInfo {
    Visual *visual;
    VisualID visualID;
    int screen;
    int depth;
    int c_class;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
};

struct XKeyEvent {
    int type;		/* of event */
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;	        /* "event" window it is reported relative to */
    Window root;	        /* root window that the event occurred on */
    Window subwindow;	/* child window */
    Time time;		/* milliseconds */
    int x, y;		/* pointer x, y coordinates in event window */
    int x_root, y_root;	/* coordinates relative to root */
    unsigned int state;	/* key or button mask */
    unsigned int keycode;	/* detail */
    Bool same_screen;	/* same screen flag */
};
typedef XKeyEvent XKeyPressedEvent;
typedef XKeyEvent XKeyReleasedEvent;

struct XButtonEvent {
    int type;		/* of event */
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;	        /* "event" window it is reported relative to */
    Window root;	        /* root window that the event occurred on */
    Window subwindow;	/* child window */
    Time time;		/* milliseconds */
    int x, y;		/* pointer x, y coordinates in event window */
    int x_root, y_root;	/* coordinates relative to root */
    unsigned int state;	/* key or button mask */
    unsigned int button;	/* detail */
    Bool same_screen;	/* same screen flag */
};
typedef XButtonEvent XButtonPressedEvent;
typedef XButtonEvent XButtonReleasedEvent;

struct XMotionEvent {
    int type;		/* of event */
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;	        /* "event" window reported relative to */
    Window root;	        /* root window that the event occurred on */
    Window subwindow;	/* child window */
    Time time;		/* milliseconds */
    int x, y;		/* pointer x, y coordinates in event window */
    int x_root, y_root;	/* coordinates relative to root */
    unsigned int state;	/* key or button mask */
    char is_hint;		/* detail */
    Bool same_screen;	/* same screen flag */
};
typedef XMotionEvent XPointerMovedEvent;

struct XCrossingEvent {
    int type;		/* of event */
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;	        /* "event" window reported relative to */
    Window root;	        /* root window that the event occurred on */
    Window subwindow;	/* child window */
    Time time;		/* milliseconds */
    int x, y;		/* pointer x, y coordinates in event window */
    int x_root, y_root;	/* coordinates relative to root */
    int mode;		/* NotifyNormal, NotifyGrab, NotifyUngrab */
    int detail;
    /*
    * NotifyAncestor, NotifyVirtual, NotifyInferior,
    * NotifyNonlinear,NotifyNonlinearVirtual
    */
    Bool same_screen;	/* same screen flag */
    Bool focus;		/* boolean focus */
    unsigned int state;	/* key or button mask */
};
typedef XCrossingEvent XEnterWindowEvent;
typedef XCrossingEvent XLeaveWindowEvent;

struct XFocusChangeEvent {
    int type;		/* FocusIn or FocusOut */
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;		/* window of event */
    int mode;		/* NotifyNormal, NotifyWhileGrabbed,
       NotifyGrab, NotifyUngrab */
    int detail;
    /*
    * NotifyAncestor, NotifyVirtual, NotifyInferior,
    * NotifyNonlinear,NotifyNonlinearVirtual, NotifyPointer,
    * NotifyPointerRoot, NotifyDetailNone
    */
};
typedef XFocusChangeEvent XFocusInEvent;
typedef XFocusChangeEvent XFocusOutEvent;

/* generated on EnterWindow and FocusIn  when KeyMapState selected */
struct XKeymapEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;
    char key_vector[32];
};

struct XExposeEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;
    int x, y;
    int width, height;
    int count;		/* if non-zero, at least this many more */
};

struct XGraphicsExposeEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Drawable drawable;
    int x, y;
    int width, height;
    int count;		/* if non-zero, at least this many more */
    int major_code;		/* core is CopyArea or CopyPlane */
    int minor_code;		/* not defined in the core */
};

struct XNoExposeEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Drawable drawable;
    int major_code;		/* core is CopyArea or CopyPlane */
    int minor_code;		/* not defined in the core */
};

struct XVisibilityEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;
    int state;		/* Visibility state */
};

struct XCreateWindowEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window parent;		/* parent of the window */
    Window window;		/* window id of window created */
    int x, y;		/* window location */
    int width, height;	/* size of window */
    int border_width;	/* border width */
    Bool override_redirect;	/* creation should be overridden */
};

struct XDestroyWindowEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window event;
    Window window;
};

struct XUnmapEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window event;
    Window window;
    Bool from_configure;
};

struct XMapEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window event;
    Window window;
    Bool override_redirect;	/* boolean, is override set... */
};

struct XMapRequestEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window parent;
    Window window;
};

struct XReparentEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window event;
    Window window;
    Window parent;
    int x, y;
    Bool override_redirect;
};

struct XConfigureEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window event;
    Window window;
    int x, y;
    int width, height;
    int border_width;
    Window above;
    Bool override_redirect;
};

struct XGravityEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window event;
    Window window;
    int x, y;
};

struct XResizeRequestEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;
    int width, height;
};

struct XConfigureRequestEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window parent;
    Window window;
    int x, y;
    int width, height;
    int border_width;
    Window above;
    int detail;		/* Above, Below, TopIf, BottomIf, Opposite */
    unsigned long value_mask;
};

struct XCirculateEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window event;
    Window window;
    int place;		/* PlaceOnTop, PlaceOnBottom */
};

struct XCirculateRequestEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window parent;
    Window window;
    int place;		/* PlaceOnTop, PlaceOnBottom */
};

struct XPropertyEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;
    Atom atom;
    Time time;
    int state;		/* NewValue, Deleted */
};

struct XSelectionClearEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;
    Atom selection;
    Time time;
};

struct XSelectionRequestEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window owner;
    Window requestor;
    Atom selection;
    Atom target;
    Atom property;
    Time time;
};

struct XSelectionEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window requestor;
    Atom selection;
    Atom target;
    Atom property;		/* ATOM or None */
    Time time;
};

struct XColormapEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;
    Colormap colormap;	/* COLORMAP or None */
    Bool c_new;		/* C++ */
    int state;		/* ColormapInstalled, ColormapUninstalled */
};

struct XClientMessageEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;
    Atom message_type;
    int format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
};

struct XColor {
    unsigned long pixel;
    unsigned short red, green, blue;
    char flags;  /* do_red, do_green, do_blue */
    char pad;
};

struct XMappingEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;	/* Display the event was read from */
    Window window;		/* unused */
    int request;		/* one of MappingModifier, MappingKeyboard,
       MappingPointer */
    int first_keycode;	/* first keycode */
    int count;		/* defines range of change w. first_keycode*/
};

struct XErrorEvent {
    int type;
    Display *display;	/* Display the event was read from */
    unsigned long resourceid;	/* resource id */
    unsigned long serial;	/* serial number of failed request */
    unsigned char error_code;	/* error code of failed request */
    unsigned char request_code;	/* Major op-code of failed request */
    unsigned char minor_code;	/* Minor op-code of failed request */
};

struct XAnyEvent {
    int type;
    unsigned long serial;	/* # of last request processed by server */
    Bool send_event;	/* true if this came from a SendEvent request */
    Display *display;/* Display the event was read from */
    Window window;	/* window on which event was requested in event mask */
};
struct XGenericEvent {
    int            type;         /* of event. Always GenericEvent */
    unsigned long  serial;       /* # of last request processed */
    Bool           send_event;   /* true if from SendEvent request */
    Display        *display;     /* Display the event was read from */
    int            extension;    /* major opcode of extension that caused the event */
    int            evtype;       /* actual event type. */
};
struct XGenericEventCookie {
    int            type;         /* of event. Always GenericEvent */
    unsigned long  serial;       /* # of last request processed */
    Bool           send_event;   /* true if from SendEvent request */
    Display        *display;     /* Display the event was read from */
    int            extension;    /* major opcode of extension that caused the event */
    int            evtype;       /* actual event type. */
    unsigned int   cookie;
    void           *data;
};
union XEvent {
    int type;
    XAnyEvent xany;
    XKeyEvent xkey;
    XButtonEvent xbutton;
    XMotionEvent xmotion;
    XCrossingEvent xcrossing;
    XFocusChangeEvent xfocus;
    XExposeEvent xexpose;
    XGraphicsExposeEvent xgraphicsexpose;
    XNoExposeEvent xnoexpose;
    XVisibilityEvent xvisibility;
    XCreateWindowEvent xcreatewindow;
    XDestroyWindowEvent xdestroywindow;
    XUnmapEvent xunmap;
    XMapEvent xmap;
    XMapRequestEvent xmaprequest;
    XReparentEvent xreparent;
    XConfigureEvent xconfigure;
    XGravityEvent xgravity;
    XResizeRequestEvent xresizerequest;
    XConfigureRequestEvent xconfigurerequest;
    XCirculateEvent xcirculate;
    XCirculateRequestEvent xcirculaterequest;
    XPropertyEvent xproperty;
    XSelectionClearEvent xselectionclear;
    XSelectionRequestEvent xselectionrequest;
    XSelectionEvent xselection;
    XColormapEvent xcolormap;
    XClientMessageEvent xclient;
    XMappingEvent xmapping;
    XErrorEvent xerror;
    XKeymapEvent xkeymap;
    XGenericEvent xgeneric;
    XGenericEventCookie xcookie;
    long pad[24];
};

enum XI_Event_Type {
    XI_DeviceChanged = 1,
    XI_KeyPress = 2,
    XI_KeyRelease = 3,
    XI_ButtonPress = 4,
    XI_ButtonRelease = 5,
    XI_Motion = 6,
    XI_Enter = 7,
    XI_Leave = 8,
    XI_FocusIn = 9,
    XI_FocusOut = 10,
    XI_HierarchyChanged = 11,
    XI_PropertyEvent = 12,
    XI_RawKeyPress = 13,
    XI_RawKeyRelease = 14,
    XI_RawButtonPress = 15,
    XI_RawButtonRelease = 16,
    XI_RawMotion = 17,
    XI_TouchBegin = 18,
    XI_TouchUpdate = 19,
    XI_TouchEnd = 20,
    XI_TouchOwnership = 21,
    XI_RawTouchBegin = 22,
    XI_RawTouchUpdate = 23,
    XI_RawTouchEnd = 24,
    XI_BarrierHit = 25,
    XI_BarrierLeave = 26,
    XI_LASTEVENT = XI_BarrierLeave,
};
extern "C" {
    Colormap XCreateColormap(Display *dpy, Window win, Visual *visual, int alloc);
    Window XCreateWindow(Display *dpy, Window parent, int x, int y, unsigned int width, unsigned int height, unsigned int border_width, int depth, unsigned int c_class, Visual *visual, unsigned long valuemask, XSetWindowAttributes *attributes);
    int XNextEvent(Display *dpy, XEvent *event_return);
    Window XDefaultRootWindow(Display *dpy);
    Pixmap XCreatePixmap(Display *dpy, Drawable d, unsigned int width, unsigned int height, unsigned int depth);
    Display *XOpenDisplay(const char *display_name);
    Cursor XCreatePixmapCursor(Display *dpy, Pixmap source, Pixmap mask, XColor *foreground_color, XColor *background_color, unsigned int x, unsigned int y);
    void XFreeEventData(Display *dpy, XGenericEventCookie *cookie);
    Bool XGetEventData(Display *dpy, XGenericEventCookie *cookie);
    int XPending(Display *dpy);
    int XFlush(Display *dpy);
    Atom XInternAtom(Display *dpy, const char *atom_name, Bool only_if_exists);
    int XStoreName(Display *dpy, Window win, const char *window_name);
    int XFree(void *data);
    int XMapRaised(Display *dpy, Window win);
    Status XIQueryVersion(Display *display, int *major_version_inout, int *minor_version_inout);
    int XISelectEvents(Display *dpy, Window win, XIEventMask *masks, int num_masks);
    int XUndefineCursor(Display *dpy, Window win);
    Bool XQueryPointer(Display *dpy, Window win, Window *root_return, Window *child_return, int *root_x_return, int *root_y_return, int *win_x_return, int *win_y_return, unsigned int *mask_return);
    Status XGetGeometry(Display *dpy, Drawable d, Window *root_return, int *x_return, int *y_return, unsigned int *width_return, unsigned int *height_return, unsigned int *border_width_return, unsigned int *depth_return);
    int XDefineCursor(Display *dpy, Window win, Cursor cursor);
    int XDefaultScreen(Display *dpy);
    int XWarpPointer(Display *dpy, Window src_w, Window dest_w, int src_x, int src_y, unsigned int src_width, unsigned int src_height, int dest_x, int dest_y);
    Status XSetWMProtocols(Display *dpy, Window win, Atom *protocols, int count);
    Bool XQueryExtension(Display *dpy, const char *name, int *major_opcode_return, int *first_event_return, int *first_error_return);
    void XDestroyWindow(Display *display, Window w);
}

//
// GLX:
//

typedef unsigned char GLubyte;
// Pointers to internal structs:
typedef void *GLXFBConfig;
typedef void *GLXContext;
// This may be a u32 or a u64.
typedef unsigned int GLXDrawable;

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_FLAGS_ARB 0x2094
#define GLX_CONTEXT_DEBUG_BIT_ARB 0x00000001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define GLX_RGBA_BIT 0x00000001
#define GLX_RENDER_TYPE 0x8011
#define GLX_RED_SIZE 8
#define GLX_GREEN_SIZE 9
#define GLX_BLUE_SIZE 10
#define GLX_ALPHA_SIZE 11
#define GLX_DOUBLEBUFFER 5
#define GLX_STENCIL_SIZE 13
#define GLX_DEPTH_SIZE 12
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126

extern "C" {
    void glXSwapBuffers(Display *dpy, GLXDrawable drawable);
    Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx);
    const char *glXQueryExtensionsString(Display *dpy, int screen);
    XVisualInfo *glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config);
    GLXFBConfig *glXChooseFBConfig(Display *dpy, int screen, const int *attribList, int *nitems);
    void *glXGetProcAddressARB(const GLubyte *);
}

//
// ALSA:
//

#define SND_PCM_NONBLOCK 0x00000001

typedef void snd_pcm_t;
typedef void snd_pcm_hw_params_t;
typedef long snd_pcm_sframes_t;
typedef unsigned long snd_pcm_uframes_t;

enum snd_pcm_state_t {
    SND_PCM_STATE_OPEN = 0,
    SND_PCM_STATE_SETUP,
    SND_PCM_STATE_RUNNING,
    SND_PCM_STATE_XRUN,
    SND_PCM_STATE_DRAINING,
    SND_PCM_STATE_PAUSED,
    SND_PCM_STATE_SUSPENDED,
    SND_PCM_STATE_DISCONNECTED,
    SND_PCM_STATE_LAST = SND_PCM_STATE_DISCONNECTED,
    SND_PCM_STATE_PRIVATE1 = 1024,
};

enum snd_pcm_stream_t {
    SND_PCM_STREAM_PLAYBACK = 0,
};

enum snd_pcm_access_t {
    SND_PCM_ACCESS_MMAP_INTERLEAVED = 0,
    SND_PCM_ACCESS_MMAP_NONINTERLEAVED,
    SND_PCM_ACCESS_MMAP_COMPLEX,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    SND_PCM_ACCESS_RW_NONINTERLEAVED,
    SND_PCM_ACCESS_LAST = SND_PCM_ACCESS_RW_NONINTERLEAVED,
};

enum snd_pcm_format_t {
    SND_PCM_FORMAT_S16_LE = 2,
};

struct snd_pcm_channel_area_t {
    void *addr;
    unsigned int first;
    unsigned int step;
};

extern "C" {
    int snd_pcm_open(snd_pcm_t **pcmp, const char *name, snd_pcm_stream_t stream, int mode);
    int snd_pcm_prepare(snd_pcm_t *pcm);
    int snd_pcm_start(snd_pcm_t *pcm);
    snd_pcm_state_t snd_pcm_state(snd_pcm_t *pcm);
    int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
    int snd_pcm_hw_params_set_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t access);
    int snd_pcm_hw_params_set_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t format);
    int snd_pcm_hw_params_set_rate_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
    int snd_pcm_hw_params_set_buffer_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val);
    int snd_pcm_hw_params_set_buffer_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val);
    int snd_pcm_hw_params_set_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);
    int snd_pcm_hw_params_set_period_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir);
    int snd_pcm_hw_params_get_period_size(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir);
    int snd_pcm_hw_params_get_period_time(const snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
    int snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
    snd_pcm_sframes_t snd_pcm_avail(snd_pcm_t *pcm);
    snd_pcm_sframes_t snd_pcm_avail_update(snd_pcm_t *pcm);
    int snd_pcm_mmap_begin(snd_pcm_t *pcm, const snd_pcm_channel_area_t **areas, snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames);
    snd_pcm_sframes_t snd_pcm_mmap_commit(snd_pcm_t *pcm, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames);
    snd_pcm_sframes_t snd_pcm_mmap_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
    snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
    int snd_pcm_hw_params_malloc(snd_pcm_hw_params_t **ptr);
    void snd_pcm_hw_params_free(snd_pcm_hw_params_t *obj);
    const char *snd_strerror(int error);
}

/* References:
The System V ABI manual from Intel details everything used
here: program startup calling conventions and function calling
conventions. */
extern "C" {
    int main(int argumentCount, char **arguments);
}
__asm__(".global _start \n"
        "_start: \n"
        "xor %rbp,%rbp \n"
        #if 1 // argc, argv
        "movq (%rsp), %rdi \n"
        "movq %rsp, %rsi \n"
        "addq $8, %rsi \n"
        #endif
        "andq $0xFFFFFFFFFFFFFF00,%rsp \n"
        "call main \n"
        "movq %rax, %rdi \n"
        "movq $231, %rax \n"
        "syscall");

typedef GLXContext glXCreateContextAttribsARB_t(Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);
static glXCreateContextAttribsARB_t *glXCreateContextAttribsARB;

// There are multiple SwapInterval functions: SwapIntervalEXT, SwapIntervalMESA and SwapIntervalSGI.
// According to SDL, EXT is preferable to MESA which is preferable to SGI.
typedef void glXSwapIntervalEXT_t(Display *dpy, GLXDrawable drawable, int interval);
static glXSwapIntervalEXT_t *glXSwapIntervalEXT;

typedef int glXSwapIntervalMESA_t(unsigned int interval);
static glXSwapIntervalMESA_t *myglXSwapIntervalMESA;

typedef int glXSwapIntervalSGI_t(int interval);
static glXSwapIntervalSGI_t *glXSwapIntervalSGI;

static bool should_render;
static bool cursor_is_being_captured;
static Display *display;
static Window window;
static Cursor empty_cursor;
static snd_pcm_t *alsa_handle;

static void *linux_mem_alloc(usize size) {
    void *result = linux_mmap(0, size, PROT_READ|PROT_WRITE,
                              MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    ASSERT(result != (void *)(-1));
    return result;
}

static void linux_free(void *ptr) {
    // Providing the length kinda sucks here...
    linux_munmap(ptr, (usize)(-1));
}

static void linux_print(string text) {
    linux_write(0, (const char *)text.data, text.length);
}

static f64 linux_get_secs() {
    /* Timer resolution testing:
       timespec res;
       clock_getres(0, &res);
       printf("Time resolution: %ld seconds, %ld nanoseconds\n", res.tv_sec, res.tv_nsec);
       One nanosecond on my laptop, so we're probably good.
     */
    
    timespec time;
    linux_clock_gettime(0, &time);
    f64 result = (f64)time.tv_sec + (f64)time.tv_nsec / 1000000000.0;
    
    return result;
}
// File creation bits:
#define S_IRUSR 0200 // User read permission
#define S_IWUSR 0400 // User write permission
static bool linux_write_entire_file(string file_name, string file_data) {
    int file = linux_open((const char *)file_name.data, O_WRONLY|O_CREAT|O_NONBLOCK|O_LARGEFILE, S_IRUSR|S_IWUSR);
    if(file) {
        ssize_t error = linux_write(file, (const char *)file_data.data, file_data.length);
        linux_close(file);
        return (error != -1);
    }
    return false;
}

static void center_cursor(Display *display, Window window) {
    Window root_return;
    s32 x, y;
    u32 width, height, border_width, depth;
    XGetGeometry(display, window, &root_return, &x, &y, &width, &height, &border_width, &depth);
    XWarpPointer(display, 0, window, 0, 0, 0, 0, width/2, height/2);
}

static void linux_capture_cursor() {
    XDefineCursor(display, window, empty_cursor);
    cursor_is_being_captured = true;
}

static void linux_release_cursor() {
    XUndefineCursor(display, window);
    cursor_is_being_captured = false;
}

static void linux_get_cursor_position_in_pixels(s32 *out_x, s32 *out_y) {
    // Cursor position is given in client rect coordinates.
    Window root_return, child_return;
    int root_x_return, root_y_return, win_x_return, win_y_return;
    unsigned int mask_return;
    Bool cursor_success = XQueryPointer(display, window, &root_return, &child_return, &root_x_return, &root_y_return, 
                                        &win_x_return, &win_y_return, &mask_return);
    
    
    s32 x, y;
    u32 width, height, border_width, depth;
    XGetGeometry(display, window, &root_return, &x, &y, &width, &height, &border_width, &depth);
    win_y_return = (int)height - win_y_return - 1;
    
    *out_x = win_x_return;
    *out_y = win_y_return;
}

static void *opengl_lib;

static void opengl_init(int default_screen, GLXFBConfig fb_config) {
    opengl_lib = dlopen("libGL.so", RTLD_LAZY);
    if(opengl_lib) {
        const char *glx_extensions = glXQueryExtensionsString(display, default_screen);
        glXCreateContextAttribsARB_t *glXCreateContextAttribsARB = (glXCreateContextAttribsARB_t *)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");
        glXSwapIntervalEXT = (glXSwapIntervalEXT_t *)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
        GLXContext gl_context = 0;
        if(glXCreateContextAttribsARB) {
            int context_attribs[] = {
                GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                GLX_CONTEXT_MINOR_VERSION_ARB, 0,
                GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#if GENESIS_DEV
                    |GLX_CONTEXT_DEBUG_BIT_ARB
#endif
                    ,
                GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                0,
            };
            
            gl_context = glXCreateContextAttribsARB(display, fb_config, 0, True, context_attribs);
        } else {
            // printf("CreateContextAttribs not loaded.\n");
        }
        
        glXMakeCurrent(display, window, gl_context);
        
        if(glXSwapIntervalEXT) {
            // printf("SwapInterval successfully loaded.\n"); 
            glXSwapIntervalEXT(display, window, 1);
        } else {
            myglXSwapIntervalMESA = (glXSwapIntervalMESA_t *)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalMESA");
            if(myglXSwapIntervalMESA) {
                // printf("SwapIntervalMESA successfully loaded.\n");
                myglXSwapIntervalMESA(1);
            }
            else {
                glXSwapIntervalSGI = (glXSwapIntervalSGI_t *)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
                if(glXSwapIntervalSGI) {
                    // printf("SwapIntervalSGI successfully loaded.\n");
                    glXSwapIntervalSGI(1);
                }
                else {
                    // printf("No SwapInterval could be loaded.\n");
                }
            }
        }
    } else {
        linux_print(STRING("Could not find the OpenGL library."));
    }
}

static u32 linux_begin_sound_update() {
    snd_pcm_state_t state = snd_pcm_state(alsa_handle);
    snd_pcm_sframes_t samples_to_update = snd_pcm_avail(alsa_handle);
    /* @Robustness: This is what the ALSA example does, find some way to write in advance instead?
    if state == SND_PCM_STATE_XRUN {
    snd_pcm_prepare(alsa_handle);
    }
    */
    if(samples_to_update >= 0) {
        return (u32)samples_to_update;
    } else {
        return 0;
    }
}
static void linux_end_sound_update(s16 *input_buffer, u32 samples_to_write) {
    ASSERT(samples_to_write > 0);
    snd_pcm_sframes_t written = snd_pcm_writei(alsa_handle, input_buffer, samples_to_write);
}

static void linux_unload_game_code(Game_Interface *g_interface) {
    if(g_interface->os_handle) {
        dlclose(g_interface->os_handle);
        g_interface->run_frame = 0;
    }
}

OS_OPEN_FILE(linux_open_file) {
    ASSERT(!file_name.data[file_name.length]);
    int handle = linux_open((char *)file_name.data, O_RDONLY|O_LARGEFILE|O_NONBLOCK, 0);
    return (void *)handle;
}
// @Important: Read fails/doesn't work the way we want I guess.
OS_READ_FILE(linux_read_file) {
    int file = (int)((u64)handle);
    linux_lseek(file, offset);
    usize bytes_read = linux_read(file, (char *)out, length);
    ASSERT(bytes_read == length);
    return (bytes_read == length);
}
OS_FILE_SIZE(linux_file_size) {
    int file = (int)((u64)handle);
    stat file_info;
    linux_fstat(file, &file_info);
    return (usize)file_info.st_size;
}
OS_READ_ENTIRE_FILE(linux_read_entire_file) {
    void *handle = linux_open_file(file_name);
    usize size = linux_file_size(handle);
    string result;
    result.data = (u8 *)linux_mem_alloc(size);
    result.length = size;
    linux_read_file(handle, result.data, 0, size);
    linux_close((int)((u64)handle));
    return result;
}

static Game_Interface linux_load_game_code(const char *built_dll_name, const char *loaded_dll_name) {
    linux_unlink(loaded_dll_name);
    linux_rename(built_dll_name, loaded_dll_name);
    
    // We need to wait between renaming the DLL and opening it.
    // Change to checking last modified time?
    timespec time_to_sleep = {};
    time_to_sleep.tv_sec = 0;
    time_to_sleep.tv_nsec = 16000000ll;
    timespec time_to_sleep_remaining = {};
    linux_nanosleep(&time_to_sleep, &time_to_sleep_remaining);
    
    // Maybe we'd rather use RTLD_NOW here?
    void *dll_handle = dlopen(loaded_dll_name, RTLD_LAZY);
    if(dll_handle) {
        game_get_api get_api_proc = (game_get_api)dlsym(dll_handle, "g_get_api");
        if(get_api_proc) {
            u32 i;
            // @Incomplete @Cleanup: Macro this out.
            OS_Export os_export;
            os_export.os.get_seconds = linux_get_secs;
            os_export.os.mem_alloc = linux_mem_alloc;
            os_export.os.open_file = linux_open_file;
            os_export.os.read_file = linux_read_file;
            os_export.os.file_size = linux_file_size;
            os_export.os.read_entire_file = linux_read_entire_file;
            os_export.os.write_entire_file = linux_write_entire_file;
            os_export.os.begin_sound_update = linux_begin_sound_update;
            os_export.os.end_sound_update = linux_end_sound_update;
            os_export.os.capture_cursor = linux_capture_cursor;
            os_export.os.release_cursor = linux_release_cursor;
            os_export.os.print = linux_print;
            ASSERT(opengl_lib);
            for(i = 0; i < ARRAY_LENGTH(opengl_base_functions); ++i) {
                os_export.opengl.base_functions[i] = (void *)dlsym(opengl_lib, opengl_base_functions[i]);
            }
            for(i = 0; i < ARRAY_LENGTH(opengl_extended_functions); ++i) {
                os_export.opengl.extended_functions[i] = (void *)glXGetProcAddressARB((const GLubyte *)opengl_extended_functions[i]);
            }
            
            Game_Interface result = get_api_proc(&os_export);
            result.os_handle = dll_handle;
            return result;
        } else {
            linux_print(STRING("Could not find get_api_proc.\n"));
        }
    } else {
        linux_print(STRING("Could not load the game DLL.\n"));
    }
    return {};
}

static void alsa_init() {
    if(snd_pcm_open(&alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0) {
        UNHANDLED;
    }
    
    unsigned int audio_hz = 44100;
    snd_pcm_uframes_t period_size_in_samples = 480;
    snd_pcm_uframes_t buffer_size_in_samples = audio_hz;
    snd_pcm_hw_params_t *hardware_params;
    int dir = 0;
    
    { // Setting hardware parameters.
        snd_pcm_hw_params_malloc(&hardware_params);
        
        // Default parameters:
        snd_pcm_hw_params_any(alsa_handle, hardware_params);
        // We make this be whatever DSound does.
        snd_pcm_hw_params_set_access(alsa_handle, hardware_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(alsa_handle, hardware_params, SND_PCM_FORMAT_S16_LE); // s16, little endian
        snd_pcm_hw_params_set_channels(alsa_handle, hardware_params, 2);
        snd_pcm_hw_params_set_rate_near(alsa_handle, hardware_params, &audio_hz, 0);
        snd_pcm_hw_params_set_buffer_size_near(alsa_handle, hardware_params, &buffer_size_in_samples);
        snd_pcm_hw_params_set_period_size_near(alsa_handle, hardware_params, &period_size_in_samples, 0);
        if(snd_pcm_hw_params(alsa_handle, hardware_params) < 0) {
            linux_print(STRING("Couldn't set proper sound parameters.\n"));
            linux_exit(1);
        }
        snd_pcm_hw_params_free(hardware_params);
    }
    
    snd_pcm_start(alsa_handle);
    snd_pcm_prepare(alsa_handle);
}

int main(int argumentCount, char **arguments) {
    // ext4 file name length limit is 255 bytes.
    char *exe_full_path = arguments[0];
    char built_dll_full_path[256];
    char loaded_dll_full_path[256];
    {
        char *built_dll_suffix = "genesis";
        char *loaded_dll_suffix = "loaded_genesis";
        s32 exe_directory_path_length = 0;
        s32 i = 0;
        while(exe_full_path[i]) {
            if(exe_full_path[i] == '/') {
                exe_directory_path_length = i + 1;
            }
            ++i;
        }
        mem_copy(exe_full_path, built_dll_full_path, exe_directory_path_length);
        mem_copy(built_dll_suffix, built_dll_full_path + exe_directory_path_length, ARRAY_LENGTH("genesis"));
        mem_copy(exe_full_path, loaded_dll_full_path, exe_directory_path_length);
        mem_copy(loaded_dll_suffix, loaded_dll_full_path + exe_directory_path_length, ARRAY_LENGTH("loaded_genesis"));
    }
    
    int result = linux_chdir("/home/keeba/Projects/genesis/trunk/assets");
    
    // @#
    unsigned int window_width = 1280;
    unsigned int window_height = 720;
    
    alsa_init();
    display = XOpenDisplay(0);
    if(display) {
        // Set the desktop's root as our root, since we only have one window:
        unsigned long root_window = DefaultRootWindow(display);
        int default_screen = DefaultScreen(display);
        
        
        int fb_attribs[] = {
            // GLX_X_RENDERABLE, True,
            // GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            // GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DOUBLEBUFFER, True,
            0,
        };
        
        // int gl_major, gl_minor;
        // glXQueryVersion(display, &gl_major, &gl_minor);
        
        
        int fb_count;
        GLXFBConfig *fb_configs = glXChooseFBConfig(display, default_screen, fb_attribs, &fb_count);
        // printf("We have %d framebuffer configs.\n", fb_count);
        GLXFBConfig fb_config = fb_configs[0];
        XFree(fb_configs);
        // We can iterate over these framebuffer configs if necessary,
        // such as when using multisampling.
        XVisualInfo *visual_info = glXGetVisualFromFBConfig(display, fb_config);
        if(visual_info) {
            // Creating the window:
            XSetWindowAttributes windowAttributes;
            windowAttributes.background_pixel = 0; // This sets the default background color to black
            windowAttributes.border_pixel = 0;
            windowAttributes.colormap = XCreateColormap(display, root_window, visual_info->visual, AllocNone);
            windowAttributes.event_mask = FocusChangeMask | StructureNotifyMask; // | KeyPressMask | KeyReleaseMask;
            windowAttributes.bit_gravity = StaticGravity; // Keep the old framebuffer data on resize instead of zeroing
            unsigned long attributeFlags = CWBorderPixel | CWBitGravity | CWColormap | CWEventMask;
            // We might not need bit gravity at all with openGL.
            window = XCreateWindow(display, root_window, 
                                   0, 0, // x, y
                                   window_width, window_height, 0, // border thickness
                                   visual_info->depth, InputOutput, visual_info->visual,
                                   attributeFlags, &windowAttributes);
            XStoreName(display, window, "a frontier thing");
            
            opengl_init(default_screen, fb_config);
            
            // Subscribing to atoms we are interested in:
            Atom WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);
            XSetWMProtocols(display, window, &WM_DELETE_WINDOW, 1);
            
            { // Sound:
                alsa_init();
                // @Incomplete: Check if we need to add manual fillahead, like on Windows.
            }
            
#if 0 // @Cleanup: Check if we need this at all?
            // X Input(the keyboard input extension from X, not XInput) setup for vanilla UTF8:
            XIM x_input_method = XOpenIM(display, 0, 0, 0);
            XIMStyles *styles = 0;
            XGetIMValues(x_input_method, XNQueryInputStyle, &styles, NULL);
            XIMStyle bestStyle = 0;
            for(int i = 0; i < styles->count_styles; ++i) {
                XIMStyle it = styles->supported_styles[i];
                if(it == (XIMPreeditNothing | XIMStatusNothing)) {
                    // The input method is standard
                    bestStyle = it;
                    break;
                }
            }
            XFree(styles);
            XIC xInputContext = XCreateIC(x_input_method, XNInputStyle, bestStyle, 
                                          XNClientWindow, window,
                                          XNFocusWindow, window, NULL);
#endif
            
            s32 xinput_opcode; // Used for event parsing
            { // XInput init
                {
                    s32 event, error;
                    if(!XQueryExtension(display, "XInputExtension", &xinput_opcode, &event, &error)) {
                        linux_print(STRING("Whoah there, you don't have XInput!\n"));
                        linux_exit(0);
                    }
                }
                
                {
                    s32 xinput_major = 2;
                    s32 xinput_minor = 0;
                    if(XIQueryVersion(display, &xinput_major, &xinput_minor) == BadRequest) {
                        char buffer[100];
                        print(buffer, 100, "Could not get XI2. Supported: %d.%d\n", xinput_major, xinput_minor);
                        linux_write(0, (const char *)buffer, 100);
                        linux_exit(0);
                    }
                }
                
                { // Selecting XInput events
                    XIEventMask event_mask = {};
                    u8 mask[3] = {};
                    event_mask.deviceid = XIAllMasterDevices; // Receive events for all peripherals
                    event_mask.mask_len = sizeof(mask);
                    event_mask.mask = mask;
                    
                    XISetMask(event_mask.mask, XI_RawMotion); // Mouse movement
                    // @Todo: Change APIs for mouse buttons because this one only accepts one rawpress and no rawrelease?
                    // Check whether this is still true with a mouse, though.
                    XISetMask(event_mask.mask, XI_RawButtonPress); // Mouse buttons
                    XISetMask(event_mask.mask, XI_RawButtonRelease);
                    XISetMask(event_mask.mask, XI_RawKeyPress); // Keyboard keys
                    XISetMask(event_mask.mask, XI_RawKeyRelease);
                    
                    // We have to set the root window when asking for raw input to
                    // receive input events even when we're not focused.
                    XISelectEvents(display, root_window, &event_mask, 1);
                }
            }
            
            XMapRaised(display, window);
            should_render = true;
            XFlush(display);
            
            // Creating an empty cursor:
            XColor empty_color = {};
            Pixmap empty_pixmap = XCreatePixmap(display, window, 1, 1, 1);
            empty_cursor = XCreatePixmapCursor(display, empty_pixmap, empty_pixmap, &empty_color, &empty_color, 0, 0);
            
            // Game code loading init:
            Game_Interface g_interface = {};
            if(linux_access(built_dll_full_path, X_OK) != 0) {
                if(linux_access(loaded_dll_full_path, X_OK == 0)) {
                    linux_rename(loaded_dll_full_path, built_dll_full_path);
                } else UNHANDLED;
            }
            
            //
            // Memory:
            //
            
            Memory_Block main_block;
            Memory_Block game_block;
            Memory_Block render_queue;
            
            main_block.size = GiB(2);
            main_block.mem = (u8 *)linux_mem_alloc(main_block.size);
            main_block.used = 0;
            sub_block(&game_block, &main_block, GiB(1));
            sub_block(&render_queue, &main_block, MiB(512));
            
            Program_State program_state = {};
            
            Implicit_Context main_context = {};
            
            g_interface = linux_load_game_code(built_dll_full_path, loaded_dll_full_path);
            g_interface.init_mem(&main_context, &game_block, &program_state);
            f64 start_frame_time = linux_get_secs();
            
            // Main loop:
            for(;;) {
                // Game code loading:
                if(linux_access(built_dll_full_path, X_OK) == 0) {
                    linux_unload_game_code(&g_interface);
                    g_interface = linux_load_game_code(built_dll_full_path, loaded_dll_full_path);
                }
                
                { // Event loop:
                    XEvent event = {};
                    XGenericEventCookie *cookie = &event.xcookie;
                    while(XPending(display) > 0) {
                        XNextEvent(display, &event);
                        
                        // Check whether the event comes from XInput:
                        // @Todo: Check which keyboard API is the simplest for getting scancodes.
                        if((cookie->type == GenericEvent) && (cookie->extension == xinput_opcode)) {
                            XGetEventData(display, cookie);
                            switch(cookie->evtype) {
                                case XI_RawMotion: {
                                    // linux_print((u8 *)"XI_RawMotion\n", sizeof("XI_RawMotion\n"));
                                } break;
                                case XI_RawKeyPress: {
                                    linux_print(STRING("XI_RawKeyPress\n"));
                                } break;
                                case XI_RawButtonPress: {
                                    linux_print(STRING("XI_RawButtonPress\n"));
                                } break;
                                case XI_RawButtonRelease: {
                                    linux_print(STRING("XI_RawButtonRelease\n"));
                                } break;
                                case XI_RawKeyRelease: {
                                    linux_print(STRING("XI_RawKeyRelease\n"));
                                } break;
                                
                            }
                            XFreeEventData(display, cookie);
                        }
                        
                        switch(event.type) {
                            case DestroyNotify: {
                                XDestroyWindowEvent *destroyEvent = (XDestroyWindowEvent *)&event;
                                if(destroyEvent->window == window) { // Unnecessary as long as we only have one window, but might as well check anyway.
                                    linux_exit(0);
                                }
                            } break;
                            
                            case MapNotify: {
                                // Used to notify when our window counts as being on the screen?
                                linux_print(STRING("MapNotify!\n"));
                            } break;
                            
                            case UnmapNotify: {
                                linux_print(STRING("UnmapNotify!\n"));
                            } break;
                            
                            case FocusIn: {
                                linux_print(STRING("FocusIn!\n"));
                                should_render = true;
                            } break;
                            
                            case FocusOut: {
                                linux_print(STRING("FocusOut!\n"));
                                should_render = false;
                            } break;
                            
                            case ClientMessage: {
                                linux_print(STRING("ClientMessage!\n"));
                                XClientMessageEvent *messageEvent = (XClientMessageEvent *)&event;
                                Atom messageAtom = (Atom)messageEvent->data.l[0];
                                if(messageAtom == WM_DELETE_WINDOW) {
                                    linux_exit(0);
                                }
                            } break;
                            
                            case KeyPress: {
                                // printf("KeyPress! \n");
                                XKeyPressedEvent *pressEvent = (XKeyPressedEvent *)&event;
                                /* if inputMode == TEXT_INPUT:
                                char utf8[4]; // Max length for a UTF8 character is 4 bytes
                                Status status = 0;
                                Xutf8LookupString(xInputContext, pressEvent, utf8, 4, 0, &status);
                                ASSERT(status != XBufferOverflow);
                                if(status == XLookupChars) {
                                // We got our UTF8 character in utf8!
                                }
                                */
                                // @Incomplete: Translate these to our own enum.
                                unsigned int keycode = pressEvent->keycode;
                            } break;
                            
                            case KeyRelease: { // @Duplication
                                // printf("KeyRelease!\n");
                                XKeyPressedEvent *pressEvent = (XKeyPressedEvent *)&event;
                                unsigned int keycode = pressEvent->keycode;
                            } break;
                        }
                    }
                }
                
                g_interface.run_frame(&main_context, &game_block, &program_state);
                
                s32 target_frame_ms = (should_render) ? 16 : 100;
                f64 end_frame_time = linux_get_secs();
                f64 sleep_ms = ((f64)target_frame_ms) - ((end_frame_time - start_frame_time) * 1000.0);
                
                // Sleep/End of frame:
                // @Cleanup: CPU sleep, replace later with GPU sleep.
                timespec time_to_sleep = {};
                time_to_sleep.tv_sec = 0;
                time_to_sleep.tv_nsec = (s64)(1000000.0 * sleep_ms);
                timespec time_to_sleep_remaining;
                linux_nanosleep(&time_to_sleep, &time_to_sleep_remaining);
                glXSwapBuffers(display, window);
                
                end_frame_time = linux_get_secs();
                
                /*
                char buffer[100];
                snprintf(buffer, 100, "Frame time: %fms\n", end_frame_time - start_frame_time);
                write(0, buffer, 100);
                */
                
                start_frame_time = end_frame_time;
            }
        }
    }
    return 0;
}
