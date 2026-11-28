// Candidate LV ("branch volumes"): for the split C(B,d) -> {c} | C \ {c},
//   s(B,d,c) = ln(eps + (1/n)^be P+ + ((n-1)/n)^be P-) + g(n) + gR R,  P = prod_j rho_j^al
// with the - branch compatibility taken over the union of the rows (columns) of C \ {c}.
//
// Same 8-bit log arithmetic as DV (simd_dv.h). The - branch counts use, per band and peer j,
//   M_e = S_e if C uses element e at j else 0,  T = M_0 | M_1 | M_2,  X_e = OR_{e' != e} M_e'
//   k-(c) = |T| if the partner of c at j (the other configuration with c's element) is in C,
//           |X_e(c)| otherwise
// so every k- is a blend of two precomputed table values. Per digit the best configuration is the
// one with the largest branch LSE (keys are monotone in it), then the key is formed as in DV.
#pragma once
#include <cmath>

#include "simd_front.h"
#include "simd_dv.h"

namespace slv {

constexpr double S = sdv::S;
#ifndef LV_GATE
#define LV_GATE 1
#endif
constexpr bool kLvGate = LV_GATE;

struct Params {
    double al = 1.1, be = 0.6, eps = 1e-3, g3 = -0.25, g4 = -0.1, gR = 0.015;  // fit3_lv_wide
    int corr = 1;
};

const int kE[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}, {0, 2, 1}, {1, 0, 2}};
const int kPair[3][2] = {{0, 3}, {1, 5}, {2, 4}};  // configurations of the lanes of pair vector k

// the other configuration with the same element as c at peer j
constexpr int kPartner[6][3] = {{4, 3, 5}, {5, 4, 3}, {3, 5, 4}, {2, 0, 1}, {0, 1, 2}, {1, 2, 0}};
inline int Partner(int c, int j) { return kPartner[c][j]; }

struct Tables {
    __m256i q_lut, qn_lut, corr_lut, fp_lut, fm_lut, g_lut;
    __m256i bits[3];        // member bits of the pair vectors
    __m256i pbits[3][3];    // partner bits of pair vector k at peer j
    __m256i ubits02[3], ubits1[3];  // configurations using elements [0 | 2] and [1 | 1] of peer j
    __m256i cbit[6], ubit[3][3], ff;  // set1(1 << c); configs using element e at peer p
    int ueps, rq;
    alignas(32) uint8_t q[16], qn[16], corr[16], fp[16], fm[16], g8[16];
    int g[8];
    sdv::Tables dvt;  // for the shared LSE helpers

    static __m256i Lanes(int lo, int hi) {
        return _mm256_set_m128i(_mm_set1_epi8((char) hi), _mm_set1_epi8((char) lo));
    }
    explicit Tables(const Params &p) : dvt(sdv::Params{p.al, p.eps, p.g3, p.g4, p.gR, 0, p.corr}) {
        for (int k = 0; k < 16; k++) {
            q[k] = k == 0 ? 255 : k <= 6 ? (uint8_t) lround(S * p.al * log2(6.0 / k)) : 0;
            qn[k] = k >= 1 && k <= 6 ? (uint8_t) lround(S * p.al * log2(6.0 / k)) : 0;
            corr[k] = dvt.corr[k];
            fp[k] = k >= 2 && k <= 6 ? (uint8_t) lround(S * p.be * log2((double) k)) : 0;
            fm[k] = k >= 2 && k <= 6 ? (uint8_t) lround(S * p.be * log2(k / (k - 1.0))) : 0;
        }
        auto bc = [](const uint8_t *t) { return _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) t)); };
        q_lut = bc(q); qn_lut = bc(qn); corr_lut = bc(corr); fp_lut = bc(fp); fm_lut = bc(fm);
        for (int k = 0; k < 3; k++) {
            bits[k] = Lanes(1 << kPair[k][0], 1 << kPair[k][1]);
            for (int j = 0; j < 3; j++)
                pbits[k][j] = Lanes(1 << Partner(kPair[k][0], j), 1 << Partner(kPair[k][1], j));
        }
        ff = _mm256_set1_epi8(-1);
        for (int c = 0; c < 6; c++) cbit[c] = _mm256_set1_epi8((char) (1 << c));
        for (int j = 0; j < 3; j++)
            for (int e = 0; e < 3; e++) {
                int u = 0;
                for (int c = 0; c < 6; c++) if (kE[c][j] == e) u |= 1 << c;
                ubit[j][e] = _mm256_set1_epi8((char) u);
            }
        for (int j = 0; j < 3; j++) {
            int u[3] = {0, 0, 0};
            for (int c = 0; c < 6; c++) u[kE[c][j]] |= 1 << c;
            ubits02[j] = Lanes(u[0], u[2]);
            ubits1[j] = Lanes(u[1], u[1]);
        }
        double unit = S / log(2.0);
        ueps = (int) lround(-S * log2(p.eps));
        double gmin = std::min(0.0, std::min(p.g3, p.g4));
        for (int n = 0; n < 8; n++) {
            double gn = n == 2 ? 0 : n == 3 ? p.g3 : p.g4;
            g[n] = n >= 2 && n <= 6 ? (int) lround(unit * (gn - gmin)) : 0;
        }
        for (int n = 0; n < 16; n++) g8[n] = n < 8 ? (uint8_t) std::min(255, g[n]) : 0;
        g_lut = bc(g8);
        rq = (int) lround(16.0 * unit * p.gR);
    }
};

