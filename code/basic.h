
#define _STRINGISE(a) #a
#define STRINGISE(a) _STRINGISE(a)
#define _PASTE(a, b) a##b
#define PASTE(a, b) _PASTE(a, b)
#define _PASTE3(base, suffix1, suffix2) base##suffix1##suffix2
#define PASTE3(base, suffix1, suffix2) _PASTE3(base, suffix1, suffix2)
#define _PASTE4(base, suffix1, suffix2, suffix3) base##suffix1##suffix2##suffix3
#define PASTE4(base, suffix1, suffix2, suffix3) _PASTE4(base, suffix1, suffix2, suffix3)

#define LINE_VAR(name) PASTE(name, __LINE__)
#define SWAP(a, b) {auto LINE_VAR(swap) = a; a = b; b = LINE_VAR(swap);}
#define ALIGN_POW2(val, pow2) (((val) + ((pow2) - 1)) & (~((pow2) - 1)))
#define ALIGN(val, a) ((a) * (((val) + (a) - 1) / (a)))
#define WRITE_AND_ADVANCE_u8PTR(ptr, type, value) {*((type *)ptr) = value; ptr += sizeof(type);}

#define ENUM(name) namespace name{enum name##_type;} enum name::name##_type

#if GENESIS_DEV
#define ASSERT(expression) if(!(expression)){*(int *)0 = 0;}
#define CONDITIONAL_BREAKPOINT(condition) {if(condition){__debugbreak();}}
#else
#define ASSERT(expression)
#define CONDITIONAL_BREAKPOINT(condition)
#endif
#define UNHANDLED ASSERT(false)

#if _WIN32 || _WIN64

#define OS_WINDOWS 1

#define S8_MIN  (-128i64)
#define S16_MIN (-32768i64)
#define S32_MIN (-2147483648i64)
#define S64_MIN (-9223372036854775808i64)
#define S8_MAX  127i64
#define S16_MAX 32767i64
#define S32_MAX 2147483647i64
#define S64_MAX 9223372036854775807i64
#define U8_MAX  0xFFui64
#define U16_MAX 0xFFFFui64
#define U32_MAX 0xFFFFFFFFui64
#define U64_MAX 0xFFFFFFFFFFFFFFFFui64

#define KB(n)  n*1000ui64
#define MB(n)  n*1000000ui64
#define GB(n)  n*1000000000ui64
#define KiB(n) n*1024ui64
#define MiB(n) n*1048576ui64
#define GiB(n) n*1073741824ui64

typedef          __int8   s8;
typedef          __int16 s16;
typedef          __int32 s32;
typedef          __int64 s64;
typedef unsigned __int8   u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;
typedef          float   f32;
typedef          double  f64;
typedef          size_t  usize;

#if _M_X64 || _M_X86
#define LITTLE_ENDIAN 1
#elif _M_ARM || _M_ARM64
#define LITTLE_ENDIAN 0
#else
#error Unknown architecture.
#endif

#if _WIN64
#define X64 1
typedef s64 ptrdiff_t;
typedef s64 ssize;
#elif _WIN32
#define X32 1
typedef s32 ptrdiff_t;
typedef s32 ssize;
#endif

extern "C" {
    u64 __rdtsc(void);
}
#define READ_CPU_CLOCK() __rdtsc()
#else

typedef   signed char      s8;
typedef          short     s16;
typedef          int       s32;
typedef          long long s64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef          float     f32;
typedef          double    f64;

#if __linux__
#define OS_LINUX 1
// Do these work?
#define S8_MIN  (-128ll)
#define S16_MIN (-32768ll)
#define S32_MIN (-2147483648ll)
#define S64_MIN (-9223372036854775808ll)
#define S8_MAX  127ll
#define S16_MAX 32767ll
#define S32_MAX 2147483647ll
#define S64_MAX 9223372036854775807ll
#define U8_MAX  0xFFull
#define U16_MAX 0xFFFFull
#define U32_MAX 0xFFFFFFFFull
#define U64_MAX 0xFFFFFFFFFFFFFFFFull


#define KB(n)  n*1000ull
#define MB(n)  n*1000000ull
#define GB(n)  n*1000000000ull
#define KiB(n) n*1024ull
#define MiB(n) n*1048576ull
#define GiB(n) n*1073741824ull

