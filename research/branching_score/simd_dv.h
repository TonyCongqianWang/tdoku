// Candidate DV ("digit volume"): s(B,d) = ln(eps + sum_{c in C(B,d)} prod_j rho_j(c)^al) + g(n) + gR R
//
// 8-bit log arithmetic, unit 1/24 bit (S = 24):
//   Q(k)  = round(S al log2(6 / k))  (k = 0 -> 255 = "volume 0"),  per crossing element
//   U(c)  = Q(k_0) +sat Q(k_1) +sat Q(k_2)               = -S log2(prod (k_j / 6)^al)
//   L     = LSE over the digit's configurations, LSE(a, b) = min(a, b) - corr(|a - b|),
//           corr(D) = round(S log2(1 + 2^(-D/S))) looked up at D >> 3, 0 for D >= 128 or max = 255
//   x     = L - sum_j QN(n_j), QN(n) = round(S al log2(6 / n))  -> -S log2(volume)
//   x     = min(x, Ueps) (Ueps if L = 255), key = 512 - x + G(n) + band term  (16 bit, argmin)
// Configurations are paired across 128-bit lanes, (c0,c3) (c1,c5) (c2,c4), which swaps elements 0
// and 2 of every peer, so the + volumes of all six come from three saturating sums per peer
// arrangement. The branch configuration is the digit's lowest (cpick 0) or the one with the
// largest + volume (cpick 1; scalar, for the chosen digit only).
#pragma once
#include <cmath>

#include "simd_front.h"

namespace sdv {

constexpr double S = 24.0;

struct Params {
    double al = 1.1, eps = 1e-4, g3 = -0.15, g4 = 0.1, gR = 0.015;  // fit3_dv_wide
    int cpick = 1;
    int corr = 1;  // 0: LSE becomes min (no correction)
};

struct Tables {
    __m256i q_lut, qn_lut, corr_lut, bits[3], c1f, c70;
    int ueps, rq, cpick;
    alignas(32) uint8_t q[16], qn[16], corr[16], g16[16];
    int g[8];

    explicit Tables(const Params &p) {
        for (int k = 0; k < 16; k++) {
            q[k] = k == 0 ? 255 : k <= 6 ? (uint8_t) lround(S * p.al * log2(6.0 / k)) : 0;
            qn[k] = k >= 1 && k <= 6 ? (uint8_t) lround(S * p.al * log2(6.0 / k)) : 0;
            corr[k] = p.corr ? (uint8_t) lround(S * log2(1.0 + pow(2.0, -(8.0 * k + 3.5) / S))) : 0;
        }
        q_lut = _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) q));
        qn_lut = _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) qn));
        corr_lut = _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) corr));
        c1f = _mm256_set1_epi8(0x1f);
        c70 = _mm256_set1_epi8(0x70);
        const int pairs[3][2] = {{0, 3}, {1, 5}, {2, 4}};
        for (int k = 0; k < 3; k++)
            bits[k] = _mm256_set_m128i(_mm_set1_epi8((char) (1 << pairs[k][1])), _mm_set1_epi8((char) (1 << pairs[k][0])));
        double unit = S / log(2.0);  // key units per nat
        ueps = (int) lround(-S * log2(p.eps));
        double gmin = std::min(0.0, std::min(p.g3, p.g4));
        for (int n = 0; n < 8; n++) {
            double gn = n == 2 ? 0 : n == 3 ? p.g3 : p.g4;
            g[n] = n >= 2 && n <= 6 ? (int) lround(unit * (gn - gmin)) : 0;
        }
        for (int n = 0; n < 16; n++) g16[n] = n < 8 ? (uint8_t) std::min(255, g[n]) : 0;
        rq = (int) lround(16.0 * unit * p.gR);
        cpick = p.cpick;
    }
};

struct LseConst {
    // gf2p8affine bit permutation: result bits 0..3 <- bits 3..6 of the difference, bit 7 <- bit 7,
    // so pshufb returns corr[D >> 3] for D < 128 and 0 for D >= 128 (result bit k = parity(A.byte[7-k] & x))
    __m256i idx_aff = _mm256_set1_epi64x((long long) (((uint64_t) 0x08 << 56) | ((uint64_t) 0x10 << 48) |
                                                      ((uint64_t) 0x20 << 40) | ((uint64_t) 0x40 << 32) |
                                                      (uint64_t) 0x80));
    __m256i ff = _mm256_set1_epi8(-1);
};
inline const LseConst g_lse{};