// arrangement of the element vectors for pair vector k at peer j: 0 = A, 1 = A' (lanes swapped), 2 = B
constexpr int kArr[3][3] = {{0, 2, 1}, {2, 1, 0}, {1, 0, 2}};

template <typename StateT>
inline bool Choose(const StateT &state, const Tables &T, int &out_b, int &out_d, int &out_c) {
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
    __m256i qn = _mm256_add_epi8(_mm256_add_epi8(_mm256_shuffle_epi8(T.qn_lut, f.n[0]),
                                                 _mm256_shuffle_epi8(T.qn_lut, f.n[1])),
                                 _mm256_shuffle_epi8(T.qn_lut, f.n[2]));
    __m256i nsw = sfront::Swap(qn);
    __m256i key[6], mn = ones, lv[3][6];
#pragma GCC unroll 3
    for (int x = 0; x < 3; x++) {
        const __m256i cfg = f.cfg[x];
        __m256i member[6], nonmember[6];
        for (int c = 0; c < 6; c++) {
            nonmember[c] = _mm256_cmpeq_epi8(_mm256_and_si256(cfg, T.cbit[c]), zero);
            member[c] = _mm256_xor_si256(nonmember[c], T.ff);
        }
        // own-band fractions are folded into peer 0 (saturating adds of non-negative values commute)
        __m256i fp = _mm256_shuffle_epi8(T.fp_lut, f.n[x]), fm = _mm256_shuffle_epi8(T.fm_lut, f.n[x]);
        __m256i qp[3][3], qx[3][3], qt[3];
        for (int p = 0; p < 3; p++) {
            __m256i m[3];
            for (int e = 0; e < 3; e++) {
                const __m256i &E = f.E[x][p][e];
                qp[p][e] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6b(E));
                if (p == 0) qp[p][e] = _mm256_adds_epu8(qp[p][e], fp);
                m[e] = _mm256_andnot_si256(_mm256_cmpeq_epi8(_mm256_and_si256(cfg, T.ubit[p][e]), zero), E);
            }
            __m256i x0 = _mm256_or_si256(m[1], m[2]), x1 = _mm256_or_si256(m[0], m[2]);
            __m256i x2 = _mm256_or_si256(m[0], m[1]), t = _mm256_or_si256(x0, m[0]);
            qx[p][0] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6b(x0));
            qx[p][1] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6b(x1));
            qx[p][2] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6b(x2));
            qt[p] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6b(t));
            if (p == 0) {
                for (int e = 0; e < 3; e++) qx[p][e] = _mm256_adds_epu8(qx[p][e], fm);
                qt[p] = _mm256_adds_epu8(qt[p], fm);
            }
        }
        __m256i lmax = zero;