#if __x86_64__
#define LITTLE_ENDIAN 1
#elif __ppc64__
#define LITTLE_ENDIAN 0
#elif __arm__
#define LITTLE_ENDIAN 0
#else
#error Unknown architecture.
#endif

#if __x86_64__ || __ppc64__
#define X64
#else
#define X32
#endif
#else
#error Unknown architecture.
#endif
#endif

#ifdef X64
#define CACHE_LINE_SIZE 64
#define UPTR_MAX U64_MAX
#define USIZE_MAX U64_MAX
#define USIZE_SIZE 8
#define USIZE_BITS 64
#define USIZE_MSB (0x8000000000000000)
#define PTRDIFF_SIZE 8
typedef          u64      usize;
typedef          s64      ssize;
typedef          s64       sptr;
typedef          u64       uptr;
typedef          s64  ptrdiff_t;
#elif X32
#define CACHE_LINE_SIZE 64
#define USIZE_MAX U32_MAX
#define USIZE_SIZE 4
#define USIZE_BITS 32
#define PTRDIFF_SIZE 4
typedef          u32      usize;
typedef          s32      ssize;
typedef          s32       sptr;
typedef          u32       uptr;
typedef          s32  ptrdiff_t;
#else
#error Unknown architecture.
#endif
typedef ptrdiff_t ssize_t;

#define ASSERT_TYPE_SIZES { \
    ASSERT(sizeof(u8) == 1); \
    ASSERT(sizeof(u16) == 2); \
    ASSERT(sizeof(u32) == 4); \
    ASSERT(sizeof(u64) == 8); \
    ASSERT(sizeof(s8) == 1); \
    ASSERT(sizeof(s16) == 2); \
    ASSERT(sizeof(s32) == 4); \
    ASSERT(sizeof(s64) == 8); \
    ASSERT(sizeof(f32) == 4); \
    ASSERT(sizeof(f64) == 8); \
    ASSERT(sizeof(usize) == USIZE_SIZE); \
    ASSERT(sizeof(ptrdiff_t) == PTRDIFF_SIZE);\
}

#define FOUR_BYTES_TO_U32(a, b, c, d) (((u32)a << 0) | ((u32)b << 8) | ((u32)c << 16) | ((u32)d << 24))
#define EIGHT_BYTES_TO_U64(a, b, c, d, e, f, g, h) (((u64)a << 0) | ((u64)b << 8) | ((u64)c << 16) | ((u64)d << 24) | ((u64)e << 32) | ((u64)f << 40) | ((u64)g << 48) | ((u64)h << 56))
#define GLOBAL_FUNCTION_POINTER(name) static name##_t *name;
#define STRUCT_FIELD_OFFSET(field, type) (&(((type *)0)->field))
#define ARRAY_LENGTH(arr) (sizeof(arr)/sizeof(arr[0]))
#define STRUCT_FIELD_OFFSET_SIZE(field, type) {STRUCT_FIELD_OFFSET(field, type), 8*sizeof((((type *)0)->field))}

// @Port: This doesn't work on Linux.
// @Untested: Check if VARARG_SIZE is still 8 on 32-bit.
#define VARARG_SIZE(type) (8)
#define BEGIN_VARARG(args, last_regular_arg) ((void)(args = (char *)(&(last_regular_arg)) + VARARG_SIZE(last_regular_arg)))
#define NEXT_VARARG(args, type) (*(type *)((args += VARARG_SIZE(type)) - VARARG_SIZE(type)))

#define DLLIST_REMOVE(element) { \
    element->prev->next = element->next; \
    element->next->prev = element->prev; \
    element->prev = element->next = 0; \
}

#define DLLIST_ADD(element, sentinel) { \
    element->prev = &sentinel; \
    element->next = sentinel.next; \
    sentinel.next = element; \
    element->next->prev = element; \
}

