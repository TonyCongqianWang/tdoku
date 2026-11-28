#include "dsel.h"
using namespace dsel;
extern "C" {
__attribute__((noinline)) __m128i k_A(__m128i x) { return SelA(x); }
__attribute__((noinline)) __m128i k_B(__m128i x) { return SelB(x); }
__attribute__((noinline)) __m128i k_C(__m128i x) { return SelC(x); }
__attribute__((noinline)) __m128i k_D1(__m128i x) { return SelD1(x); }
__attribute__((noinline)) __m128i k_D2(__m128i x) { return SelD2(x); }
__attribute__((noinline)) __m128i k_E1(__m128i x) { return SelE1(x); }
__attribute__((noinline)) __m128i k_E2(__m128i x) { return SelE2(x); }
#ifdef __GFNI__
__attribute__((noinline)) __m128i k_F(__m128i x) { return SelF(x); }
#endif
__attribute__((noinline)) __m128i k_DB(__m128i x) { return SelDB(x); }
#if 0
#endif
}