#pragma GCC unroll 6
        for (int c = 0; c < 6; c++) {
            __m256i up = zero, um = zero;
#pragma GCC unroll 3
            for (int p = 0; p < 3; p++) {
                int e = kE[c][p];
                // partner in C: |T| (the smaller Q), else |X_e|:  min(Q(X_e), Q(T) | nonmember(partner))
                __m256i qm = _mm256_min_epu8(qx[p][e], _mm256_or_si256(qt[p], nonmember[Partner(c, p)]));
                up = p == 0 ? qp[p][e] : _mm256_adds_epu8(up, qp[p][e]);
                um = p == 0 ? qm : _mm256_adds_epu8(um, qm);
            }
            __m256i l = kLvGate ? sdv::Lse<true>(up, um, T.corr_lut) : sdv::Lse<false>(up, um, T.corr_lut);
            l = _mm256_and_si256(l, member[c]);
            lv[x][c] = l;
            lmax = _mm256_max_epu8(lmax, l);
        }
        __m256i g8 = _mm256_shuffle_epi8(T.g_lut, f.n[x]);
        key[x] = sdv::Key16(_mm256_castsi256_si128(lmax), _mm256_castsi256_si128(nsw), _mm256_castsi256_si128(f.n[x]),
                            _mm256_castsi256_si128(g8), T.ueps, (R[x] * T.rq) >> 4, (unfixed >> x) & 1);
        key[3 + x] = sdv::Key16(_mm256_extracti128_si256(lmax, 1), _mm256_extracti128_si256(nsw, 1),
                                _mm256_extracti128_si256(f.n[x], 1), _mm256_extracti128_si256(g8, 1), T.ueps,
                                (R[3 + x] * T.rq) >> 4, (unfixed >> (3 + x)) & 1);
    }
    for (int b = 0; b < 6; b++) mn = _mm256_min_epu16(mn, key[b]);
    __m128i mn128 = _mm_min_epu16(_mm256_castsi256_si128(mn), _mm256_extracti128_si256(mn, 1));
    uint32_t min_val = (uint32_t) _mm_cvtsi128_si32(_mm_minpos_epu16(mn128)) & 0xffffu;
    __m256i target = _mm256_set1_epi16((short) min_val);
    for (int b = 0; b < 6; b++) {
        uint32_t mk = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi16(key[b], target));
        if (mk) {
            int d = __builtin_ctz(mk) >> 1, x = b % 3, lane = b < 3 ? 0 : 16;
            unsigned cs = sfront::Front::Byte(f.cfg[x], lane + d);
            int qnd = sfront::Front::Byte(nsw, lane + d);
            auto xof = [&](int l) { return l == 255 ? T.ueps : std::min(l - qnd, T.ueps); };
            int lbest = 0;
            for (int c = 0; c < 6; c++) if (cs >> c & 1) lbest = std::max(lbest, (int) sfront::Front::Byte(lv[x][c], lane + d));
            int xb = xof(lbest);
            out_b = b;
            out_d = d;
            for (int c = 0; c < 6; c++)
                if ((cs >> c & 1) && xof(sfront::Front::Byte(lv[x][c], lane + d)) == xb) { out_c = c; break; }
            return true;
        }
    }
    return false;
}

// Scalar mirror: key and best configuration of one digit, from + / - element counts
inline unsigned KeyRef(const Tables &T, unsigned cs, const int kp[6][3], const int km[6][3], const int nj[3],
                       int n, int R, int &cbest) {
    int qnsum = T.qn[nj[0]] + T.qn[nj[1]] + T.qn[nj[2]];
    unsigned best = 0x10000;
    for (int c = 0; c < 6; c++) {
        if (!(cs >> c & 1)) continue;
        int up = std::min(255, std::min(255, T.q[kp[c][0]] + T.q[kp[c][1]]) + T.q[kp[c][2]]);
        int um = std::min(255, std::min(255, T.q[km[c][0]] + T.q[km[c][1]]) + T.q[km[c][2]]);
        int vp = std::min(255, up + T.fp[n]), vm = std::min(255, um + T.fm[n]);
        int L = sdv::LseRef(vp, vm, T.dvt, kLvGate);
        int x = L == 255 ? T.ueps : std::min(L - qnsum, T.ueps);
        unsigned key = (unsigned) (512 - x + T.g[n] + ((R * T.rq) >> 4)) & 0xffff;
        if (key < best) { best = key; cbest = c; }
    }
    return best;
}

}  // namespace slv