#ifndef USING_CRT
extern "C" {
    void *memset(void *mem, int value, usize size);
    void *memcpy(void *to, const void *from, usize size);
    int   memcmp(const void *a, const void *b, usize size);
    
#if OS_WINDOWS
#pragma function(memset)
#endif
    void *memset(void *mem, int value, usize size) {
        usize u64_count = size / 8;
        usize bytes_left = size % 8;
        long long value64 = ((long long)value << 32) | (long long)value;
        long long *m64 = (long long *)mem;
        usize i;
        
        for(i = 0; i < u64_count; ++i) {
            *m64++ = value64;
        }
        char *m8 = (char *)m64;
        for(i = 0; i < bytes_left; ++i) {
            m8[i] = (value >> (56 - (8 * i))) & 0xFF;
        }
        return mem;
    }
    
#if OS_WINDOWS
#pragma function(memcpy)
#endif
    void *memcpy(void *to, const void *from, usize size) {
        usize u64_count = size / 8;
        usize bytes_left = size % 8;
        unsigned long long *f64 = (unsigned long long *)from;
        unsigned long long *t64 = (unsigned long long *)to;
        unsigned char *f8 = (unsigned char *)(f64 + u64_count);
        unsigned char *t8 = (unsigned char *)(t64 + u64_count);
        usize i;
        
        for(i = 0; i < u64_count; ++i) {
            t64[i] = f64[i];
        }
        for(i = 0; i < bytes_left; ++i) {
            t8[i] = f8[i];
        }
        return to;
    }
    
#if OS_WINDOWS
#pragma function(memcmp)
#endif
    int memcmp(const void *a, const void *b, usize size) {
        unsigned char *at_a = (unsigned char *)a;
        unsigned char *at_b = (unsigned char *)b;
        
        for(usize i = 0; i < size; ++i) {
            if(at_a[i] == at_b[i]) {
                continue;
            }
            
            return at_a[i] - at_b[i];
        }
        
        return 0;
    }
}
#endif

static inline void zero_mem(void *mem, usize size) {
    memset(mem, 0, size);
}
static inline void mem_set(void *mem, s32 value, usize size) {
    memset(mem, value, size);
}
static inline void mem_copy(void *from, void *to, usize size) {
    memcpy(to, from, size);
}
static inline s32 mem_compare(void *a, void *b, usize size) {
    return memcmp(a, b, size);
}

struct string {
    u8 *data;
    usize length;
};

#define STRING(literal) string{(u8 *)literal, ARRAY_LENGTH(literal)-1}

//
// Memory:
//

struct Memory_Block {
    u8 *mem;
    usize size;
    usize used;
};
struct Scoped_Memory {
    Memory_Block *block;
    usize watermark;
    
    Scoped_Memory(Memory_Block *block) {
        this->block = block;
        this->watermark = block->used;
    }
    ~Scoped_Memory() {
        block->used = watermark;
    }
};
#define SCOPE_MEMORY(block) Scoped_Memory LINE_VAR(_scoped_memory) = Scoped_Memory((block))

static void *push_size(Memory_Block *block, usize size, const u32 align = 4) {
    ASSERT(align); // No align should be align = 1.
    uptr result = (uptr)block->mem + block->used;
    usize align_minus_one = (usize)align - 1;
    result = (result + align_minus_one) & ~align_minus_one;
    block->used = result - (uptr)block->mem + size;
    ASSERT(block->used <= block->size);
    return (void *)result;
}

#define push_struct(block, type, ...) (type *)push_size(block, sizeof(type), ## __VA_ARGS__)
#define push_array(block, type, count, ...) (type *)push_size(block, (count)*sizeof(type), ## __VA_ARGS__)

inline void sub_block(Memory_Block *sub, Memory_Block *main,
                      const usize size, const u32 align = 16) {
    sub->mem = (u8 *)push_size(main, size, align);
    if(sub->mem) {
        sub->size = size;
    } else {
        sub->size = 0;
    }
    sub->used = 0;
}

inline void sub_block(Memory_Block *sub, Memory_Block *main, const u32 align = 16) {
    usize align_minus_one = align - 1;
    sub->mem = (u8 *)((uptr)(main->mem + align_minus_one) & ~align_minus_one);
    sub->size = main->size - (sub->mem - main->mem);
    sub->used = 0;
    main->used = main->size;
}

