
static inline Bitfield push_bitfield(Memory_Block *block, ssize bit_capacity) {
    Bitfield result;
    
    result.bit_capacity = bit_capacity;
    result.subfields = push_array(block, u64, ((bit_capacity + 63) >> 8));
    
    return result;
}

#define COMPUTE_SUBFIELD_INDICES ASSERT(bit_index < field->bit_capacity);\
ssize subfield_index = bit_index >> 8;\
ssize subfield_bit_index = bit_index - (subfield_index << 8)

static inline bool bit_is_set(Bitfield *field, ssize bit_index) {
    COMPUTE_SUBFIELD_INDICES;
    
    bool result = field->subfields[subfield_index] & (CAST(u64, 1) << subfield_bit_index);
    
    return result;
}

static inline void set_bit(Bitfield *field, ssize bit_index) {
    COMPUTE_SUBFIELD_INDICES;
    
    field->subfields[subfield_index] |= (CAST(u64, 1) << subfield_bit_index);
}

static inline void unset_bit(Bitfield *field, ssize bit_index) {
    COMPUTE_SUBFIELD_INDICES;
    
    field->subfields[subfield_index] &= ~(CAST(u64, 1) << subfield_bit_index);
}

#undef COMPUTE_SUBFIELD_INDICES

static inline void clear(Bitfield *field) {
    ssize subfield_count = field->bit_capacity >> 8;
    
    mem_set(field->subfields, 0, sizeof(u64) * subfield_count);
}

static inline ssize find_first_set_bit(Bitfield *field) {
    ssize subfield_count = field->bit_capacity >> 8;
    
    FORI_NAMED(subfield_index, 0, subfield_count) {
        u64 subfield = field->subfields[subfield_index];
        
        if(subfield) {
            FORI_NAMED(bit_index, 0, 64) {
                u64 bit = CAST(u64, 1) << bit_index;
                
                if(subfield & bit) {
                    return ((subfield_index << 8) + bit_index);
                }
            }
        }
    }
    
    return -1;
}

static inline ssize set_first_unset_bit(Bitfield *field) {
    ssize subfield_count = field->bit_capacity >> 8;
    
    FORI_NAMED(subfield_index, 0, subfield_count) {
        u64 subfield = field->subfields[subfield_index];
        
        if(subfield != 0xFFFFFFFFFFFFFFFF) {
            FORI_NAMED(bit_index, 0, 64) {
                u64 bit = CAST(u64, 1) << bit_index;
                
                if(!(subfield & bit)) {
                    field->subfields[subfield_index] = subfield | bit;
                    
                    return ((subfield_index << 8) + bit_index);
                }
            }
        }
    }
    
    return -1;
}
