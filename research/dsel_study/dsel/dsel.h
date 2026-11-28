// Digit selection kernels for Tdoku's ChooseBandAndValueToBranch (the part after band selection).
//
// Input: a band's configuration vector x: 16-bit lanes 0..5 hold the 9-bit digit sets of the six
// configurations; lanes 6, 7 and bits 9..15 are zero. The caller guarantees that some digit has
// two or more configurations (the band is not fixed).
// Output: a value mask with the chosen digit's bit set in (at least) lanes 0..5, exactly what
// BranchOnBandAndValue expects.
//
// Policies:
//   B (Tdoku since 43b3782): lowest digit with exactly 2, else exactly 3, else lowest digit with >= 4.
//   exact MRV (A, C, D*, E*, F): lowest digit among those with the minimum count >= 2.
#pragma once
#include <immintrin.h>
#include <cstdint>
#include <initializer_list>

namespace dsel {

#define DSEL_INLINE static inline __attribute__((always_inline))

alignas(16) static const uint16_t kValueMask[9][8] = {
        {1, 1, 1, 1, 1, 1, 1, 1}, {2, 2, 2, 2, 2, 2, 2, 2}, {4, 4, 4, 4, 4, 4, 4, 4},
        {8, 8, 8, 8, 8, 8, 8, 8}, {16, 16, 16, 16, 16, 16, 16, 16}, {32, 32, 32, 32, 32, 32, 32, 32},
        {64, 64, 64, 64, 64, 64, 64, 64}, {128, 128, 128, 128, 128, 128, 128, 128},
        {256, 256, 256, 256, 256, 256, 256, 256}};

DSEL_INLINE __m128i ValueMask(int d) { return _mm_load_si128((const __m128i *) kValueMask[d]); }

// lane i <- lane (i + k) mod 6 for lanes 0..5; lanes 6, 7 <- 0
DSEL_INLINE __m128i Rot1() {
    return _mm_setr_epi8(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, -128, -128, -128, -128);
}
DSEL_INLINE __m128i Rot3() {
    return _mm_setr_epi8(6, 7, 8, 9, 10, 11, 0, 1, 2, 3, 4, 5, -128, -128, -128, -128);
}
DSEL_INLINE __m128i LowBit(__m128i v) { return _mm_and_si128(v, _mm_sign_epi16(v, _mm_set1_epi16(-1))); }
DSEL_INLINE bool Zero(__m128i v) { return _mm_testz_si128(v, v); }
#define AND _mm_and_si128
#define OR _mm_or_si128
#define XOR _mm_xor_si128
#define ANDN _mm_andnot_si128  // ANDN(a, b) = ~a & b

// ---------------------------------------------------------------------------------------------
// A: original exact MRV (per-digit popcount + minpos), as before commit 43b3782.
static inline int PopcountDigit(__m128i x, int d) {
    __m128i m = AND(x, ValueMask(d));
    return __builtin_popcountll((uint64_t) _mm_cvtsi128_si64(m)) +
           __builtin_popcountll((uint64_t) _mm_cvtsi128_si64(_mm_unpackhi_epi64(m, m)));
}
DSEL_INLINE __m128i SelA(__m128i x) {
    __m128i counts = _mm_setr_epi16(
            (short) PopcountDigit(x, 0), (short) PopcountDigit(x, 1), (short) PopcountDigit(x, 2),
            (short) PopcountDigit(x, 3), (short) PopcountDigit(x, 4), (short) PopcountDigit(x, 5),
            (short) PopcountDigit(x, 6), (short) PopcountDigit(x, 7));
    uint32_t value_minpos = (uint32_t) _mm_cvtsi128_si32(_mm_minpos_epu16(_mm_sub_epi16(counts, _mm_set1_epi16(2))));
    int best_value = (int) (value_minpos >> 16u);
    uint32_t excess_over_two = value_minpos & 0xffffu;
    if (excess_over_two) {
        uint32_t last_count = (uint32_t) PopcountDigit(x, 8);
        if (last_count > 1 && last_count < excess_over_two + 2) best_value = 8;
    }
    return ValueMask(best_value);
}
__attribute__((noinline, cold)) static __m128i SelAColdFallback(__m128i x) { return SelA(x); }

// ---------------------------------------------------------------------------------------------
// B: Tdoku's current approximate unary threshold counting (verbatim structure).
DSEL_INLINE __m128i SelB(__m128i x) {
    const __m128i rot = Rot1();
    __m128i one = x;
    __m128i rotated = _mm_shuffle_epi8(one, rot);  // 1
    __m128i two = AND(one, rotated);
    one = OR(one, rotated);
    rotated = _mm_shuffle_epi8(rotated, rot);  // 2
    __m128i three = AND(two, rotated);
    two = OR(two, AND(one, rotated));
    one = OR(one, rotated);
    rotated = _mm_shuffle_epi8(rotated, rot);  // 3
    __m128i four = AND(three, rotated);
    three = OR(three, AND(two, rotated));
    two = OR(two, AND(one, rotated));
    one = OR(one, rotated);
    rotated = _mm_shuffle_epi8(rotated, rot);  // 4
    four = OR(four, AND(three, rotated));
    three = OR(three, AND(two, rotated));
    two = OR(two, AND(one, rotated));
    one = OR(one, rotated);
    rotated = _mm_shuffle_epi8(rotated, rot);  // 5
    four = OR(four, AND(three, rotated));
    three = OR(three, AND(two, rotated));
    two = OR(two, AND(one, rotated));
    __m128i only_two = ANDN(three, two);
    if (__builtin_expect(!Zero(only_two), 1)) return LowBit(only_two);
    __m128i only_three = ANDN(four, three);
    if (__builtin_expect(!Zero(only_three), 1)) return LowBit(only_three);
    return LowBit(four);
}

// ---------------------------------------------------------------------------------------------
// C: exact unary threshold counting up to six (dead operations removed).
DSEL_INLINE __m128i SelC(__m128i x) {
    const __m128i rot = Rot1();
    __m128i one = x;
    __m128i r = _mm_shuffle_epi8(one, rot);  // 1
    __m128i two = AND(one, r);
    one = OR(one, r);
    r = _mm_shuffle_epi8(r, rot);  // 2
    __m128i three = AND(two, r);
    two = OR(two, AND(one, r));
    one = OR(one, r);
    r = _mm_shuffle_epi8(r, rot);  // 3
    __m128i four = AND(three, r);
    three = OR(three, AND(two, r));
    two = OR(two, AND(one, r));
    one = OR(one, r);
    r = _mm_shuffle_epi8(r, rot);  // 4
    __m128i five = AND(four, r);
    four = OR(four, AND(three, r));
    three = OR(three, AND(two, r));
    two = OR(two, AND(one, r));
    one = OR(one, r);
    r = _mm_shuffle_epi8(r, rot);  // 5
    __m128i six = AND(five, r);
    five = OR(five, AND(four, r));
    four = OR(four, AND(three, r));
    three = OR(three, AND(two, r));
    two = OR(two, AND(one, r));
    __m128i m = ANDN(three, two);
    if (__builtin_expect(!Zero(m), 1)) return LowBit(m);
    m = ANDN(four, three);
    if (!Zero(m)) return LowBit(m);
    m = ANDN(five, four);
    if (!Zero(m)) return LowBit(m);
    m = ANDN(six, five);
    if (!Zero(m)) return LowBit(m);
    return LowBit(six);
}

// ---------------------------------------------------------------------------------------------
// Bit-sliced binary counting (carry-save adders).
DSEL_INLINE void FA(__m128i a, __m128i b, __m128i c, __m128i &sum, __m128i &carry) {
#if defined(__AVX512VL__)
    sum = _mm_ternarylogic_epi32(a, b, c, 0x96);
    carry = _mm_ternarylogic_epi32(a, b, c, 0xe8);
#else
    __m128i t = XOR(a, b);
    sum = XOR(t, c);
    carry = OR(AND(a, b), AND(t, c));
#endif
}

// exact-MRV decode from bitplanes b0, b1, b2 (count = 4*b2 + 2*b1 + b0, count <= 6)
DSEL_INLINE __m128i DecodeExact(__m128i b0, __m128i b1, __m128i b2) {
    __m128i m = ANDN(OR(b0, b2), b1);  // 2
    if (__builtin_expect(!Zero(m), 1)) return LowBit(m);
    m = ANDN(b2, AND(b1, b0));  // 3
    if (!Zero(m)) return LowBit(m);
    m = ANDN(OR(b1, b0), b2);  // 4
    if (!Zero(m)) return LowBit(m);
    m = ANDN(b1, AND(b2, b0));  // 5
    if (!Zero(m)) return LowBit(m);
    return LowBit(AND(b2, b1));  // 6
}

// D1: CSA on lane rotations. FA over lanes {i, i+1, i+2}; rotating its outputs by 3 gives the FA
// over lanes {i+3, i+4, i+5}, so 4 shuffles suffice and every lane holds the full count.
DSEL_INLINE void CountD1(__m128i x, __m128i &b0, __m128i &b1, __m128i &b2) {
    const __m128i rot1 = Rot1(), rot3 = Rot3();
    __m128i r1 = _mm_shuffle_epi8(x, rot1);
    __m128i r2 = _mm_shuffle_epi8(r1, rot1);
    __m128i s, c;
    FA(x, r1, r2, s, c);
    __m128i s3 = _mm_shuffle_epi8(s, rot3);
    __m128i c3 = _mm_shuffle_epi8(c, rot3);
    b0 = XOR(s, s3);
    __m128i cc = AND(s, s3);
    FA(c, c3, cc, b1, b2);
}
DSEL_INLINE __m128i SelD1(__m128i x) {
    __m128i b0, b1, b2;
    CountD1(x, b0, b1, b2);
    return DecodeExact(b0, b1, b2);
}

// D2: CSA with in-qword shifts (shift ports instead of the shuffle port); valid in lane 0 only,
// decoded in scalar registers. Lanes 0..3 sit in qword 0, lanes 4..5 in qword 1.
DSEL_INLINE __m128i SelD2(__m128i x) {
    __m128i z = _mm_unpackhi_epi64(x, x);  // lanes 0, 1 <- configs 4, 5
    __m128i s1, c1, s2, c2;
    FA(x, _mm_srli_epi64(x, 16), _mm_srli_epi64(x, 32), s1, c1);   // configs 0, 1, 2
    FA(_mm_srli_epi64(x, 48), z, _mm_srli_epi64(z, 16), s2, c2);   // configs 3, 4, 5
    __m128i b0 = XOR(s1, s2);
    __m128i cc = AND(s1, s2);
    __m128i b1, b2;
    FA(c1, c2, cc, b1, b2);
    // pack the three planes' lane 0 into one GPR: b0 | b1 << 16 | b2 << 32
    __m128i p = _mm_unpacklo_epi16(b0, b1);           // lane0 = b0, lane1 = b1
    p = _mm_unpacklo_epi32(p, b2);                    // lane2 = b2 (lane 0 of b2)
    uint64_t planes = (uint64_t) _mm_cvtsi128_si64(p);
    uint32_t B0 = (uint32_t) planes & 0x1ffu, B1 = (uint32_t) (planes >> 16) & 0x1ffu,
             B2 = (uint32_t) (planes >> 32) & 0x1ffu;
    uint32_t m = B1 & ~(B0 | B2);
    if (__builtin_expect(m == 0, 0)) {
        m = B1 & B0 & ~B2;
        if (!m) m = B2 & ~(B1 | B0);
        if (!m) m = B2 & B0 & ~B1;
        if (!m) m = B2 & B1;
    }
    return ValueMask(__builtin_ctz(m));
}

// ---------------------------------------------------------------------------------------------
// E1: exact-two fast path with unary counting up to three; cold exact fallback (A).
DSEL_INLINE __m128i SelE1(__m128i x) {
    const __m128i rot = Rot1();
    __m128i one = x;
    __m128i r = _mm_shuffle_epi8(one, rot);  // 1
    __m128i two = AND(one, r);
    one = OR(one, r);
    r = _mm_shuffle_epi8(r, rot);  // 2
    __m128i three = AND(two, r);
    two = OR(two, AND(one, r));
    one = OR(one, r);
    r = _mm_shuffle_epi8(r, rot);  // 3
    three = OR(three, AND(two, r));
    two = OR(two, AND(one, r));
    one = OR(one, r);
    r = _mm_shuffle_epi8(r, rot);  // 4
    three = OR(three, AND(two, r));
    two = OR(two, AND(one, r));
    one = OR(one, r);
    r = _mm_shuffle_epi8(r, rot);  // 5
    three = OR(three, AND(two, r));
    two = OR(two, AND(one, r));
    __m128i m = ANDN(three, two);
    if (__builtin_expect(!Zero(m), 1)) return LowBit(m);
    return SelAColdFallback(x);
}

// E2: exact-two detection from two 3-lane group counts (s, c) and (s3, c3):
// total == 2 iff s == s3 and (s ? c == c3 == 0 : c != c3).
DSEL_INLINE __m128i SelE2(__m128i x) {
    const __m128i rot1 = Rot1(), rot3 = Rot3();
    __m128i r1 = _mm_shuffle_epi8(x, rot1);
    __m128i r2 = _mm_shuffle_epi8(r1, rot1);
    __m128i s, c;
    FA(x, r1, r2, s, c);
    __m128i s3 = _mm_shuffle_epi8(s, rot3);
    __m128i c3 = _mm_shuffle_epi8(c, rot3);
    __m128i e = XOR(c, c3);
    __m128i t = OR(ANDN(OR(c, c3), s), ANDN(s, e));  // s ? ~(c|c3) : (c^c3)
    __m128i m = ANDN(XOR(s, s3), t);
    if (__builtin_expect(!Zero(m), 1)) return LowBit(m);
    return SelAColdFallback(x);
}

// ---------------------------------------------------------------------------------------------
// F: GFNI transpose to one byte per digit, nibble-LUT popcount, exact argmin via minpos.
#ifdef __GFNI__
DSEL_INLINE __m128i SelF(__m128i x) {
    // element e's low byte -> byte 7 - e, high byte -> byte 15 - e (elements 0..5)
    const __m128i ctrl = _mm_setr_epi8(-128, -128, 10, 8, 6, 4, 2, 0, -128, -128, 11, 9, 7, 5, 3, 1);
    __m128i t = _mm_gf2p8affine_epi64_epi8(_mm_set1_epi64x((long long) 0x8040201008040201ull),
                                           _mm_shuffle_epi8(x, ctrl), 0);
    // byte d (0..7) = configs of digit d; byte 8 = configs of digit 8; bytes 9..15 = 0
    const __m128i lut = _mm_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4);
    const __m128i lo4 = _mm_set1_epi8(0x0f);
    __m128i n = _mm_add_epi8(_mm_shuffle_epi8(lut, AND(t, lo4)),
                             _mm_shuffle_epi8(lut, AND(_mm_srli_epi16(t, 4), lo4)));
    // key = n - 2 (counts 0, 1 and empty bytes wrap to >= 0xfe); 16-bit keys for digits 0..7
    __m128i key = _mm_sub_epi8(n, _mm_set1_epi8(2));
    __m128i k16 = _mm_cvtepu8_epi16(key);
    uint32_t mp = (uint32_t) _mm_cvtsi128_si32(_mm_minpos_epu16(k16));
    uint32_t best = mp >> 16, best_key = mp & 0xffffu;
    uint32_t key8 = (uint32_t) _mm_extract_epi8(key, 8);
    if (key8 < best_key) best = 8;
    return ValueMask((int) best);
}
#endif