// LSE(a, b) = min - corr(|a - b|), no correction when the larger value is 255 ("volume 0")
template <bool kGate = true>
inline __m256i Lse(const __m256i &a, const __m256i &b, const __m256i &lut) {
    __m256i mn = _mm256_min_epu8(a, b), mx = _mm256_max_epu8(a, b);
    __m256i idx = _mm256_gf2p8affine_epi64_epi8(_mm256_sub_epi8(mx, mn), g_lse.idx_aff, 0);
    __m256i corr = _mm256_shuffle_epi8(lut, idx);
    if (kGate) corr = _mm256_andnot_si256(_mm256_cmpeq_epi8(mx, g_lse.ff), corr);
    return _mm256_subs_epu8(mn, corr);
}

inline int LseRef(int a, int b, const Tables &t, bool gate = true) {
    int mn = a < b ? a : b, mx = a < b ? b : a, dd = (mx - mn) >> 3;
    int corr = (dd < 16 && (mx != 255 || !gate)) ? t.corr[dd] : 0;
    return mn - corr < 0 ? 0 : mn - corr;
}

// 16-bit key of one band (lane `half` of the band pair): 512 - clamp(L - N) + G(n) + band term
inline __m256i Key16(const __m128i &L8, const __m128i &N8, const __m128i &n8, const __m128i &g8, int ueps_v,
                     int rterm, bool unfixed) {
    const __m256i ones = _mm256_set1_epi16(-1);
    __m256i L16 = _mm256_cvtepu8_epi16(L8);
    __m256i ueps = _mm256_set1_epi16((short) ueps_v);
    __m256i x = _mm256_min_epi16(_mm256_sub_epi16(L16, _mm256_cvtepu8_epi16(N8)), ueps);
    x = _mm256_blendv_epi8(x, ueps, _mm256_cmpeq_epi16(L16, _mm256_set1_epi16(255)));
    __m256i v = _mm256_add_epi16(_mm256_sub_epi16(_mm256_set1_epi16((short) (512 + rterm)), x),
                                 _mm256_cvtepu8_epi16(g8));
    __m256i cand = _mm256_cmpgt_epi16(_mm256_cvtepu8_epi16(n8), _mm256_set1_epi16(1));
    if (!unfixed) cand = _mm256_setzero_si256();
    return _mm256_or_si256(v, _mm256_andnot_si256(cand, ones));
}

