
#if OS_WINDOWS
#include <intrin.h>
#elif OS_LINUX
#include <x86intrin.h>
#else
#error Unrecognised OS.
#endif

#if OS_WINDOWS
#define WRITE_FENCE _WriteBarrier(); _mm_sfence()
#define ATOMIC_INC32(value) _InterlockedIncrement((value))
#define ATOMIC_INC64(value) _InterlockedIncrement64((value))
#endif

union simd128 {
    __m128 as_m128;
    __m128i as_m128i;
    __m128d as_m128d;
    f32 as_f32[4];
    f64 as_f64[2];
    s32 as_s32[4];
    s64 as_s64[2];
};

inline void xorshift_x4(__m128i *state) {
    __m128i temp0 = _mm_slli_epi32(*state, 13);
    __m128i temp1 = _mm_srli_epi32(temp0, 17);
    __m128i temp2 = _mm_xor_si128(temp0, temp1);
    __m128i temp3 = _mm_slli_epi32(temp2, 5);
    __m128i out = _mm_xor_si128(temp2, temp3);
    *state = out;
}

inline __m128 xorshift_tpdf_x4(__m128i *state0, __m128i *state1, const __m128 inv_range, const __m128 one) {
    xorshift_x4(state0);
    xorshift_x4(state1);
    __m128 f0 = _mm_cvtepi32_ps(*state0);
    __m128 f1 = _mm_cvtepi32_ps(*state1);
    // f0 and f1 are in range s32_min->s32_max
    // f0 + f1 is in range s32_min * 2->s32_max * 2
    // We make the noise uniform by adding 1, since |s32_min| = |s32_max| + 1
    // The range then becomes -|2 * s32_max + 1| -> |2 * s32_max + 1|
    __m128 sum = _mm_add_ps(_mm_add_ps(f0, f1), one);
    // We then divide by the range to get -1->1.
    // If we truncate instead of rounding, we should generate -0.5->1.5 noise instead.
    __m128 tpdf = _mm_mul_ps(sum, inv_range);
    return tpdf;
}

static inline __m128i f_round_s_x4(__m128 f) {
    __m128i result = _mm_cvtps_epi32(f);
    return result;
}
static inline __m128 f_ceil_f_x4(__m128 a) {
    __m128 result = _mm_ceil_ps(a);
    return result;
}
static inline __m128 f_floor_f_x4(__m128 a) {
    __m128 result = _mm_floor_ps(a);
    return result;
}
static inline __m128 f_round_f_x4(__m128 a) {
    __m128 result = _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    return result;
}
static inline __m128i f64_round_s_x2(__m128d f) {
    __m128i result = _mm_cvtpd_epi32(f);
    return result;
}
static inline __m128d f64_round_f64_x2(__m128d f) {
    __m128d result = _mm_round_pd(f, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    return result;
}
static inline __m128d f64_floor_f64_x2(__m128d f) {
    __m128d result = _mm_floor_pd(f);
    return result;
}
static inline __m128d f64_ceil_f64_x2(__m128d f) {
    __m128d result = _mm_ceil_pd(f);
    return result;
}

static inline f32 f_round_f(f32 a) {
    simd128 result;
    
    result.as_m128 = f_round_f_x4(_mm_load_ss(&a));
    
    return result.as_f32[0];
}
static inline f32 f_ceil_f(f32 a) {
    simd128 result;
    
    result.as_m128 = f_ceil_f_x4(_mm_load_ss(&a));
    
    return result.as_f32[0];
}
static inline f32 f_floor_f(f32 a) {
    simd128 result;
    
    result.as_m128 = f_floor_f_x4(_mm_load_ss(&a));
    
    return result.as_f32[0];
}
static inline s32 f_round_s(f32 a) {
    simd128 result;
    
    result.as_m128i = f_round_s_x4(_mm_load_ss(&a));
    
    return result.as_s32[0];
}
static inline s32 f64_round_s(f64 a) {
    simd128 result;
    
    result.as_m128i = f64_round_s_x2(_mm_load_sd(&a));
    
    return result.as_s32[0];
}
static inline f64 f64_round_f64(f64 a) {
    simd128 result;
    
    result.as_m128d = f64_round_f64_x2(_mm_load_sd(&a));
    
    return result.as_f64[0];
}
static inline f64 f64_floor_f64(f64 a) {
    simd128 result;
    
    result.as_m128d = f64_floor_f64_x2(_mm_load_sd(&a));
    
    return result.as_f64[0];
}
static inline f64 f64_ceil_f64(f64 a) {
    simd128 result;
    
    result.as_m128d = f64_ceil_f64_x2(_mm_load_sd(&a));
    
    return result.as_f64[0];
}