#define __FOR(structure, suffix, it_name) for(auto _iterator##suffix = iteration_begin(structure); \
auto *it_name = iteration_get(structure, &_iterator##suffix); \
iteration_step(structure, &_iterator##suffix))
#define _FOR(structure, suffix, it_name) __FOR(structure, suffix, it_name)
#define FOR_NAMED(structure, it_name) _FOR(structure, __LINE__, it_name)
#define FOR(structure) FOR_NAMED(structure, it)

#define FORI_TYPED_NAMED(type, i_name, low, high) for(type i_name = (low); (i_name) < (high); ++i_name)
#define FORI_NAMED(i_name, low, high) FORI_TYPED_NAMED(ssize, i_name, low, high)
#define FORI_TYPED(type, low, high) FORI_TYPED_NAMED(type, i, low, high)
#define FORI(low, high) FORI_NAMED(i, low, high)
// FORI_REVERSE is inclusive, because otherwise we get shifty behaviour when 0 is the iteration bound.
// Maybe we should make FORI inclusive too, for consistency's sake?
#define FORI_REVERSE_TYPED_NAMED(type, i_name, high, low) for(type i_name = (high); (i_name) >= (low); --i_name)
#define FORI_REVERSE_NAMED(i_name, high, low) FORI_REVERSE_TYPED_NAMED(ssize, i_name, high, low)
#define FORI_REVERSE_TYPED(type, high, low) FORI_REVERSE_TYPED_NAMED(type, i, high, low)
#define FORI_REVERSE(high, low) FORI_REVERSE_NAMED(i, high, low)

#define _MIRROR_VARIABLE(struct_name, type, mirror_name, source_name) \
type mirror_name = *source_name; \
struct{\
    struct s{\
        type *mirror;\
        type *source;\
        ~s(){\
            *source = *mirror;\
        }\
    }s;\
} struct_name;\
struct_name.s.mirror = &mirror_name;\
struct_name.s.source = source_name;
#define MIRROR_VARIABLE(type, name, source) _MIRROR_VARIABLE(LINE_VAR(_mirror_variable), type, name, source)

#define CAST(type, expr) ((type)(expr))
// @Untested
#define BITCAST(type, expr) (*((type*)(&(expr))))

// Any

struct any32 {
    union {
        s32 s;
        u32 u;
        f32 f;
        
        s16 by_s16[2];
        u16 by_u16[2];
        
        s8 by_s8[4];
        u8 by_u8[4];
    };
};
inline any32 make_any32(s32 s) {
    any32 result;
    result.s = s;
    return result;
}
inline any32 make_any32(u32 u) {
    any32 result;
    result.u = u;
    return result;
}
inline any32 make_any32(f32 f) {
    any32 result;
    result.f = f;
    return result;
}

// @Temporary
#define _PRINT_ARRAY(array, len, str, printed, i) FORI_NAMED(i, 0, n){printed += print(str.data + printed, str.length - printed, "%f\n", array[i]);}
#define PRINT_ARRAY(array, len, str, printed) _PRINT_ARRAY(array, len, str, printed, LINE_VAR(i))

#if LITTLE_ENDIAN
static inline u16 read_u16_be(u16 *ptr) {
    u16 result = (((u8 *)ptr)[0] << 8) + ((u8 *)ptr)[1];
    return result;
}
static inline s16 read_s16_be(s16 *ptr) {
    s16 result = (((u8 *)ptr)[0] << 8) + ((u8 *)ptr)[1];
    return result;
}
static inline u32 read_u32_be(u32 *ptr) {
    u32 result = (((u8 *)ptr)[0] << 24) + (((u8 *)ptr)[1] << 16) + (((u8 *)ptr)[2] << 8) + (((u8 *)ptr)[3]);
    return result;
}
static inline u16 read_u16_le(u16 *ptr) {
    return *ptr;
}
static inline s16 read_s16_le(s16 *ptr) {
    return *ptr;
}
static inline u32 read_u32_le(u32 *ptr) {
    return *ptr;
}
#else
static inline u16 read_u16_le(u16 *ptr) {
    u16 result = (((u8 *)ptr)[0] << 8) + ((u8 *)ptr)[1];
    return result;
}
static inline s16 read_s16_le(s16 *ptr) {
    s16 result = (((u8 *)ptr)[0] << 8) + ((u8 *)ptr)[1];
    return result;
}
static inline u32 read_u32_le(u32 *ptr) {
    u32 result = (((u8 *)ptr)[0] << 24) + (((u8 *)ptr)[1] << 16) + (((u8 *)ptr)[2] << 8) + (((u8 *)ptr)[3]);
    return result;
}
static inline u16 read_u16_be(u16 *ptr) {
    return *ptr;
}
static inline s16 read_s16_be(s16 *ptr) {
    return *ptr;
}
static inline u32 read_u32_be(u32 *ptr) {
    return *ptr;
}
#endif