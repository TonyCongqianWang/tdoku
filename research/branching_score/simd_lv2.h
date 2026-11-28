// LV2: the LV score (simd_lv.h) on packed vectors with 8-bit keys.
//
// Packing: the band-pair vectors a_x = [h_x | v_x] use bytes 0..8 of each lane. Three of them are
// packed into two:  PA = [a_0 digits 0..7 | a_1 digits 0..7]  (per lane),
//                   PB = [a_2 digits 0..8 | a_0 digit 8 | a_1 digit 8 | 0...]
// (3 operations per triple), so every per-digit operation after the front end runs on 2 vectors
// instead of 3. All tables are per byte and independent of the band, so they apply unchanged.
//
// Key (8 bit, maximised): y = max(1, avg(Lmax, 255 - B)),  B = N + G(n) + band term (saturating),
// Lmax = max over the digit's configurations of LSE(V+, V-), no eps clamp (a refuted branch has
// volume 0 through Q(0) = 255). Candidates are the digits with n >= 2 (a fixed band has n = 1 for
// every digit). Ties: lowest band, then digit, then configuration.
#pragma once
#include <cmath>

#include "simd_dv.h"
#include "simd_front.h"
#include "simd_lv.h"

namespace slv2 {

using slv::kE;
using slv::kPartner;

struct Params {
    double al = 1.1, be = 0.6, g3 = -0.25, g4 = -0.1, gR = 0.015;
};

struct Tables {
    __m256i q_lut, qn_lut, corr_lut, fp_lut, fm_lut, g_lut, valid_lut, cbit[6], ubit[3][3], ff, one, pack_ctrl;
    __m256i rep9, rq16;
    alignas(32) uint8_t q[16], qn[16], corr[16], fp[16], fm[16], g8[16];
    int rq;
    explicit Tables(const Params &p) {
        slv::Params lp;
        lp.al = p.al; lp.be = p.be; lp.g3 = p.g3; lp.g4 = p.g4; lp.gR = p.gR;
        slv::Tables t(lp);
        memcpy(q, t.q, 16); memcpy(qn, t.qn, 16); memcpy(corr, t.corr, 16);
        memcpy(fp, t.fp, 16); memcpy(fm, t.fm, 16); memcpy(g8, t.g8, 16);
        q_lut = t.q_lut; qn_lut = t.qn_lut; corr_lut = t.corr_lut; fp_lut = t.fp_lut; fm_lut = t.fm_lut;
        g_lut = t.g_lut; rq = t.rq;
        for (int c = 0; c < 6; c++) cbit[c] = t.cbit[c];
        for (int j = 0; j < 3; j++) for (int e = 0; e < 3; e++) ubit[j][e] = t.ubit[j][e];
        alignas(16) uint8_t v[16];
        for (int n = 0; n < 16; n++) v[n] = n >= 2 && n <= 6 ? 0xff : 0;
        valid_lut = _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) v));
        ff = _mm256_set1_epi8(-1);
        one = _mm256_set1_epi8(1);
        alignas(16) int8_t pc[16];
        for (int k = 0; k < 16; k++) pc[k] = (int8_t) 0x80;
        pc[9] = 0;   // a_0 digit 8 (byte 0 of unpackhi)
        pc[10] = 8;  // a_1 digit 8 (byte 8 of unpackhi)
        pack_ctrl = _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) pc));
        for (int k = 0; k < 16; k++) pc[k] = k <= 8 ? 0 : (int8_t) 0x80;  // byte 0 -> bytes 0..8
        rep9 = _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) pc));
        rq16 = _mm256_set1_epi16((short) rq);
    }
};

struct Packed {
    __m256i a, b;
};

inline Packed Pack(const __m256i &a0, const __m256i &a1, const __m256i &a2, const __m256i &ctrl) {
    return {_mm256_unpacklo_epi64(a0, a1),
            _mm256_or_si256(a2, _mm256_shuffle_epi8(_mm256_unpackhi_epi64(a0, a1), ctrl))};
}

// band and digit of byte k (0..31) of packed vector A (half 0) or B (half 1); band -1 if unused
inline void Locate(int half, int k, int &b, int &d) {
    int lane = k >> 4, i = k & 15;
    if (half == 0) { b = (i >> 3) + 3 * lane; d = i & 7; return; }
    if (i <= 8) { b = 2 + 3 * lane; d = i; return; }
    if (i <= 10) { b = (i - 9) + 3 * lane; d = 8; return; }
    b = -1;
}
// byte of (band b, digit d)
inline void Where(int b, int d, int &half, int &k) {
    int x = b % 3, lane = b < 3 ? 0 : 16;
    if (x == 2) { half = 1; k = lane + d; return; }
    if (d == 8) { half = 1; k = lane + 9 + x; return; }
    half = 0; k = lane + 8 * x + d;
}

