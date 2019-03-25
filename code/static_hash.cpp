
// Define these before #including the file:
#ifndef KEY_TYPE
#define KEY_TYPE u32
#endif

#ifndef ITEM_TYPE
#define ITEM_TYPE u32
#endif

#ifndef ASSERT_ON_HASH_COLLISIONS
#define ASSERT_ON_HASH_COLLISIONS (0)
#endif

#ifndef ASSERT_ON_INSERT_FAILURE
#define ASSERT_ON_INSERT_FAILURE (1)
#endif

#ifndef MAX_ITEM_COUNT
#define MAX_ITEM_COUNT (256)
#endif

#ifndef MAX_PROBE_DISTANCE
#define MAX_PROBE_DISTANCE MAX_ITEM_COUNT
#endif

// Default type name is simply [STRUCT]_[ITEM_TYPE], which
// is *not* robust to generating multiple [STRUCT]s with different
// [COUNT_TYPE]s or [MAX_ITEM_COUNT]s but the same [ITEM_TYPE].
// In that case, just name them!
#ifndef STRUCT_TYPE
#define STRUCT_TYPE PASTE(static_hash_, ITEM_TYPE)
#endif

#ifndef UNINITIALISED_HASH_VALUE
#define UNINITIALISED_HASH_VALUE (0)
#endif

#ifndef HASH_FUNCTION
#define HASH_FUNCTION PASTE(STRUCT_TYPE, _default_hash_function)
static KEY_TYPE HASH_FUNCTION(ITEM_TYPE *item) {
    u8 *s = (u8 *)item;
    KEY_TYPE hash = 0;
    
    for(usize i = 0; i < sizeof(ITEM_TYPE); ++i) {
        hash = (hash << 7) + (hash >> 25) + s[i];
    }
    hash = hash + (hash >> 16);
    if(!hash) {
        hash = 1;
    }
    
    ASSERT(hash != UNINITIALISED_HASH_VALUE);
    return hash;
}
static KEY_TYPE HASH_FUNCTION(ITEM_TYPE item) {
    return HASH_FUNCTION(&item);
}
#endif

struct STRUCT_TYPE {
    KEY_TYPE *keys;
    ITEM_TYPE *items;
};

static void init(STRUCT_TYPE *table, Memory_Block *block) {
    table->keys = push_array(block, KEY_TYPE, MAX_ITEM_COUNT);
    table->items = push_array(block, ITEM_TYPE, MAX_ITEM_COUNT);
    mem_set(table->keys, UNINITIALISED_HASH_VALUE, sizeof(KEY_TYPE) * MAX_ITEM_COUNT);
}

static inline void empty(STRUCT_TYPE *table) {
    mem_set(table->keys, UNINITIALISED_HASH_VALUE, sizeof(KEY_TYPE) * MAX_ITEM_COUNT);
}

static ITEM_TYPE *add(STRUCT_TYPE *table, ITEM_TYPE *to_add) {
    KEY_TYPE key = HASH_FUNCTION(to_add);
    usize lookup = key;
    usize probe = 0;
    
    do {
        // @Speed: Hopefully, MAX_ITEM_COUNT being a constant will
        // turn this into a & at compile time when it's a power of 2.
        lookup = (key + probe) % MAX_ITEM_COUNT;
        KEY_TYPE found_key = table->keys[lookup];
        
        if(found_key == UNINITIALISED_HASH_VALUE) {
            ITEM_TYPE *empty_slot = table->items + lookup;
            
            table->keys[lookup] = key;
            *empty_slot = *to_add;
            return empty_slot;
        }
        
        ++probe;
    } while(probe < MAX_PROBE_DISTANCE);
    
#if ASSERT_ON_INSERT_FAILURE
    UNHANDLED;
#endif
    
    return 0;
}

static inline ITEM_TYPE *add(STRUCT_TYPE *table, ITEM_TYPE to_add) {
    return add(table, &to_add);
}

static ITEM_TYPE *find(STRUCT_TYPE *table, ITEM_TYPE *item) {
    KEY_TYPE key = HASH_FUNCTION(item);
    usize lookup;
    usize probe = 0;
    
    do {
        lookup = (key + probe) % MAX_ITEM_COUNT;
        KEY_TYPE found_key = table->keys[lookup];
        
        if(found_key == key) {
            // Check for hash collisions:
#if ASSERT_ON_HASH_COLLISIONS
            ASSERT(!mem_compare(table->items + lookup, item, sizeof(ITEM_TYPE)));
#else
            if(!mem_compare(table->items + lookup, item, sizeof(ITEM_TYPE))) {
#endif
                return table->items + lookup;
#if !ASSERT_ON_HASH_COLLISIONS
            }
#endif
        }
        
        ++probe;
    } while(probe < MAX_PROBE_DISTANCE);
    
    return 0;
}

static inline ITEM_TYPE *find(STRUCT_TYPE *table, ITEM_TYPE item) {
    return find(table, &item);
}

#undef KEY_TYPE
#undef ITEM_TYPE
#undef MAX_ITEM_COUNT
#undef STRUCT_TYPE
#undef HASH_FUNCTION
#undef MAX_PROBE_DISTANCE
#undef ASSERT_ON_HASH_COLLISIONS
#undef UNINITIALISED_HASH_VALUE
