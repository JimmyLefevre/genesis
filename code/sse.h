
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