// one packed half: Lmax per byte, and L per configuration (for the tail)
inline __m256i Half(const Tables &T, const __m256i E[3][3], const __m256i &cfg, const __m256i &n,
                    __m256i lv[6]) {
    const __m256i zero = _mm256_setzero_si256();
    __m256i member[6], nonmember[6];
    for (int c = 0; c < 6; c++) {
        nonmember[c] = _mm256_cmpeq_epi8(_mm256_and_si256(cfg, T.cbit[c]), zero);
        member[c] = _mm256_xor_si256(nonmember[c], T.ff);
    }
    __m256i fp = _mm256_shuffle_epi8(T.fp_lut, n), fm = _mm256_shuffle_epi8(T.fm_lut, n);
    __m256i qp[3][3], qx[3][3], qt[3];
#pragma GCC unroll 3
    for (int p = 0; p < 3; p++) {
        __m256i m[3];
#pragma GCC unroll 3
        for (int e = 0; e < 3; e++) {
            qp[p][e] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6(E[p][e]));
            if (p == 0) qp[p][e] = _mm256_adds_epu8(qp[p][e], fp);
            m[e] = _mm256_andnot_si256(_mm256_cmpeq_epi8(_mm256_and_si256(cfg, T.ubit[p][e]), zero), E[p][e]);
        }
        __m256i x0 = _mm256_or_si256(m[1], m[2]), x1 = _mm256_or_si256(m[0], m[2]);
        __m256i x2 = _mm256_or_si256(m[0], m[1]), t = _mm256_or_si256(x0, m[0]);
        qx[p][0] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6(x0));
        qx[p][1] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6(x1));
        qx[p][2] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6(x2));
        qt[p] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6(t));
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
            __m256i qm = _mm256_min_epu8(qx[p][e], _mm256_or_si256(qt[p], nonmember[kPartner[c][p]]));
            up = p == 0 ? qp[p][e] : _mm256_adds_epu8(up, qp[p][e]);
            um = p == 0 ? qm : _mm256_adds_epu8(um, qm);
        }
        __m256i l = _mm256_and_si256(sdv::Lse<false>(up, um, T.corr_lut), member[c]);
        lv[c] = l;
        lmax = _mm256_max_epu8(lmax, l);
    }
    return lmax;
}

