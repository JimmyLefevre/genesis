
// Define these before #including the file:
#ifndef ITEM_TYPE
#define ITEM_TYPE u32
#endif

#ifndef ITEM_MAX_COUNT
#define ITEM_MAX_COUNT 4
#endif

#ifndef COUNT_TYPE
#define COUNT_TYPE usize
#endif

// These are internal:
// Current type name is simply [STRUCT]_[ITEM_TYPE], which
// is not robust to generating multiple [STRUCT]s with different
// [COUNT_TYPE]s or [ITEM_MAX_COUNT]s.
#define STRUCT_TYPE PASTE(static_array_, ITEM_TYPE)
#define ITERATOR_TYPE COUNT_TYPE

struct STRUCT_TYPE {
    
};