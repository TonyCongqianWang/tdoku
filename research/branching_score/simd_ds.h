// Candidate DS ("digit sum"): ChooseIx generalised to digits with n >= 3 configurations.
//
//   s(B,d) = sum_j (1 / n_j) * sum_{c in C(B,d)} k_j(c) / n^sn + g(n) + gR * R(B)
//
// sum_{c in C} k_j(c) is the number of compatible (band B, crossing band) configuration pairs in
// box j, which is symmetric, so -- exactly as in ChooseIx -- only the row transposes of each box
// are needed. Fixed point (units of 1/240): pair counts times W(n_j) = 240 / n_j (16 bit), then
// times M(n) = 2^16 / n^sn (mulhi), plus G(n) and the band term. The argmin takes the lowest band,
// then the lowest digit on ties; the configuration is the lowest one of the digit.
#pragma once
#include <cmath>

#include "simd_front.h"

namespace sds {

struct Params {
    double sn = 0.5, g3 = -0.25, g4 = -0.5, gR = 0.025;  // fit3_ds_wide
};

struct Tables {
    __m256i w_lut;          // 8 bit: 60 / n (pair counts are added before the 16-bit multiply)
    __m256i m_lo, m_hi;     // M(n) = 65536 / n^sn split in bytes (pshufb on n)
    __m256i g_lut;          // G(n) - min G, in units of 1/240 / 4 (8 bit)
    __m256i ua02[3], ub02[3], ua1[3], ub1[3];
    int r_mul;              // band term: (R * r_mul) >> 4, units of 1/240
    __m256i bitsel;

    explicit Tables(const Params &p) {
        alignas(32) uint8_t w[32] = {0}, ml[32] = {0}, mh[32] = {0}, g[32] = {0};
        double gmin = std::min(0.0, std::min(p.g3, p.g4));
        for (int n = 0; n < 16; n++) {
            w[n] = w[16 + n] = (uint8_t) (n >= 1 && n <= 6 ? 60 / n : 0);
            unsigned m = n >= 2 && n <= 6 ? (unsigned) lround(65535.0 / pow((double) n, p.sn)) : 0;
            ml[n] = ml[16 + n] = (uint8_t) (m & 0xff);
            mh[n] = mh[16 + n] = (uint8_t) (m >> 8);
            double gn = n == 2 ? 0 : n == 3 ? p.g3 : p.g4;
            long q = n >= 2 && n <= 6 ? lround((gn - gmin) * 240.0 / 4.0) : 0;
            g[n] = g[16 + n] = (uint8_t) (q < 0 ? 0 : q > 255 ? 255 : q);
        }
        w_lut = _mm256_load_si256((const __m256i *) w);
        m_lo = _mm256_load_si256((const __m256i *) ml);
        m_hi = _mm256_load_si256((const __m256i *) mh);
        g_lut = _mm256_load_si256((const __m256i *) g);
        r_mul = (int) lround(p.gR * 240.0 * 16.0);
        const int kE[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}, {0, 2, 1}, {1, 0, 2}};
        for (int q = 0; q < 3; q++) {
            uint8_t using_elem[3] = {0, 0, 0};
            for (int c = 0; c < 6; c++) using_elem[kE[c][q]] |= (uint8_t) (1u << c);
            auto lo = [](uint32_t m) { return (uint8_t) (m & (0u - m)); };
            auto hi = [](uint32_t m) { return (uint8_t) (m & (m - 1)); };
            ua02[q] = _mm256_set_m128i(_mm_set1_epi8((char) lo(using_elem[2])), _mm_set1_epi8((char) lo(using_elem[0])));
            ub02[q] = _mm256_set_m128i(_mm_set1_epi8((char) hi(using_elem[2])), _mm_set1_epi8((char) hi(using_elem[0])));
            ua1[q] = _mm256_set1_epi8((char) lo(using_elem[1]));
            ub1[q] = _mm256_set1_epi8((char) hi(using_elem[1]));
        }
    }
};