template <typename StateT>
inline bool Choose(const StateT &state, const Tables &T, int &out_b, int &out_d, int &out_c) {
    sfront::Front2 f;
    f.Compute(state.bands, state.boxen);
    Packed cfg = Pack(f.cfg[0], f.cfg[1], f.cfg[2], T.pack_ctrl);
    Packed n = Pack(f.n[0], f.n[1], f.n[2], T.pack_ctrl);
    __m256i validA = _mm256_shuffle_epi8(T.valid_lut, n.a), validB = _mm256_shuffle_epi8(T.valid_lut, n.b);
    if (__builtin_expect(_mm256_testz_si256(_mm256_or_si256(validA, validB), T.ff), 0)) return false;
    // band term (R * rq) >> 4 per byte, R = sum over the band's digits of n (SAD per lane)
    __m256i rtx[3];
    for (int x = 0; x < 3; x++) {
        __m256i sad = _mm256_sad_epu8(f.n[x], _mm256_setzero_si256());
        __m256i r = _mm256_add_epi64(sad, _mm256_bsrli_epi128(sad, 8));  // R in the low word of each lane
        r = _mm256_srli_epi16(_mm256_mullo_epi16(r, T.rq16), 4);
        rtx[x] = _mm256_shuffle_epi8(r, T.rep9);
    }
    Packed rt = Pack(rtx[0], rtx[1], rtx[2], T.pack_ctrl);
    const __m256i &rtA = rt.a, &rtB = rt.b;
    __m256i qn = _mm256_add_epi8(_mm256_add_epi8(_mm256_shuffle_epi8(T.qn_lut, f.n[0]),
                                                 _mm256_shuffle_epi8(T.qn_lut, f.n[1])),
                                 _mm256_shuffle_epi8(T.qn_lut, f.n[2]));
    __m256i nsw = sfront::Swap(qn);  // crossing normalisation, the same for every band pair
    Packed N = Pack(nsw, nsw, nsw, T.pack_ctrl);
    __m256i EA[3][3], EB[3][3];
    for (int p = 0; p < 3; p++)
        for (int e = 0; e < 3; e++) {
            Packed pe = Pack(f.E[0][p][e], f.E[1][p][e], f.E[2][p][e], T.pack_ctrl);
            EA[p][e] = pe.a;
            EB[p][e] = pe.b;
        }
    __m256i lvA[6], lvB[6];
    __m256i lA = Half(T, EA, cfg.a, n.a, lvA), lB = Half(T, EB, cfg.b, n.b, lvB);
    auto key = [&](const __m256i &l, const __m256i &nn, const __m256i &NN, const __m256i &r, const __m256i &valid) {
        __m256i B = _mm256_adds_epu8(_mm256_adds_epu8(NN, _mm256_shuffle_epi8(T.g_lut, nn)), r);
        __m256i y = _mm256_max_epu8(_mm256_avg_epu8(l, _mm256_xor_si256(B, T.ff)), T.one);
        return _mm256_and_si256(y, valid);
    };
    __m256i yA = key(lA, n.a, N.a, rtA, validA), yB = key(lB, n.b, N.b, rtB, validB);
    // global maximum, then the first (band, digit) holding it
    __m256i m = _mm256_max_epu8(yA, yB);
    m = _mm256_max_epu8(m, sfront::Swap(m));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 8));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 4));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 2));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 1));
    __m256i target = _mm256_broadcastb_epi8(_mm256_castsi256_si128(m));
    uint32_t ma = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi8(yA, target));
    uint32_t mb = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi8(yB, target));
    // per band 9-bit digit masks, in band order
    uint32_t dm[6] = {
            (ma & 0xff) | ((mb >> 9 & 1) << 8),          (ma >> 8 & 0xff) | ((mb >> 10 & 1) << 8),
            mb & 0x1ff,                                  (ma >> 16 & 0xff) | ((mb >> 25 & 1) << 8),
            (ma >> 24 & 0xff) | ((mb >> 26 & 1) << 8),   mb >> 16 & 0x1ff};
    for (int b = 0; b < 6; b++) {
        if (!dm[b]) continue;
        int d = __builtin_ctz(dm[b]), half, k;
        Where(b, d, half, k);
        const __m256i *lv = half ? lvB : lvA;
        unsigned cs = sfront::Front::Byte(half ? cfg.b : cfg.a, k);
        int Bv = sfront::Front::Byte(_mm256_adds_epu8(_mm256_adds_epu8(half ? N.b : N.a,
                                                                       _mm256_shuffle_epi8(T.g_lut, half ? n.b : n.a)),
                                                      half ? rtB : rtA), k);
        int target_y = sfront::Front::Byte(target, 0);
        out_b = b;
        out_d = d;
        out_c = __builtin_ctz(cs);
        for (int c = 0; c < 6; c++) {
            if (!(cs >> c & 1)) continue;
            int l = sfront::Front::Byte(lv[c], k);
            if (std::max(1, (l + (255 - Bv) + 1) >> 1) == target_y) { out_c = c; break; }
        }
        return true;
    }
    return false;
}

// scalar mirror: key y (maximised) and best configuration of one digit
inline int KeyRef(const Tables &T, unsigned cs, const int kp[6][3], const int km[6][3], const int nj[3], int n,
                  int R, int &cbest) {
    int B = std::min(255, T.qn[nj[0]] + T.qn[nj[1]] + T.qn[nj[2]] + T.g8[n] + std::min(255, (R * T.rq) >> 4));
    int best = -1;
    for (int c = 0; c < 6; c++) {
        if (!(cs >> c & 1)) continue;
        int up = std::min(255, std::min(255, T.q[kp[c][0]] + T.fp[n] + T.q[kp[c][1]]) + T.q[kp[c][2]]);
        int um = std::min(255, std::min(255, T.q[km[c][0]] + T.fm[n] + T.q[km[c][1]]) + T.q[km[c][2]]);
        int mn = std::min(up, um), mx = std::max(up, um), dd = (mx - mn) >> 3;
        int L = std::max(0, mn - (dd < 16 ? T.corr[dd] : 0));
        int y = std::max(1, (L + (255 - B) + 1) >> 1);
        if (y > best) { best = y; cbest = c; }
    }
    return best;
}

}  // namespace slv2
