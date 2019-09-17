
#define TAU 6.28318530717f
#define PI  3.14159265358f
#define GRAVITY 9.81f
#define LARGE_FLOAT 3.e+37f

#define IDIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))
#define ROUND_UP_TO_POWER_OF_TWO(a, pow) (((a) + ((pow)-1)) & (~((pow)-1)))

struct v2 {
    f32 x;
    f32 y;
};

struct v3 {
    f32 x;
    f32 y;
    f32 z;
};

struct v4 {
    union {
        struct {
            f32 x;
            f32 y;
            f32 z;
            f32 w;
        };
        struct {
            f32 r;
            f32 g;
            f32 b;
            f32 a;
        };
        f32 by_element[4];
    };
};

struct v2u {
    union {
        struct {
            u32 x;
            u32 y;
        };
        u32 e[2];
    };
};

struct v2s {
    s32 x;
    s32 y;
};

struct v2s16 {
    union {
        struct {
            s16 x;
            s16 y;
        };
        s16 E[2];
    };
};

struct v2s8 {
    union {
        struct {
            s8 x;
            s8 y;
        };
        s8 E[2];
    };
};

struct v2sf {
    s32 x;
    f32 y;
};

struct rect2 {
    union {
        struct {
            f32 left;
            f32 bottom;
            f32 right;
            f32 top;
        };
        struct {
            v2 min;
            v2 max;
        };
    };
};

struct rect2s {
    union {
        struct {
            s32 left;
            s32 bottom;
            s32 right;
            s32 top;
        };
        struct {
            v2s min;
            v2s max;
        };
        s32 E[4];
    };
};

struct rect2s16 {
    union {
        struct {
            s16 left;
            s16 bottom;
            s16 right;
            s16 top;
        };
        struct {
            v2s16 min;
            v2s16 max;
        };
    };
};

struct tri {
    union {
        struct {
            v2 a;
            v2 b;
            v2 c;
        };
        v2 E[3];
    };
};

static inline v2 V2(const f32 x, const f32 y) {
    v2 result;
    result.x = x;
    result.y = y;
    return result;
}

static inline v2 V2(const s32 x, const s32 y) {
    v2 result;
    result.x = (f32)x;
    result.y = (f32)y;
    return result;
}

static inline v2 V2(const f32 v) {
    v2 result;
    result.x = v;
    result.y = v;
    return result;
}

static inline v2 V2(const v2s v) {
    v2 result;
    result.x = (f32)v.x;
    result.y = (f32)v.y;
    return result;
}

static inline v2 V2(const s32 v[2]) {
    v2 result;
    result.x = (f32)v[0];
    result.y = (f32)v[1];
    return result;
}

static inline v2 V2() {
    v2 result;
    result.x = 0.0f;
    result.y = 0.0f;
    return result;
}

static inline v2 operator+(const v2 a, const v2 b) {
    return V2(a.x + b.x, a.y + b.y);
}

static inline v2 operator-(const v2 a, const v2 b) {
    return V2(a.x - b.x, a.y - b.y);
}

static inline v2 operator-(const v2 a) {
    return V2(-a.x, -a.y);
}

static inline v2 operator*(const v2 a, const f32 b) {
    return V2(a.x * b, a.y * b);
}

static inline v2 operator/(const v2 a, const f32 b) {
    return V2(a.x / b, a.y / b);
}

static inline v2 operator+=(v2 &a, const v2 b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}

