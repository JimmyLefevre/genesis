
static bitwriter make_bitwriter(void *buffer) {
    
    bitwriter result = {};
    result.buffer = (u32 *)buffer;
    
    return(result);
}

#define WRITE_BITS_TYPED(bw, type, val) write_bits(bw, sizeof(type), val)
#define READ_BITS_TYPED(br, type) CAST(type, read_bits(br, sizeof(type)))

static void write_bits(bitwriter *bw, u32 bits_to_write, u64 value) {
    
    ASSERT(bits_to_write <= 32);
    
    bw->scratch |= (value << bw->bits_in_scratch);
    bw->bits_in_scratch += bits_to_write;
    if(bw->bits_in_scratch > 32) {
        *(bw->buffer + bw->words_written++) = (bw->scratch & 0xFFFFFFFF);
        bw->scratch = (bw->scratch >> 32);
        bw->bits_in_scratch -= 32;
    }
}

static u32 flush_bits(bitwriter *bw) {
    
    u32 result = (bw->words_written * 4) + ((bw->bits_in_scratch+7)/8);
    
    *(bw->buffer + bw->words_written++) = (bw->scratch & 0xFFFFFFFF);
    bw->bits_in_scratch = 0;
    
    return(result);
}

static bitreader make_bitreader(void *buffer) {
    bitreader result = {};
    result.buffer = (u32 *)buffer;
    
    return(result);
}

static any32 read_bits(bitreader *br, u32 bits_to_read) {
    if(br->bits_in_scratch < bits_to_read) {
        ASSERT(br->bits_in_scratch <= 32);
        br->scratch |= ((u64)br->buffer[br->words_read++] << br->bits_in_scratch);
        br->bits_in_scratch += 32;
    }
    
    u64 mask = (1 << bits_to_read) - 1;
    any32 result;
    result.u = br->scratch & ((1LL << bits_to_read) - 1);
    br->scratch = (br->scratch >> bits_to_read);
    br->bits_in_scratch -= bits_to_read;
    
    return(result);
}
