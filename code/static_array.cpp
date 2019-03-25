
// Define these before #including the file:
#ifndef ITEM_TYPE
#define ITEM_TYPE u32
#endif

#ifndef ITEM_MAX_COUNT
#define ITEM_MAX_COUNT 256
#endif

#ifndef COUNT_TYPE
#define COUNT_TYPE usize
#endif

// Current default type name is simply [STRUCT]_[ITEM_TYPE], which
// is not robust to generating multiple [STRUCT]s with different
// [COUNT_TYPE]s or [ITEM_MAX_COUNT]s. Just don't use default names twice.
#ifndef STRUCT_TYPE
#define STRUCT_TYPE PASTE(static_array_, ITEM_TYPE)
#endif

#ifndef ITERATOR_TYPE
#define ITERATOR_TYPE COUNT_TYPE
#else
#error Static arrays expect ITERATOR_TYPE to be COUNT_TYPE by default.
#endif

struct STRUCT_TYPE {
    ITEM_TYPE *items;
    COUNT_TYPE count;
};

static void init(STRUCT_TYPE *array, Memory_Block *block) {
    array->items = push_array(block, ITEM_TYPE, ITEM_MAX_COUNT);
    array->count = 0;
}

static ITEM_TYPE *add(STRUCT_TYPE *array, ITEM_TYPE *to_add) {
    ASSERT(array->count < ITEM_MAX_COUNT);
    ITEM_TYPE *result = array->items + array->count++;
    *result = *to_add;
    return result;
}

static inline ITEM_TYPE *add(STRUCT_TYPE *array, ITEM_TYPE to_add) {
    return add(array, &to_add);
}

static ITEM_TYPE *reserve(STRUCT_TYPE *array, COUNT_TYPE to_reserve) {
    ASSERT((array->count + to_reserve) <= ITEM_MAX_COUNT);
    ITEM_TYPE *result = array->items + array->count;
    array->count += to_reserve;
    return result;
}

static void remove(STRUCT_TYPE *array, COUNT_TYPE index) {
    ASSERT(index < ITEM_MAX_COUNT);
    ASSERT(array->count > 0);
    array->items[index] = array->items[--array->count];
}

static void remove(STRUCT_TYPE *array, ITEM_TYPE *to_remove) {
    COUNT_TYPE index = (COUNT_TYPE)(to_remove - array->items);
    remove(array, index);
}

static inline ITERATOR_TYPE iteration_begin(STRUCT_TYPE *array) {
    ITERATOR_TYPE result = 0;
    return result;
}

static inline void iteration_step(STRUCT_TYPE *array, ITERATOR_TYPE *it) {
    *it += 1;
}

static inline ITEM_TYPE *iteration_get(STRUCT_TYPE *array, ITERATOR_TYPE *it) {
    if(*it < array->count) {
        return array->items + *it;
    } else {
        return 0;
    }
}

#undef ITEM_TYPE
#undef ITEM_MAX_COUNT
#undef COUNT_TYPE
#undef STRUCT_TYPE
#undef ITERATOR_TYPE