// Returns false if every band is fixed. Otherwise the chosen band b (0..5), digit d, config c.
template <typename StateT>
inline bool Choose(const StateT &state, const Tables &T, int &out_b, int &out_d, int &out_c) {
    const sfront::Tables &F = sfront::T();
    int R[6];
    uint32_t unfixed = 0;
    for (int b = 0; b < 6; b++) {
        R[b] = state.bands[b / 3][b % 3].configurations.Popcount();
        unfixed |= (uint32_t) (R[b] >= 10) << b;
    }
    if (__builtin_expect(!unfixed, 0)) return false;
    __m256i cfgv[3], nv[3], wv[3];
    for (int x = 0; x < 3; x++) {
        __m256i r = _mm256_set_m128i(state.bands[1][x].configurations.vec,
                                     state.bands[0][x].configurations.vec);
        cfgv[x] = sfront::GfT(r, F.gf_cfg);
        nv[x] = sfront::Pop6(cfgv[x]);
        wv[x] = _mm256_shuffle_epi8(T.w_lut, nv[x]);
    }
    // pair counts times crossing weights, accumulated per band (16 bit, units of 1/60)
    __m256i acc[6];
    for (int b = 0; b < 6; b++) acc[b] = _mm256_setzero_si256();
    for (int i = 0; i < 3; i++) {
        __m256i hh = sfront::Lo2(cfgv[i]);
        for (int j = 0; j < 3; j++) {
            const __m256i cells = state.boxen[i * 3 + j].cells.vec;
            __m256i r02 = sfront::GfT(cells, F.gf_row02);
            __m256i r1 = sfront::GfT(cells, F.gf_row1);
            __m256i vv = sfront::Hi2(cfgv[j]);
            __m256i k02 = sfront::Pop6(_mm256_and_si256(vv, _mm256_shuffle_epi8(F.compat_rr[i], r02)));
            __m256i k1 = sfront::Pop6(_mm256_and_si256(vv, _mm256_shuffle_epi8(F.compat_rr[i], r1)));
            auto has = [&](const __m256i &bitc) {
                return _mm256_cmpeq_epi8(_mm256_and_si256(hh, bitc), bitc);
            };
            __m256i p02 = _mm256_add_epi8(_mm256_and_si256(has(T.ua02[j]), k02),
                                          _mm256_and_si256(has(T.ub02[j]), k02));
            __m256i p1 = _mm256_add_epi8(_mm256_and_si256(has(T.ua1[j]), k1),
                                         _mm256_and_si256(has(T.ub1[j]), k1));
            __m128i pairs = _mm_add_epi8(_mm_add_epi8(_mm256_castsi256_si128(p02),
                                                      _mm256_extracti128_si256(p02, 1)),
                                         _mm256_castsi256_si128(p1));
            __m256i p16 = _mm256_cvtepu8_epi16(pairs);
            __m256i wv_v = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(wv[j], 1));
            __m256i wv_h = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(wv[i]));
            acc[i] = _mm256_add_epi16(acc[i], _mm256_mullo_epi16(wv_v, p16));
            acc[3 + j] = _mm256_add_epi16(acc[3 + j], _mm256_mullo_epi16(wv_h, p16));
        }
    }
    // key = acc * 4 * M(n) >> 16 + 4 * G(n) + band term (units of 1/240); invalid -> 0xffff
    const __m256i ones = _mm256_set1_epi16(-1);
    __m256i key[6], mn = ones;
    for (int b = 0; b < 6; b++) {
        __m256i n8 = b < 3 ? nv[b] : sfront::Swap(nv[b - 3]);
        __m256i mlo = _mm256_shuffle_epi8(T.m_lo, n8), mhi = _mm256_shuffle_epi8(T.m_hi, n8);
        __m256i m16 = _mm256_unpacklo_epi8(mlo, mhi);  // bytes 0..7 -> lanes 0..7, 16..23 -> lanes 8..15
        // lanes: unpacklo gives digits 0..7 in the low lane and 16..23 (junk) in the high lane;
        // digit 8 needs unpackhi of the low lane, so rebuild a 16-lane vector over digits 0..15
        __m256i m16b = _mm256_unpackhi_epi8(mlo, mhi);
        __m256i mfull = _mm256_permute2x128_si256(m16, m16b, 0x20);
        __m256i gq = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(_mm256_shuffle_epi8(T.g_lut, n8)));
        __m256i n16 = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(n8));
        __m256i v = _mm256_mulhi_epu16(_mm256_slli_epi16(acc[b], 2), mfull);
        v = _mm256_add_epi16(v, _mm256_slli_epi16(gq, 2));
        v = _mm256_add_epi16(v, _mm256_set1_epi16((short) ((R[b] * T.r_mul) >> 4)));
        __m256i cand = _mm256_cmpgt_epi16(n16, _mm256_set1_epi16(1));
        if (!((unfixed >> b) & 1)) cand = _mm256_setzero_si256();
        key[b] = _mm256_or_si256(v, _mm256_andnot_si256(cand, ones));
        mn = _mm256_min_epu16(mn, key[b]);
    }
    __m128i mn128 = _mm_min_epu16(_mm256_castsi256_si128(mn), _mm256_extracti128_si256(mn, 1));
    uint32_t min_val = (uint32_t) _mm_cvtsi128_si32(_mm_minpos_epu16(mn128)) & 0xffffu;
    __m256i target = _mm256_set1_epi16((short) min_val);
    for (int b = 0; b < 6; b++) {
        uint32_t m = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi16(key[b], target));
        if (m) {
            int d = __builtin_ctz(m) >> 1;
            out_b = b;
            out_d = d;
            out_c = __builtin_ctz((unsigned) sfront::Front::Byte(cfgv[b % 3], (b < 3 ? 0 : 16) + d));
            return true;
        }
    }
    return false;  // unreachable: an unfixed band has a digit with n >= 2
}

// Scalar mirror of the fixed-point key (testing). pairs[j] = sum_{c in C} k_j(c), nj[j] crossing n.
inline unsigned KeyRef(const Tables &T, const Params &p, const int pairs[3], const int nj[3], int n, int R) {
    unsigned acc = 0;
    for (int j = 0; j < 3; j++) acc += (unsigned) (pairs[j] * (60 / nj[j]));
    acc &= 0xffff;
    unsigned m = (unsigned) lround(65535.0 / pow((double) n, p.sn));
    unsigned v = (((acc << 2) & 0xffff) * m) >> 16;
    alignas(32) uint8_t g[32];
    _mm256_store_si256((__m256i *) g, T.g_lut);
    v += (unsigned) g[n] << 2;
    v += (unsigned) ((R * T.r_mul) >> 4);
    return v & 0xffff;
}

}  // namespace sds