// scalar reference counts and policies (for tests)
static inline int RefCount(const uint16_t lanes[8], int d) {
    int n = 0;
    for (int i = 0; i < 6; i++) n += (lanes[i] >> d) & 1;
    return n;
}
static inline int RefExact(const uint16_t lanes[8]) {
    int best = -1, bn = 99;
    for (int d = 0; d < 9; d++) {
        int n = RefCount(lanes, d);
        if (n >= 2 && n < bn) { bn = n; best = d; }
    }
    return best;
}
static inline int RefB(const uint16_t lanes[8]) {
    for (int target : {2, 3})
        for (int d = 0; d < 9; d++) if (RefCount(lanes, d) == target) return d;
    for (int d = 0; d < 9; d++) if (RefCount(lanes, d) >= 4) return d;
    return -1;
}

#undef AND
#undef OR
#undef XOR
#undef ANDN
}  // namespace dsel

namespace dsel {
// DB: carry-save counting with Tdoku's current policy B (decision-identical to SelB).
DSEL_INLINE __m128i SelDB(__m128i x) {
    __m128i b0, b1, b2;
    CountD1(x, b0, b1, b2);
    __m128i m = _mm_andnot_si128(_mm_or_si128(b0, b2), b1);  // exactly 2
    if (__builtin_expect(!Zero(m), 1)) return LowBit(m);
    m = _mm_andnot_si128(b2, _mm_and_si128(b1, b0));        // exactly 3
    if (__builtin_expect(!Zero(m), 1)) return LowBit(m);
    return LowBit(b2);                                       // lowest digit with >= 4
}
}  // namespace dsel
