// Microbenchmark of the digit-selection kernels on captured (or synthetic) band states.
// usage: mbench <variant> <mode: tput|lat> <states.bin|synth:uniform|synth:high> [n_states] [reps]
// Prints TSC cycles per call (minimum over 9 trials). Run one variant per process so that perf
// stat counters can be attributed to a single kernel.
#include "dsel.h"
#include <x86intrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include <string>

using namespace dsel;

DSEL_INLINE __m128i Identity(__m128i x) { return x; }

template<int V>
DSEL_INLINE __m128i Run(__m128i x) {
    if (V == 0) return Identity(x);
    if (V == 1) return SelA(x);
    if (V == 2) return SelB(x);
    if (V == 3) return SelC(x);
    if (V == 4) return SelD1(x);
    if (V == 5) return SelD2(x);
    if (V == 6) return SelE1(x);
    if (V == 7) return SelE2(x);
#ifdef __GFNI__
    if (V == 8) return SelF(x);
#endif
    if (V == 9) return SelDB(x);
    return x;
}

static const char *kNames[] = {"identity", "A", "B", "C", "D1", "D2", "E1", "E2", "F", "DB"};

template<int V>
__attribute__((noinline)) double Bench(const __m128i *st, size_t n, int reps, bool latency, uint64_t &sink) {
    double best = 1e30;
    for (int t = 0; t < 9; t++) {
        __m128i acc = _mm_setzero_si128();
        uint64_t t0 = __rdtsc();
        if (!latency) {
            for (int r = 0; r < reps; r++)
                for (size_t i = 0; i < n; i++) acc = _mm_xor_si128(acc, Run<V>(st[i]));
        } else {
            size_t idx = 0;
            for (int r = 0; r < reps; r++)
                for (size_t i = 0; i < n; i++) {
                    __m128i m = Run<V>(st[idx]);
                    acc = _mm_xor_si128(acc, m);
                    // the next state's index depends on this call's result (bit 15 is always 0)
                    idx = (idx + 1 + ((uint32_t) _mm_cvtsi128_si32(m) >> 15 & 1)) & (n - 1);
                }
        }
        uint64_t t1 = __rdtsc();
        sink += (uint64_t) _mm_cvtsi128_si64(acc);
        double c = double(t1 - t0) / (double(reps) * double(n));
        if (c < best) best = c;
    }
    return best;
}

int main(int argc, char **argv) {
    int v = -1;
    for (int i = 0; i < 10; i++) if (!strcmp(argv[1], kNames[i])) v = i;
    if (v < 0) { fprintf(stderr, "unknown variant\n"); return 1; }
    bool latency = !strcmp(argv[2], "lat");
    std::string src = argv[3];
    size_t n = argc > 4 ? (size_t) atol(argv[4]) : 2048;  // power of two
    int reps = argc > 5 ? atoi(argv[5]) : 2000;
    std::vector<__m128i> all;
    if (src.rfind("synth:", 0) == 0) {
        std::mt19937_64 rng(7);
        bool high = src == "synth:high";
        while (all.size() < n) {
            alignas(16) uint16_t lanes[8] = {0};
            for (int d = 0; d < 9; d++) {
                int p = high ? (int) ((rng() | rng()) & 63) : (int) (rng() & 63);
                for (int i = 0; i < 6; i++) if (p >> i & 1) lanes[i] |= (uint16_t) (1u << d);
            }
            if (RefExact(lanes) < 0) continue;
            all.push_back(_mm_load_si128((const __m128i *) lanes));
        }
    } else {
        FILE *f = fopen(src.c_str(), "rb");
        __m128i x;
        while (fread(&x, 16, 1, f) == 1) all.push_back(x);
        fclose(f);
        // evenly spaced sample of n states, keeping the solver's order within the sample
        std::vector<__m128i> s;
        for (size_t i = 0; i < n; i++) s.push_back(all[i * all.size() / n]);
        all.swap(s);
    }
    __m128i *st = (__m128i *) aligned_alloc(64, n * 16);
    memcpy(st, all.data(), n * 16);
    uint64_t sink = 0;
    double c = 0;
    switch (v) {
        case 0: c = Bench<0>(st, n, reps, latency, sink); break;
        case 1: c = Bench<1>(st, n, reps, latency, sink); break;
        case 2: c = Bench<2>(st, n, reps, latency, sink); break;
        case 3: c = Bench<3>(st, n, reps, latency, sink); break;
        case 4: c = Bench<4>(st, n, reps, latency, sink); break;
        case 5: c = Bench<5>(st, n, reps, latency, sink); break;
        case 6: c = Bench<6>(st, n, reps, latency, sink); break;
        case 7: c = Bench<7>(st, n, reps, latency, sink); break;
        case 8: c = Bench<8>(st, n, reps, latency, sink); break;
        case 9: c = Bench<9>(st, n, reps, latency, sink); break;
    }
    printf("%s %s %s %.2f %llu\n", kNames[v], argv[2], argv[3], c, (unsigned long long) (sink & 1));
}
