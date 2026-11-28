// DV2: the DV score (simd_dv.h) on packed vectors with 8-bit keys (see simd_lv2.h for the packing
// and the key y = max(1, avg(L, 255 - B)), maximised; no eps clamp).
#pragma once
#include "simd_dv.h"
#include "simd_lv2.h"

namespace sdv2 {

using slv::kE;

struct Params {
    double al = 1.1, g3 = -0.15, g4 = 0.1, gR = 0.015;
    int cpick = 1;
};

struct Tables {
    sdv::Tables dvt;
    slv2::Tables pk;  // packing controls, valid/rep tables (its score tables are not used)
    __m256i q_lut, qn_lut, corr_lut, g_lut, rq16;
    int cpick;
    explicit Tables(const Params &p)
        : dvt([&] { sdv::Params d; d.al = p.al; d.g3 = p.g3; d.g4 = p.g4; d.gR = p.gR; d.cpick = p.cpick; return d; }()),
          pk(slv2::Params{}) {
        q_lut = dvt.q_lut; qn_lut = dvt.qn_lut; corr_lut = dvt.corr_lut;
        g_lut = _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) dvt.g16));
        rq16 = _mm256_set1_epi16((short) dvt.rq);
        cpick = p.cpick;
    }
};

inline __m256i Half(const Tables &T, const __m256i E[3][3], const __m256i &cfg, __m256i U[6]) {
    const __m256i zero = _mm256_setzero_si256();
    __m256i q[3][3];
#pragma GCC unroll 3
    for (int p = 0; p < 3; p++)
#pragma GCC unroll 3
        for (int e = 0; e < 3; e++) q[p][e] = _mm256_shuffle_epi8(T.q_lut, sfront::Pop6(E[p][e]));
#pragma GCC unroll 6
    for (int c = 0; c < 6; c++) {
        __m256i u = _mm256_adds_epu8(_mm256_adds_epu8(q[0][kE[c][0]], q[1][kE[c][1]]), q[2][kE[c][2]]);
        U[c] = _mm256_or_si256(u, _mm256_cmpeq_epi8(_mm256_and_si256(cfg, T.pk.cbit[c]), zero));
    }
    return sdv::Lse(sdv::Lse(sdv::Lse(U[0], U[1], T.corr_lut), U[2], T.corr_lut),
                    sdv::Lse(sdv::Lse(U[3], U[5], T.corr_lut), U[4], T.corr_lut), T.corr_lut);
}