template <typename StateT>
inline bool Choose(const StateT &state, const Tables &T, int &out_b, int &out_d, int &out_c) {
    static constexpr int kE[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}, {0, 2, 1}, {1, 0, 2}};
    int R[6];
    uint32_t unfixed = 0;
    for (int b = 0; b < 6; b++) {
        R[b] = state.bands[b / 3][b % 3].configurations.Popcount();
        unfixed |= (uint32_t) (R[b] >= 10) << b;
    }
    if (__builtin_expect(!unfixed, 0)) return false;
    sfront::Front2 f;
    f.Compute(state.bands, state.boxen);
    const __m256i zero = _mm256_setzero_si256(), ones = _mm256_set1_epi16(-1);
    const __m256i glut = _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) T.g16));
    __m256i qn = _mm256_add_epi8(_mm256_add_epi8(_mm256_shuffle_epi8(T.qn_lut, f.n[0]),
                                                 _mm256_shuffle_epi8(T.qn_lut, f.n[1])),
                                 _mm256_shuffle_epi8(T.qn_lut, f.n[2]));
    __m256i nsw = sfront::Swap(qn);  // [sum over v | sum over h]: the crossing bands of [h_x | v_x]
    __m256i key[6], mn = ones, uv[3][6];
    for (int x = 0; x < 3; x++) {
        __m256i q[3][3];
        for (int p = 0; p < 3; p++)
            for (int e = 0; e < 3; e++) q[p][e] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6b(f.E[x][p][e]));
        __m256i U[6];
        for (int c = 0; c < 6; c++) {
            __m256i u = _mm256_adds_epu8(_mm256_adds_epu8(q[0][kE[c][0]], q[1][kE[c][1]]), q[2][kE[c][2]]);
            __m256i bit = _mm256_set1_epi8((char) (1 << c));
            U[c] = _mm256_or_si256(u, _mm256_cmpeq_epi8(_mm256_and_si256(f.cfg[x], bit), zero));
            uv[x][c] = U[c];
        }
        __m256i L = Lse(Lse(Lse(U[0], U[1], T.corr_lut), U[2], T.corr_lut),
                        Lse(Lse(U[3], U[5], T.corr_lut), U[4], T.corr_lut), T.corr_lut);
        __m256i g8 = _mm256_shuffle_epi8(glut, f.n[x]);
        key[x] = Key16(_mm256_castsi256_si128(L), _mm256_castsi256_si128(nsw), _mm256_castsi256_si128(f.n[x]),
                       _mm256_castsi256_si128(g8), T.ueps, (R[x] * T.rq) >> 4, (unfixed >> x) & 1);
        key[3 + x] = Key16(_mm256_extracti128_si256(L, 1), _mm256_extracti128_si256(nsw, 1),
                           _mm256_extracti128_si256(f.n[x], 1), _mm256_extracti128_si256(g8, 1), T.ueps,
                           (R[3 + x] * T.rq) >> 4, (unfixed >> (3 + x)) & 1);
    }
    for (int b = 0; b < 6; b++) mn = _mm256_min_epu16(mn, key[b]);
    __m128i mn128 = _mm_min_epu16(_mm256_castsi256_si128(mn), _mm256_extracti128_si256(mn, 1));
    uint32_t min_val = (uint32_t) _mm_cvtsi128_si32(_mm_minpos_epu16(mn128)) & 0xffffu;
    __m256i target = _mm256_set1_epi16((short) min_val);
    for (int b = 0; b < 6; b++) {
        uint32_t m = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi16(key[b], target));
        if (m) {
            int d = __builtin_ctz(m) >> 1, x = b % 3, lane = b < 3 ? 0 : 16;
            unsigned cs = sfront::Front::Byte(f.cfg[x], lane + d);
            out_b = b;
            out_d = d;
            out_c = __builtin_ctz(cs);
            if (T.cpick == 1 && __builtin_popcount(cs) > 2) {  // largest + volume = smallest U
                int best = 256;
                for (int c = 0; c < 6; c++) {
                    if (!(cs >> c & 1)) continue;
                    int u = sfront::Front::Byte(uv[x][c], lane + d);
                    if (u < best) { best = u; out_c = c; }
                }
            }
            return true;
        }
    }
    return false;
}

// Scalar mirror (testing): key of digit d given its configuration set, per-config element
// counts k[c][j] and crossing counts nj[j]; also the cpick-1 configuration.
inline unsigned KeyRef(const Tables &T, unsigned cs, const int k[6][3], const int nj[3], int n, int R, int &cbest) {
    int U[6];
    for (int c = 0; c < 6; c++) {
        if (!(cs >> c & 1)) { U[c] = 255; continue; }
        int u = std::min(255, T.q[k[c][0]] + T.q[k[c][1]]);
        U[c] = std::min(255, u + T.q[k[c][2]]);
    }
    cbest = __builtin_ctz(cs);
    if (T.cpick == 1 && n > 2) {
        int best = 256;
        for (int c = 0; c < 6; c++) if ((cs >> c & 1) && U[c] < best) { best = U[c]; cbest = c; }
    }
    int lo = LseRef(LseRef(U[0], U[1], T), U[2], T), hi = LseRef(LseRef(U[3], U[5], T), U[4], T);
    int L = LseRef(lo, hi, T);
    int x = L - (T.qn[nj[0]] + T.qn[nj[1]] + T.qn[nj[2]]);
    x = L == 255 ? T.ueps : std::min(x, T.ueps);
    return (unsigned) (512 - x + T.g[n] + ((R * T.rq) >> 4)) & 0xffff;
}

}  // namespace sdv