static inline v2 operator-=(v2 &a, const v2 b) {
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

static inline v2 operator*=(v2 &a, const f32 b) {
    a.x *= b;
    a.y *= b;
    return a;
}

static inline v2 operator/=(v2 &a, const f32 b) {
    a.x /= b;
    a.y /= b;
    return a;
}

static inline v3 V3(const f32 x, const f32 y, const f32 z) {
    v3 result = {x, y, z};
    return result;
}

static inline v3 V3(const f32 v) {
    v3 result = {v, v, v};
    return result;
}

static inline v3 V3() {
    v3 result = {};
    return result;
}

static inline v3 operator+(const v3 a, const v3 b) {
    return V3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline v3 operator-(const v3 a, const v3 b) {
    return V3(a.x - b.x, a.y - b.y, a.z + b.z);
}

static inline v3 operator-(const v3 a) {
    return V3(-a.x, -a.y, -a.z);
}

static inline v3 operator*(const v3 a, const f32 b) {
    return V3(a.x * b, a.y * b, a.z * b);
}

static inline v3 operator/(const v3 a, const f32 b) {
    return V3(a.x / b, a.y / b, a.z / b);
}

static inline v3 operator+=(v3 &a, const v3 b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

static inline v3 operator-=(v3 &a, const v3 b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

static inline v3 operator*=(v3 &a, const f32 b) {
    a.x *= b;
    a.y *= b;
    a.z *= b;
    return a;
}

static inline v3 operator/=(v3 &a, const f32 b) {
    a.x /= b;
    a.y /= b;
    a.z /= b;
    return a;
}

static inline v4 V4(const f32 x, const f32 y, const f32 z, const f32 w) {
    v4 result = {x, y, z, w};
    return result;
}

static inline v4 V4(const f32 v) {
    v4 result = {v, v, v, v};
    return result;
}

static inline v4 V4() {
    v4 result = {};
    return result;
}

static inline v4 V4(v3 xyz, f32 w) {
    v4 result = {xyz.x, xyz.y, xyz.z, w};
    return result;
}

static inline v4 operator+(const v4 a, const v4 b) {
    return V4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

static inline v4 operator-(const v4 a, const v4 b) {
    return V4(a.x - b.x, a.y - b.y, a.z + b.z, a.w + b.w);
}

static inline v4 operator-(const v4 a) {
    return V4(-a.x, -a.y, -a.z, -a.w);
}

static inline v4 operator*(const v4 a, const f32 b) {
    return V4(a.x * b, a.y * b, a.z * b, a.w * b);
}

static inline v4 operator/(const v4 a, const f32 b) {
    return V4(a.x / b, a.y / b, a.z / b, a.w / b);
}

static inline v4 operator+=(v4 &a, const v4 b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    a.w += b.w;
    return a;
}

static inline v4 operator-=(v4 &a, const v4 b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    a.w -= b.w;
    return a;
}

static inline v4 operator*=(v4 &a, const f32 b) {
    a.x *= b;
    a.y *= b;
    a.z *= b;
    a.w *= b;
    return a;
}

static inline v4 operator/=(v4 &a, const f32 b) {
    a.x /= b;
    a.y /= b;
    a.z /= b;
    a.w /= b;
    return a;
}

static inline v2u V2U(const u32 x, const u32 y) {
    v2u result;
    result.x = x;
    result.y = y;
    return result;
}
static inline v2u V2U() {
    v2u result;
    result.x = 0;
    result.y = 0;
    return result;
}

static inline v2u operator+(const v2u a, const v2u b) {
    return V2U(a.x + b.x, a.y + b.y);
}

static inline v2u operator-(const v2u a, const v2u b) {
    return V2U(a.x - b.x, a.y - b.y);
}

static inline v2u operator*(const v2u a, const u32 b) {
    return V2U(a.x * b, a.y * b);
}

static inline v2u operator/(const v2u a, const u32 b) {
    return V2U(a.x / b, a.y / b);
}

static inline v2u operator+=(v2u &a, const v2u b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}

static inline v2u operator-=(v2u &a, const v2u b) {
    a.x -= b.x;
    a.y -= b.y;
    return a;
}

static inline v2u operator*=(v2u &a, const u32 b) {
    a.x *= b;
    a.y *= b;
    return a;
}

static inline v2u operator/=(v2u &a, const u32 b) {
    a.x /= b;
    a.y /= b;
    return a;
}


static inline v2s V2S(const s32 x, const s32 y) {
    v2s result = {x, y};
    return result;
}
static inline v2s V2S(v2s16 a) {
    v2s result = {a.x, a.y};
    return result;
}
static inline v2s V2S() {
    v2s result = {};
    return result;
}

static inline v2s operator+(const v2s a, const v2s b) {
    return V2S(a.x + b.x, a.y + b.y);
}
static inline v2s operator-(const v2s a, const v2s b) {
    return V2S(a.x - b.x, a.y - b.y);
}
static inline v2s operator-(const v2s a) {
    return V2S(-a.x, -a.y);
}
static inline v2s operator*(const v2s a, const s32 b) {
    return V2S(a.x * b, a.y * b);
}
static inline v2s operator/(const v2s a, const s32 b) {
    return V2S(a.x / b, a.y / b);
}
static inline v2s operator+=(v2s &a, const v2s b) {
    a.x += b.x;
    a.y += b.y;
    return a;
}
static inline v2s operator-=(v2s &a, const v2s b) {
    a.x -= b.x;
    a.y -= b.y;
    return a;
}
static inline v2s operator*=(v2s &a, const v2s b) {
    a.x *= b.x;
    a.y *= b.y;
    return a;
}
static inline v2s operator/=(v2s &a, const v2s b) {
    a.x /= b.x;
    a.y /= b.y;
    return a;
}

static inline void v2s_add(v2s *a, v2s b) {
    a->x += b.x;
    a->y += b.y;
}

static inline v2s16 V2S16(s16 x, s16 y) {
    v2s16 result;
    result.x = x;
    result.y = y;
    return(result);
}
static inline v2s16 V2S16(s16 v) {
    return(V2S16(v, v));
}
static inline v2s16 V2S16(v2s a) {
    v2s16 result = {CAST(s16, a.x), CAST(s16, a.y)};
    return(result);
}
static inline v2s16 V2S16() {
    return(V2S16(0, 0));
}

static inline bool v2s16_equal(v2s16 a, v2s16 b) {
    return((a.x == b.x) && (a.y == b.y));
}

static inline void v2s16_add(v2s16 *a, v2s16 b) {
    a->x += b.x;
    a->y += b.y;
}

static inline v2s16 v2s16_diff(v2s16 a, v2s16 b) {
    v2s16 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return(result);
}

static inline v2s16 v2s16_sum(v2s16 a, v2s16 b) {
    v2s16 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return(result);
}

static inline v2s16 operator+(const v2s16 a, const v2s16 b) {
    return V2S16(a.x + b.x, a.y + b.y);
}
static inline v2s16 operator-(const v2s16 a, const v2s16 b) {
    return V2S16(a.x - b.x, a.y - b.y);
}

static inline s32 s_clamp(s32 v, s32 low_bound, s32 high_bound) {
    if(v <= low_bound) {
        return low_bound;
    }
    if(v >= high_bound) {
        return high_bound;
    }
    return v;
}

static inline bool rolling_tgt_u32(u32 a, u32 b) {
    return (((a > b) && (a - b <= 2147483648)) ||
            ((a < b) && (b - a >  2147483648)));
}

static inline bool rolling_tge_u32(u32 a, u32 b) {
    return (((a >= b) && (a - b <= 2147483648)) ||
            ((a <= b) && (b - a >  2147483648)));
}

static inline f32 f_move_toward(f32 from, f32 to, f32 amount){
    f32 result = from + amount;
    
    if(((amount > 0) && (result > to)) ||
       ((amount < 0) && (result < to))) {
        result = to;
    }
    
    return(result);
}

static inline s32 f_ceil_s(f32 a) {
    s32 truncate = (s32)a;
    s32 result = (a > truncate) ? (truncate + 1) : truncate;
    return result;
}

static inline u32 f_ceil_u(f32 a) {
    u32 truncate = (u32)a;
    u32 result = (a > truncate) ? (truncate + 1) : truncate;
    return result;
}

static inline s32 s_abs(s32 s) {
    u32 u = BITCAST(u32, s);
    
    s32 complement = s >> 31;
    s32 add = u >> 31;
    s32 result = (s ^ complement) + add;
    
    return result;
}
static inline s16 s_abs(s16 s) {
    u16 u = BITCAST(u16, s);
    
    s16 complement = s >> 15;
    s16 add = u >> 15;
    s16 result = (s ^ complement) + add;
    
    return result;
}

static inline s32 s_min(s32 a, s32 b) {
    s32 a_lt_b = (a - b) >> 31;
    s32 result = (a & a_lt_b) | (b & ~a_lt_b);
    return result;
}

static inline s64 s_min64(s64 a, s64 b) {
    s64 a_lt_b = (a - b) >> 63;
    s64 result = (a & a_lt_b) | (b & ~a_lt_b);
    return result;
}

static inline ssize s_minsize(ssize a, ssize b) {
    ssize a_lt_b = (a - b) >> (USIZE_BITS - 1);
    ssize result = (a & a_lt_b) | (b & ~a_lt_b);
    return result;
}

static inline void xorshift(u32 *state) {
    u32 out = *state ^ (*state << 13);
    out = out ^ (out >> 17);
    out = out ^ (out << 5);
    *state = out;
}

#define TEST_GT(a, b, bits) (((b) - (a)) >> ((bits) - 1))
#define TEST_LT(a, b, bits) (((a) - (b)) >> ((bits) - 1))
#define SELECT(mask, a, b) (((a) & (mask)) | ((b) & (~(mask))))

#define DEFINE_SLL(bits) static inline u##bits sll(u##bits a, u32 shift) {return a << shift;}\
static inline s##bits sll(s##bits a, u32 shift) {return CAST(u##bits, a) << shift;}
#define DEFINE_SR(bits) static inline u##bits srl(u##bits a, u32 shift) {return a >> shift;}\
static inline u##bits srl(s##bits a, u32 shift) {return CAST(u##bits, a) >> shift;}\
static inline s##bits sra(s##bits a, u32 shift) {return a >> shift;}\
static inline s##bits sra(u##bits a, u32 shift) {return CAST(s##bits, a) >> shift;}
DEFINE_SLL(8);
DEFINE_SLL(16);
DEFINE_SLL(32);
DEFINE_SLL(64);
DEFINE_SR(8);
DEFINE_SR(16);
DEFINE_SR(32);
DEFINE_SR(64);
#undef DEFINE_SLL
#undef DEFINE_SR

static inline u32 f_test_lt(f32 a, f32 b) {
    f32 diff = a - b;
    s32 mask = *((s32 *)&diff) >> 31;
    return mask;
}

static inline f32 f_select(u32 mask, f32 a, f32 b) {
    u32 ua = *(u32 *)&a;
    u32 ub = *(u32 *)&b;
    u32 uresult = (ua & mask) | (ub & ~mask);
    return *(f32 *)&uresult;
}

static inline u32 boolean_mask(u32 b) {
    const s32 s   = b;
    const s32 neg = -s;
    const s32 result = (s >> 31) | (neg >> 31);
    return result;
}

// @Untested: There should probably be a u_max too.
// How does this handle negative a for signed and unsigned?
static inline s32 s_max(s32 a, s32 b) {
    s32 a_lt_b = (a - b) >> 31;
    s32 result = (a & ~a_lt_b) | (b & a_lt_b);
    return result;
}

static inline u32 f_round_to_u(f32 a) {
    u32 result = (u32)(a + 0.5f);
    
    return(result);
}

static inline s32 floor_s(f32 a) {
    const s32 sa = (s32)a;
    const f32 fa_minus_sa = a - (f32)sa;
    const s32 nudge_mask = (*(s32 *)&fa_minus_sa) >> 31;
    const s32 nudge = -1 & nudge_mask;
    const s32 result = sa + nudge;
    return result;
}

static inline s32 ceil_s(f32 a) {
    const s32 sa = (s32)a;
    const f32 sa_minus_fa = (f32)sa - a;
    const s32 sa_lt_fa = (*(s32 *)&sa_minus_fa) >> 31;
    const s32 nudge = 1 & sa_lt_fa;
    const s32 result = sa + nudge;
    return result;
}

// Taken from Charles Bloom's article:
// https://cbloomrants.blogspot.com/2009/06/06-21-09-fast-exp-log.html
static inline f32 f_exp(f32 a) {
    union {
        f32 f;
        u32 u;
    } exp;
    // Move a to the exponent and convert exp2 to exp:
    const f32 expconst0 = (1 << 23) / 0.69314718f;
    // Negate floating-point bias and add our own bias:
    // By default, the result is >= exp.
    const u32 expconst1 = (127 << 23) - 361007;
    exp.u = (u32)(expconst0 * a + expconst1);
    return exp.f;
}
static inline f32 f_log2(f32 a) {
    union {
        f32 f;
        u32 u;
    } fu;
    fu.f = a;
    f32 vv = (f32)(fu.u - (127 << 23));
    vv *= (1.0f/8388608.0f);
    fu.u = (fu.u & 0x7FFFFF) | (127 << 23);
    f32 frac = fu.f - 1.0f;
    const f32 correction = 0.346573583f;
    return vv + correction * frac * (1.0f - frac);
}
static inline f32 f_pow2(f32 a) {
    any32 result;
    
    f32 exp = a * 8388608.0f; // 8388608 = (1 << 23)
    
    result.u = (CAST(u32, exp) + (127 << 23));
    
    return result.f;
}

static inline f32 f_reverse_lerp(f32 from, f32 to, f32 at) {
    f32 result = (at - from) / (to - from);
    
    return result;
}

static inline f32 f_lerp(f32 from, f32 to, f32 at){
    f32 result = ((1.0f - at) * from) + (at * to);
    
    return(result);
}

static inline f32 f_abs(f32 a){
    u32 u = (*(u32 *)&a) & 0x7FFFFFFF;
    f32 result = *(f32 *)&u;
    
    return(result);
}

static inline v2 v2_abs(const v2 a) {
    return V2(f_abs(a.x), f_abs(a.y));
}

static inline f32 f_min(f32 a, f32 b) {
    f32 a_minus_b = a - b;
    s32 a_lt_b = BITCAST(s32, a_minus_b) >> 31;
    u32 uresult = (BITCAST(u32, a) & a_lt_b) | (BITCAST(u32, b) & ~a_lt_b);
    f32 result = BITCAST(f32, uresult);
    
    f32 sanity_check = (a < b) ? a : b;
    ASSERT(result == sanity_check);
    
    return result;
}

static inline f32 f_max(f32 a, f32 b) {
    f32 a_minus_b = a - b;
    s32 a_lt_b = BITCAST(s32, a_minus_b) >> 31;
    u32 uresult = (BITCAST(u32, a) & ~a_lt_b) | (BITCAST(u32, b) & a_lt_b);
    f32 result = BITCAST(f32, uresult);
    
    f32 sanity_check = (a < b) ? b : a;
    ASSERT(result == sanity_check);
    
    return result;
}

static inline f32 f_mod(f32 a, f32 b) {
    f32 result = a - b * (f32)((s32)(a / b));
    return result;
}

static inline f32 inv_sqrt(f32 a) {
    f32 half = a * 0.5f;
    s32 ai = *(int *)&a;
    ai = 0x5f375a86 - (ai >> 1);
    a = *(float *)&ai;
    a = a * (1.5f - half * a * a);
    return a;
}
static inline f32 f_sqrt(f32 a) {
    return (a * inv_sqrt(a));
}

// @Speed: On Sandy Bridge, FSIN takes 47-100 cycles. FSINCOS takes 43-123.
// http://mooooo.ooo/chebyshev-sine-approximation/
static inline f32 f_sin(f32 a) {
    if(a < -PI) {
        a += PI * 2.0f;
    } else if(a > PI) {
        a -= PI * 2.0f;
    }
    ASSERT((a >= -PI) && (a <= PI));
    f32 asq = a * a;
    f32 p11 = 0.00000000013291342f;
    f32 p9  = p11 * asq - 0.000000023317787f;
    f32 p7  =  p9 * asq + 0.0000025222919f;
    f32 p5  =  p7 * asq - 0.00017350505f;
    f32 p3  =  p5 * asq + 0.0066208798f;
    f32 p1  =  p3 * asq - 0.10132118f;
    return ((a - 3.1415927f + 0.00000008742278f) *
            (a + 3.1415927f - 0.00000008742278f) * p1 * a);
}
static inline f32 f_cos(f32 a) {
    f32 result = f_sin(a + PI * 0.5f);
    return(result);
}

static inline f32 f_sin_n(f32 a) {
    return f_sin(f_mod(a, 2.0f * PI));
}

// Result is between -1 and 3, growing as a real angle would.
static inline f32 pseudoangle(v2 dp) {
    f32 absdx = f_abs(dp.x);
    f32 absdy = f_abs(dp.y);
    f32 result = dp.y / (absdx + absdy);
    if(dp.x < 0.0f) {
        result = -result + 2.0f;
    }
    return result;
}

static inline f32 f_in_range(f32 value, f32 lower_bound, f32 upper_bound) {
    bool result = (value >= lower_bound) && (value <= upper_bound);
    return(result);
}

static inline f32 f_clamp(f32 to_clamp, f32 low_bound, f32 high_bound) {
    ASSERT(low_bound < high_bound);
    f32 result = f_min(high_bound, f_max(low_bound, to_clamp));
    return(result);
}
static inline f32 f_symmetrical_clamp(const f32 to_clamp, const f32 abs_bound) {
    u32 sign_bit = (*((u32 *)(&to_clamp))) & (1 << 31);
    u32 _abs_v =   (*((u32 *)(&to_clamp))) & (~(1 << 31));
    f32 abs_v = *(f32 *)&_abs_v;
    
    if(abs_v > abs_bound) {
        abs_v = abs_bound;
    }
    u32 _v = (*((u32 *)(&abs_v))) | sign_bit;
    return *((f32 *)(&_v));
}
static inline f32 f_clamp01(f32 to_clamp) {
    return(f_clamp(to_clamp, 0.0f, 1.0f));
}

static inline s32 f_round_to_s(f32 f) {
    s32 result = (f >= 0.0f) ? (s32)(f + 0.5f) : (s32)(f - 0.5f);
    return(result);
}

static inline f32 f_round(f32 f) {
    f32 result = (f32)f_round_to_s(f);
    return result;
}

static inline f32 f_round_to_f(f32 f, f32 round) {
    f32 result = f + round * 0.5f - f_mod(f, round);
    return result;
}

// Bezier/spline explanation: https://www.youtube.com/watch?v=S2fz4BS2J3Y
// a  b c
//  \/\/   Recursive interpolation of these three points, with
//   \/    "\" being 1-t and "/" being t.
// result
static inline v2 quadratic_bezier(v2 a, v2 b, v2 c, f32 t) {
    f32 one_minus_t = 1.0f - t;
    v2 result = a * one_minus_t * one_minus_t + b * 2.0f * t * one_minus_t + c * t * t;
    return result;
}

static inline v2 v2_inverse_rotation(v2 complex) {
    v2 result;
    result.x = complex.x;
    result.y = -complex.y;
    return result;
}

static inline v2 v2_move_toward(v2 from, v2 to, v2 amount) {
    return(V2(f_move_toward(from.x, to.x, amount.x),
              f_move_toward(from.y, to.y, amount.y)));
}

static inline v2 V2(v2s16 int_v) {
    v2 result;
    result.x = (f32)int_v.x;
    result.y = (f32)int_v.y;
    return(result);
}

static inline bool isnull(v2 a) {
    
    return((a.x == 0) && (a.y == 0));
}

static inline bool v2_equal(v2 a, v2 b) {
    bool result = (a.x == b.x) && (a.y == b.y);
    return(result);
}

static inline v2 v2_negate(v2 v) {
    v2 result = V2(-v.x, -v.y);
    return(result);
}

static inline v2 v2_inverse(v2 v) {
    ASSERT(v.x);
    ASSERT(v.y);
    v2 result = V2(1.0f / v.x, 1.0f / v.y);
    return result;
}

static inline v2 v2_element_wise_div(v2 a, v2 b) {
    ASSERT(b.x);
    ASSERT(b.y);
    v2 result = V2(a.x / b.x, a.y / b.y);
    return result;
}

static inline v2 v2_perp(v2 a) {
    v2 result;
    
    result.x = -a.y;
    result.y = a.x;
    
    return(result);
}

static inline v2 v2_sum_f(v2 a, f32 b){
    v2 result;
    
    result.x = a.x + b;
    result.y = a.y + b;
    
    return(result);
}

static inline void v2_hadamard(v2 *a, v2 b){
    a->x *= b.x;
    a->y *= b.y;
}

static inline v2 v2_hadamard_prod(v2 a, v2 b){
    v2 result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    return(result);
}

static inline f32 v2_cross_prod(v2 a, v2 b) {
    f32 result = a.x * b.y - a.y * b.x;
    return result;
}

static inline f32 v2_length_sq(v2 a){
    f32 result = a.x*a.x + a.y*a.y;
    return(result);
}

static inline f32 v2_length(v2 a){
    f32 result = f_sqrt(a.x*a.x + a.y*a.y);
    return(result);
}

static inline bool v2_longer_than(v2 v, f32 length) {
    return v2_length_sq(v) > (length * length);
}

static inline v2 v2_clamp(v2 a, f32 low, f32 high) {
    v2 result;
    
    result.x = f_clamp(a.x, low, high);
    result.y = f_clamp(a.y, low, high);
    
    return result;
}

static inline v2 v2_lerp(v2 from, v2 to, f32 at){
    v2 result;
    result.x = ((1.0f - at) * from.x) + (at * to.x);
    result.y = ((1.0f - at) * from.y) + (at * to.y);
    
    return(result);
}

static inline v2 v2_reverse_lerp(v2 from, v2 to, v2 at) {
    v2 result;
    result.x = (at.x - from.x) / (to.x - from.x);
    result.y = (at.y - from.y) / (to.y - from.y);
    
    return result;
}

static inline bool v2_in_circle(v2 v, v2 circle_p, f32 radius, f32 enlarge = 0.0f){
    bool result = (v2_length(v - circle_p) < (radius + enlarge));
    
    return(result);
}

static inline v2 v2_normalize(v2 a){
    
    v2 result = {};
    f32 alen = v2_length(a);
    if(alen) {
        
        f32 normalize_factor = 1.0f/alen;
        
        result.x = a.x * normalize_factor;
        result.y = a.y * normalize_factor;
    }
    return(result);
}

static inline v2 v2_angle_to_complex(f32 rads) {
    v2 result;
    result.x = f_cos(rads);
    result.y = f_sin(rads);
    return(result);
}

static inline f32 v2_inner(v2 a, v2 b){
    f32 result = a.x*b.x + a.y*b.y;
    
    return(result);
}

static inline v2 v2_complex_prod(v2 a, v2 b) {
    v2 result;
    result.x = a.x*b.x - a.y*b.y;
    result.y = a.y*b.x + a.x*b.y;
    return(result);
}

static inline v2 v2_complex_prod_n(v2 a, v2 b) {
    return v2_normalize(v2_complex_prod(a, b));
}

static inline s32 v2_rect2_first_hit(v2 p, rect2 *rects, ssize rect_count) {
    FORI_TYPED(s32, 0, rect_count) {
        rect2 rect = rects[i];
        if((p.x > rect.left && p.x < rect.right) &&
           (p.y > rect.bottom && p.y < rect.top)) {
            return i;
        }
    }
    return -1;
}

static inline bool point_in_rect2(v2 p, rect2 rect) {
    bool result = (p.x > rect.left && p.x < rect.right) &&
        (p.y > rect.bottom && p.y < rect.top);
    return(result);
}

static inline void line_line_intersection(const v2 a0, const v2 b0, const v2 a1, const v2 b1, f32 *out_t, f32 *out_s) {
    const v2 d0 = b0 - a0;
    const v2 d1 = b1 - a1;
    const v2 da = a0 - a1;
    const f32 inv_cross = 1.0f / v2_cross_prod(d0, d1);
    const f32 s = v2_cross_prod(d0, da) * inv_cross;
    const f32 t = v2_cross_prod(d1, da) * inv_cross;
    *out_s = s;
    *out_t = t;
}

static inline bool line_line_overlap(v2 a0, v2 b0, v2 a1, v2 b1) {
    f32 t;
    f32 s;
    
    line_line_intersection(a0, b0, a1, b1, &t, &s);
    
    return (t >= 0.0f) && (t <= 1.0f) && (s >= 0.0f) && (s <= 1.0f);
}

static inline rect2 rect2_4f(f32 left, f32 bottom, f32 right, f32 top){
    rect2 result = {};
    
    result.left = left;
    result.bottom = bottom;
    result.right = right;
    result.top = top;
    
    return(result);
}

static inline rect2 rect2_minmax(v2 min, v2 max) {
    
    return(rect2_4f(min.x, min.y, max.x, max.y));
}

static inline v2 rect2_center(rect2 a) {
    v2 result;
    
    result.x = (a.left + a.right) * 0.5f;
    result.y = (a.bottom + a.top) * 0.5f;
    
    return result;
}

static inline v2 rect2_dim(rect2 a) {
    v2 result;
    
    result.x = a.right - a.left;
    result.y = a.top - a.bottom;
    
    return result;
}

static inline rect2 rect2_pdim(v2 p, v2 dim){
    rect2 result = {};
    
    result.left   = p.x - dim.x*0.5f;
    result.right  = p.x + dim.x*0.5f;
    result.top    = p.y + dim.y*0.5f;
    result.bottom = p.y - dim.y*0.5f;
    
    return(result);
}

static inline rect2 rect2_phalfdim(v2 p, v2 halfdim){
    rect2 result;
    
    result.left   = p.x - halfdim.x;
    result.right  = p.x + halfdim.x;
    result.top    = p.y + halfdim.y;
    result.bottom = p.y - halfdim.y;
    
    return(result);
}

static inline rect2 rect2_grow_dim(rect2 a, v2 dim){
    rect2 result = a;
    
    f32 halfw = dim.x/2;
    f32 halfh = dim.y/2;
    
    result.left -= halfw;
    result.right += halfw;
    result.top += halfh;
    result.bottom -= halfh;
    
    return(result);
}

static inline rect2 rect2_clip(rect2 a, rect2 b) {
    rect2 result = rect2_4f(f_max(a.left, b.left),
                            f_max(a.bottom, b.bottom),
                            f_min(a.right, b.right),
                            f_min(a.top, b.top));
    return result;
}

static inline v2 v2_fit_to_h(v2 v, f32 h) {
    
    ASSERT(v.y);
    f32 resize_factor = h / v.y;
    v2 result;
    result.x = v.x * resize_factor;
    result.y = h;
    
    return(result);
}

static inline v3 v3_lerp(v3 from, v3 to, f32 at) {
    v3 result;
    
    result.x = (1.0f - at) * from.x + at * to.x;
    result.y = (1.0f - at) * from.y + at * to.y;
    result.z = (1.0f - at) * from.z + at * to.z;
    
    return result;
}

static inline v3 v3_hadamard_prod(v3 a, v3 b) {
    v3 result;
    
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    
    return result;
}

static inline v4 v4_lerp(v4 from, v4 to, f32 at) {
    v4 result;
    
    result.x = (1.0f - at) * from.x + at * to.x;
    result.y = (1.0f - at) * from.y + at * to.y;
    result.z = (1.0f - at) * from.z + at * to.z;
    result.w = (1.0f - at) * from.w + at * to.w;
    
    return result;
}

static inline v4 v4_hadamard_prod(v4 a, v4 b) {
    v4 result;
    
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    result.w = a.w * b.w;
    
    return result;
}

static inline bool rects_differ(rect2s a, rect2s b) {
    return ((a.left != b.left) || (a.bottom != b.bottom) ||
            (a.right != b.right) || (a.top != b.top));
}
static inline bool xy_in_rect(s32 x, s32 y, rect2s rect) {
    return ((y >= rect.bottom) && (y <= rect.top) &&
            (x >= rect.left) && (x <= rect.right));
}
static rect2s rect2s_fit(s32 to_fit_w, s32 to_fit_h, s32 bounds_w, s32 bounds_h) {
    rect2s result;
    
    ASSERT((to_fit_w > 0) && (to_fit_h > 0) &&
           (bounds_w > 0) && (bounds_h > 0));
    
    f32 w_over_h = (f32)to_fit_w / (f32)to_fit_h;
    f32 h_over_w = 1.0f / w_over_h;
    
    f32 fit_w = bounds_h * w_over_h;
    f32 fit_h = bounds_w * h_over_w;
    
    if(fit_w > (f32)bounds_w) {
        // We are width-bound.
        
        s32 one_black_bar_height = f_round_to_s(((f32)bounds_h - fit_h) * 0.5f);
        
        result.left = 0;
        result.right = bounds_w;
        result.bottom = one_black_bar_height;
        result.top = bounds_h - one_black_bar_height;
    } else {
        // We are height-bound.
        
        s32 one_black_bar_width = f_round_to_s(((f32)bounds_w - fit_w) * 0.5f);
        
        result.left = one_black_bar_width;
        result.right = bounds_w - one_black_bar_width;
        result.bottom = 0;
        result.top = bounds_h;
    }
    
    return(result);
}

static inline bool bitscan_reverse(u64 value, s32 *index) {
    if(value) {
        s32 result = 63;
        usize bit = ((u64)1 << result);
        
        while(!(bit & value) && result) {
            bit >>= 1;
            --result;
        }
        
        *index = result;
        return true;
    }
    
    return false;
}

static inline bool bitscan_forward(usize value, s32 *index) {
    if(value) {
        s32 result = 0;
        usize bit = 1;
        
        while(!(bit & value)) {
            bit <<= 1;
            ++result;
        }
        
        *index = result;
        return true;
    }
    
    return false;
}

// @Speed!
static inline u64 bit_reverse(u64 a, ssize bits) {
    u64 result = 0;
    FORI(0, bits) {
        result |= ((a >> i) & 1) << (bits - 1 - i);
    }
    return result;
}

static inline void unpack_rgba8(u32 pixel, s32 *r, s32 *g, s32 *b, s32 *a) {
    *r = (pixel)       & 0xFF;
    *g = (pixel >>  8) & 0xFF;
    *b = (pixel >> 16) & 0xFF;
    *a = (pixel >> 24) & 0xFF;
}