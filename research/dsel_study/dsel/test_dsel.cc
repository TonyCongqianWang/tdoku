// Correctness tests for the digit-selection kernels in dsel.h.
#include "dsel.h"
#include <cstdio>
#include <cstring>
#include <random>

using namespace dsel;

static int MaskDigit(__m128i m) {
    // the returned mask must have exactly one digit bit, replicated in lanes 0..5
    alignas(16) uint16_t l[8];
    _mm_store_si128((__m128i *) l, m);
    for (int i = 1; i < 6; i++) if (l[i] != l[0]) return -2;
    if (l[0] == 0 || (l[0] & (l[0] - 1)) || l[0] > 0x1ff) return -3;
    return __builtin_ctz(l[0]);
}

static __m128i Load(const uint16_t lanes[8]) { return _mm_loadu_si128((const __m128i *) lanes); }

struct Variant { const char *name; __m128i (*fn)(__m128i); bool exact; };
static __m128i fA(__m128i x) { return SelA(x); }
static __m128i fB(__m128i x) { return SelB(x); }
static __m128i fC(__m128i x) { return SelC(x); }
static __m128i fD1(__m128i x) { return SelD1(x); }
static __m128i fD2(__m128i x) { return SelD2(x); }
static __m128i fE1(__m128i x) { return SelE1(x); }
static __m128i fE2(__m128i x) { return SelE2(x); }
#ifdef __GFNI__
static __m128i fF(__m128i x) { return SelF(x); }
#endif
static const Variant kVariants[] = {
        {"A", fA, true}, {"B", fB, false}, {"C", fC, true}, {"D1", fD1, true}, {"D2", fD2, true},
        {"E1", fE1, true}, {"E2", fE2, true},
#ifdef __GFNI__
        {"F", fF, true},
#endif
};

int main() {
    long failures = 0, checks = 0;
    auto check = [&](const uint16_t lanes[8], const char *what) {
        int exact = RefExact(lanes), b = RefB(lanes);
        if (exact < 0) return;  // fixed band: not a valid input
        for (const auto &v : kVariants) {
            int got = MaskDigit(v.fn(Load(lanes)));
            int want = v.exact ? exact : b;
            checks++;
            if (got != want) {
                if (failures < 20)
                    printf("FAIL %s %s: got %d want %d lanes %03x %03x %03x %03x %03x %03x\n", v.name, what,
                           got, want, lanes[0], lanes[1], lanes[2], lanes[3], lanes[4], lanes[5]);
                failures++;
            }
        }
    };

    // 1. bit-sliced counts: all 64 six-bit patterns for every digit, checked directly
    long count_checks = 0, count_fail = 0;
    for (int d = 0; d < 9; d++)
        for (int p = 0; p < 64; p++) {
            uint16_t lanes[8] = {0};
            for (int i = 0; i < 6; i++) if (p >> i & 1) lanes[i] |= (uint16_t) (1u << d);
            __m128i b0, b1, b2;
            CountD1(Load(lanes), b0, b1, b2);
            alignas(16) uint16_t l0[8], l1[8], l2[8];
            _mm_store_si128((__m128i *) l0, b0);
            _mm_store_si128((__m128i *) l1, b1);
            _mm_store_si128((__m128i *) l2, b2);
            for (int i = 0; i < 6; i++) {
                int n = (l0[i] >> d & 1) + 2 * (l1[i] >> d & 1) + 4 * (l2[i] >> d & 1);
                count_checks++;
                if (n != __builtin_popcount(p) || ((l0[i] | l1[i] | l2[i]) & ~(1u << d))) count_fail++;
            }
            if (l0[6] | l0[7] | l1[6] | l1[7] | l2[6] | l2[7]) count_fail++;
        }
    printf("bit-sliced counts: %ld lane checks, %ld failures\n", count_checks, count_fail);
    failures += count_fail;

    // 2. selection with one digit taking each of the 64 patterns and the others fixed (count 1)
    //    or taking a second, independent pattern
    for (int d = 0; d < 9; d++)
        for (int p = 0; p < 64; p++)
            for (int e = 0; e < 9; e++)
                for (int q = 0; q < 64; q++) {
                    if (e == d && q) continue;
                    uint16_t lanes[8] = {0};
                    for (int k = 0; k < 9; k++) lanes[k % 6] |= (uint16_t) (1u << k);  // count 1 each
                    for (int i = 0; i < 6; i++) lanes[i] &= (uint16_t) ~(1u << d);
                    for (int i = 0; i < 6; i++) if (p >> i & 1) lanes[i] |= (uint16_t) (1u << d);
                    if (e != d) {
                        for (int i = 0; i < 6; i++) lanes[i] &= (uint16_t) ~(1u << e);
                        for (int i = 0; i < 6; i++) if (q >> i & 1) lanes[i] |= (uint16_t) (1u << e);
                    }
                    check(lanes, "pairs");
                }

    // 3. random full vectors: every digit an independent pattern, biased toward high counts
    std::mt19937_64 rng(12345);
    for (int t = 0; t < 2000000; t++) {
        uint16_t lanes[8] = {0};
        int mode = t % 3;
        for (int d = 0; d < 9; d++) {
            int p;
            if (mode == 0) p = (int) (rng() & 63);
            else if (mode == 1) p = (int) ((rng() | rng()) & 63);         // mostly 4..6
            else p = (int) ((rng() & rng() & rng()) & 63) | (1 << (rng() % 6));  // mostly 1..2
            for (int i = 0; i < 6; i++) if (p >> i & 1) lanes[i] |= (uint16_t) (1u << d);
        }
        check(lanes, "random");
    }
    printf("selection: %ld checks, %ld failures\n", checks, failures - count_fail);
    printf(failures ? "FAILED\n" : "ALL PASSED\n");
    return failures != 0;
}