template <typename StateT>
inline bool Choose(const StateT &state, const Tables &T, int &out_b, int &out_d, int &out_c) {
    using slv2::Pack;
    using slv2::Packed;
    const slv2::Tables &P = T.pk;
    sfront::Front2 f;
    f.Compute(state.bands, state.boxen);
    Packed cfg = Pack(f.cfg[0], f.cfg[1], f.cfg[2], P.pack_ctrl);
    Packed n = Pack(f.n[0], f.n[1], f.n[2], P.pack_ctrl);
    __m256i validA = _mm256_shuffle_epi8(P.valid_lut, n.a), validB = _mm256_shuffle_epi8(P.valid_lut, n.b);
    if (__builtin_expect(_mm256_testz_si256(_mm256_or_si256(validA, validB), P.ff), 0)) return false;
    __m256i rtx[3];
    for (int x = 0; x < 3; x++) {
        __m256i sad = _mm256_sad_epu8(f.n[x], _mm256_setzero_si256());
        __m256i r = _mm256_add_epi64(sad, _mm256_bsrli_epi128(sad, 8));
        r = _mm256_srli_epi16(_mm256_mullo_epi16(r, T.rq16), 4);
        rtx[x] = _mm256_shuffle_epi8(r, P.rep9);
    }
    Packed rt = Pack(rtx[0], rtx[1], rtx[2], P.pack_ctrl);
    __m256i qn = _mm256_add_epi8(_mm256_add_epi8(_mm256_shuffle_epi8(T.qn_lut, f.n[0]),
                                                 _mm256_shuffle_epi8(T.qn_lut, f.n[1])),
                                 _mm256_shuffle_epi8(T.qn_lut, f.n[2]));
    __m256i nsw = sfront::Swap(qn);
    Packed N = Pack(nsw, nsw, nsw, P.pack_ctrl);
    __m256i EA[3][3], EB[3][3];
    for (int p = 0; p < 3; p++)
        for (int e = 0; e < 3; e++) {
            Packed pe = Pack(f.E[0][p][e], f.E[1][p][e], f.E[2][p][e], P.pack_ctrl);
            EA[p][e] = pe.a;
            EB[p][e] = pe.b;
        }
    __m256i uA[6], uB[6];
    __m256i lA = Half(T, EA, cfg.a, uA), lB = Half(T, EB, cfg.b, uB);
    auto key = [&](const __m256i &l, const __m256i &nn, const __m256i &NN, const __m256i &r, const __m256i &valid) {
        __m256i B = _mm256_adds_epu8(_mm256_adds_epu8(NN, _mm256_shuffle_epi8(T.g_lut, nn)), r);
        __m256i y = _mm256_max_epu8(_mm256_avg_epu8(l, _mm256_xor_si256(B, P.ff)), P.one);
        return _mm256_and_si256(y, valid);
    };
    __m256i yA = key(lA, n.a, N.a, rt.a, validA), yB = key(lB, n.b, N.b, rt.b, validB);
    __m256i m = _mm256_max_epu8(yA, yB);
    m = _mm256_max_epu8(m, sfront::Swap(m));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 8));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 4));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 2));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 1));
    __m256i target = _mm256_broadcastb_epi8(_mm256_castsi256_si128(m));
    uint32_t ma = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi8(yA, target));
    uint32_t mb = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi8(yB, target));
    uint32_t dm[6] = {
            (ma & 0xff) | ((mb >> 9 & 1) << 8),          (ma >> 8 & 0xff) | ((mb >> 10 & 1) << 8),
            mb & 0x1ff,                                  (ma >> 16 & 0xff) | ((mb >> 25 & 1) << 8),
            (ma >> 24 & 0xff) | ((mb >> 26 & 1) << 8),   mb >> 16 & 0x1ff};
    for (int b = 0; b < 6; b++) {
        if (!dm[b]) continue;
        int d = __builtin_ctz(dm[b]), half, k;
        slv2::Where(b, d, half, k);
        unsigned cs = sfront::Front::Byte(half ? cfg.b : cfg.a, k);
        out_b = b;
        out_d = d;
        out_c = __builtin_ctz(cs);
        if (T.cpick == 1 && __builtin_popcount(cs) > 2) {
            const __m256i *U = half ? uB : uA;
            int best = 256;
            for (int c = 0; c < 6; c++) {
                if (!(cs >> c & 1)) continue;
                int u = sfront::Front::Byte(U[c], k);
                if (u < best) { best = u; out_c = c; }
            }
        }
        return true;
    }
    return false;
}

// scalar mirror: key y (maximised) and configuration of one digit
inline int KeyRef(const Tables &T, unsigned cs, const int k[6][3], const int nj[3], int n, int R, int &cbest) {
    const sdv::Tables &t = T.dvt;
    int U[6];
    for (int c = 0; c < 6; c++)
        U[c] = !(cs >> c & 1) ? 255 : std::min(255, t.q[k[c][0]] + t.q[k[c][1]] + t.q[k[c][2]]);
    cbest = __builtin_ctz(cs);
    if (T.cpick == 1 && n > 2) {
        int best = 256;
        for (int c = 0; c < 6; c++) if ((cs >> c & 1) && U[c] < best) { best = U[c]; cbest = c; }
    }
    int L = sdv::LseRef(sdv::LseRef(sdv::LseRef(U[0], U[1], t), U[2], t),
                        sdv::LseRef(sdv::LseRef(U[3], U[5], t), U[4], t), t);
    int B = std::min(255, t.qn[nj[0]] + t.qn[nj[1]] + t.qn[nj[2]] + t.g16[n] + ((R * t.rq) >> 4));
    return std::max(1, (L + (255 - B) + 1) >> 1);
}

}  // namespace sdv2
