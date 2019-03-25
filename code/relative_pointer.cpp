
#ifndef VALUE_TYPE
#define VALUE_TYPE void
#endif

#ifndef OFFSET_TYPE
#define OFFSET_TYPE sptr
#endif

#ifndef RELPTR_TYPE
#define RELPTR_TYPE PASTE4(relative_pointer_, VALUE_TYPE, _, OFFSET_TYPE)
#endif

struct RELPTR_TYPE {
    OFFSET_TYPE offset;
};

inline VALUE_TYPE *operator->(RELPTR_TYPE &relptr) {
    ASSERT(relptr.offset);
    return (VALUE_TYPE *)(((u8 *)&relptr) + relptr.offset);
}

inline void operator=(RELPTR_TYPE &relptr, VALUE_TYPE *absptr) {
    if(absptr) {
        sptr diff = absptr - &relptr;
        ASSERT((OFFSET_TYPE)diff == diff);
        relptr.offset = diff;
    } else {
        relptr.offset = 0;
    }
}

#undef VALUE_TYPE
#undef OFFSET_TYPE
#undef RELPTR_TYPE
