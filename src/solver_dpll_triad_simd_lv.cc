// Branch-volume branching heuristic for the Tdoku solver (opt-in: configuration 2 << 8).
//
// Like the intersection-aware heuristic (solver_dpll_triad_simd_ix.cc) this reuses the original
// solver's representation and propagation unchanged and only changes the decision: it splits a
// band-digit configuration set C (n = |C| >= 2) into {c} | C \ {c} for the (band, digit, c) that
// minimizes the estimated remaining search volume of the two branches,
//     s = ln(n^-be P+ + ((n-1)/n)^be P-) + g(n) + gR R,     P = prod_j (k_j / n_j)^al,
// where, for each of the three crossing bands j, n_j is the number of configurations of the digit
// in band j and k_j the number of those that stay locally compatible with the branch (they place
// the digit in the shared box at a cell that the branch's rows / columns allow; for the - branch
// the union of the rows of C \ {c}). R is the band's number of configurations, g(2) = 0.
// Parameters were fitted on end-to-end guess counts: al 1.1, be 0.6, g(3) -0.25, g(>=4) -0.1,
// gR 0.015 (natural log units).
//
// The score is computed for all 6 x 9 x 6 candidates at once in 8-bit log arithmetic (1/24 bit):
// crossing counts are table lookups of popcounts in a "digit per byte" layout, products are
// saturating sums, sums of volumes a log-sum-exp with a 16-entry correction table. The six bands'
// digits are packed into two AVX2 vectors. Ties go to the lowest band, digit and configuration.
// Requires AVX2 (GFNI is used when available); otherwise the original heuristic is used.

#define TDOKU_HEURISTIC_TU
#include "solver_dpll_triad_simd.cc"


namespace {

#ifdef __AVX2__
namespace volume {

// element (row of the box for horizontal bands, column for vertical) of configuration c in peer p
constexpr int kElem[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}, {0, 2, 1}, {1, 0, 2}};
// the other configuration with the same element as c in peer p
constexpr int kPartner[6][3] = {{4, 3, 5}, {5, 4, 3}, {3, 5, 4}, {2, 0, 1}, {0, 1, 2}, {1, 2, 0}};

// 8-bit tables (unit 1/24 bit; see the derivation above):
//   Q[k] = round(24 al log2(6 / k)), Q[0] = 255 ("volume 0"); used for k (with the + / - sets)
//   and n (normalisation, Q[n] summed over the crossing bands)
//   corr[i] = round(24 log2(1 + 2^-((8 i + 3.5) / 24)))   (log-sum-exp correction, D = 8 i..8 i+7)
//   fp[n] = round(24 be log2 n), fm[n] = round(24 be log2(n / (n - 1)))
//   g[n] = round(24 / ln 2 * (g(n) - min g)),  band term (R * 8) >> 4 = round(24 / ln 2 * gR * 16) / 16
alignas(16) constexpr uint8_t kQ[16] = {255, 68, 42, 26, 15, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
alignas(16) constexpr uint8_t kQn[16] = {0, 68, 42, 26, 15, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
alignas(16) constexpr uint8_t kCorr[16] = {22, 19, 16, 13, 11, 9, 7, 6, 5, 4, 3, 2, 2, 2, 1, 1};
alignas(16) constexpr uint8_t kFp[16] = {0, 0, 14, 23, 29, 33, 37, 0, 0, 0, 0, 0, 0, 0, 0, 0};
alignas(16) constexpr uint8_t kFm[16] = {0, 0, 14, 8, 6, 5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0};
alignas(16) constexpr uint8_t kG[16] = {0, 0, 9, 0, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0};
alignas(16) constexpr uint8_t kValid[16] = {0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0};
alignas(16) constexpr uint8_t kPopcount[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
constexpr int kBandMul = 8;

struct Tables {
    __m256i q, qn, corr, fp, fm, g, valid, popcount, lo4, ff, one;
    __m256i cbit[6], ubit[3][3];  // set1(1 << c); configurations using element e of peer p
    __m256i compat[3];            // pshufb: element set A -> configurations with element in peer x in A
    __m256i perm_c02, perm_r1c1;  // vpermd: gather a box's columns 0 / 2, row 1 / column 1 per lane
    __m256i pack_ctrl, rep9, band_mul;
#ifdef __GFNI__
    __m256i ident, hi2, lse_idx;
    __m256i tr_cfg, tr_row02, tr_c02, tr_r1c1;  // byte shuffles before the 8x8 bit transposition
#else
    __m256i bitsel, x1f, x70;
    __m256i ex_cfg[6], ex_row02[3], ex_c02[3], ex_r1c1[3];  // per element: broadcast its bytes
#endif

    static __m256i Bcast(const uint8_t *t) {
        return _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) t));
    }
#ifdef __GFNI__
    // lane l: the low bytes of relative elements sel[l][k] to qword byte 7 - k, high bytes to 15 - k
    static __m256i TransposeCtrl(const int sel[2][6], int count) {
        alignas(32) int8_t b[32];
        for (int k = 0; k < 32; k++) b[k] = (int8_t) 0x80;
        for (int lane = 0; lane < 2; lane++)
            for (int k = 0; k < count; k++) {
                b[lane * 16 + 7 - k] = (int8_t) (2 * sel[lane][k]);
                b[lane * 16 + 15 - k] = (int8_t) (2 * sel[lane][k] + 1);
            }
        return _mm256_load_si256((const __m256i *) b);
    }
#else
    // lane l: element sel[l][k]'s low byte to bytes 0..7, high byte to 8..15
    static void ExpandCtrls(const int sel[2][6], int count, __m256i *out) {
        for (int k = 0; k < count; k++) {
            alignas(32) int8_t b[32];
            for (int lane = 0; lane < 2; lane++)
                for (int i = 0; i < 16; i++) b[lane * 16 + i] = (int8_t) (2 * sel[lane][k] + (i >= 8));
            out[k] = _mm256_load_si256((const __m256i *) b);
        }
    }
#endif

    Tables() noexcept {
        q = Bcast(kQ); qn = Bcast(kQn); corr = Bcast(kCorr); fp = Bcast(kFp); fm = Bcast(kFm);
        g = Bcast(kG); valid = Bcast(kValid); popcount = Bcast(kPopcount);
        lo4 = _mm256_set1_epi8(0x0f);
        ff = _mm256_set1_epi8(-1);
        one = _mm256_set1_epi8(1);
        for (int c = 0; c < 6; c++) cbit[c] = _mm256_set1_epi8((char) (1 << c));
        for (int p = 0; p < 3; p++) {
            uint8_t using_elem[3] = {0, 0, 0};
            for (int c = 0; c < 6; c++) using_elem[kElem[c][p]] |= (uint8_t) (1u << c);
            for (int e = 0; e < 3; e++) ubit[p][e] = _mm256_set1_epi8((char) using_elem[e]);
            alignas(16) uint8_t t[16] = {0};
            for (int a = 0; a < 8; a++)
                for (int e = 0; e < 3; e++) if ((a >> e) & 1) t[a] |= using_elem[e];
            compat[p] = Bcast(t);
        }
        // cells are 16 16-bit elements (row r, column c at 4 r + c); dword k = elements 2k, 2k+1
        perm_c02 = _mm256_setr_epi32(0, 2, 4, 7, 1, 3, 5, 7);   // lane 0: col 0 at 0,2,4; lane 1: col 2
        perm_r1c1 = _mm256_setr_epi32(2, 3, 7, 7, 0, 2, 4, 7);  // lane 0: row 1 at 0,1,2; lane 1: col 1 at 1,3,5
        alignas(16) int8_t pc[16];
        for (int k = 0; k < 16; k++) pc[k] = (int8_t) 0x80;
        pc[9] = 0;
        pc[10] = 8;
        pack_ctrl = Bcast((const uint8_t *) pc);
        for (int k = 0; k < 16; k++) pc[k] = k <= 8 ? 0 : (int8_t) 0x80;
        rep9 = Bcast((const uint8_t *) pc);
        band_mul = _mm256_set1_epi16(kBandMul);
        const int sel_cfg[2][6] = {{0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 4, 5}};
        const int sel_row02[2][6] = {{0, 1, 2}, {0, 1, 2}};
        const int sel_c02[2][6] = {{0, 2, 4}, {0, 2, 4}};
        const int sel_r1c1[2][6] = {{0, 1, 2}, {1, 3, 5}};
#ifdef __GFNI__
        ident = _mm256_set1_epi64x((long long) 0x8040201008040201ull);
        // gf2p8affine: result bit k = parity(A.byte[7 - k] & x)
        hi2 = _mm256_set1_epi64x((long long) (((uint64_t) 0x10 << 56) | ((uint64_t) 0x20 << 48)));
        lse_idx = _mm256_set1_epi64x((long long) (((uint64_t) 0x08 << 56) | ((uint64_t) 0x10 << 48) |
                                                  ((uint64_t) 0x20 << 40) | ((uint64_t) 0x40 << 32) | 0x80));
        tr_cfg = TransposeCtrl(sel_cfg, 6);
        tr_row02 = TransposeCtrl(sel_row02, 3);
        tr_c02 = TransposeCtrl(sel_c02, 3);
        tr_r1c1 = TransposeCtrl(sel_r1c1, 3);
#else
        bitsel = _mm256_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128,
                                  1, 2, 4, 8, 16, 32, 64, -128, 1, 2, 4, 8, 16, 32, 64, -128);
        x1f = _mm256_set1_epi8(0x1f);
        x70 = _mm256_set1_epi8(0x70);
        ExpandCtrls(sel_cfg, 6, ex_cfg);
        ExpandCtrls(sel_row02, 3, ex_row02);
        ExpandCtrls(sel_c02, 3, ex_c02);
        ExpandCtrls(sel_r1c1, 3, ex_r1c1);
#endif
    }
};

const Tables tables_v{};

inline __m256i Swap(const __m256i &x) { return _mm256_permute2x128_si256(x, x, 0x01); }

// byte d (digit) of each lane: bit k set iff digit d is in the k-th selected element of the lane
template <int count>
inline __m256i Transpose(const __m256i &x, const __m256i &gf_ctrl, const __m256i *expand) {
#ifdef __GFNI__
    (void) expand;
    return _mm256_gf2p8affine_epi64_epi8(tables_v.ident, _mm256_shuffle_epi8(x, gf_ctrl), 0);
#else
    (void) gf_ctrl;
    __m256i acc = _mm256_setzero_si256();
    for (int k = 0; k < count; k++) {
        __m256i sel = _mm256_and_si256(_mm256_shuffle_epi8(x, expand[k]), tables_v.bitsel);
        acc = _mm256_or_si256(acc, _mm256_and_si256(_mm256_cmpeq_epi8(sel, tables_v.bitsel),
                                                     _mm256_set1_epi8((char) (1 << k))));
    }
    return acc;
#endif
}

// popcount of bytes holding at most 6 bits
inline __m256i Pop6(const __m256i &x) {
    const Tables &t = tables_v;
    __m256i lo = _mm256_shuffle_epi8(t.popcount, _mm256_and_si256(x, t.lo4));
#ifdef __GFNI__
    __m256i hi = _mm256_shuffle_epi8(t.popcount, _mm256_gf2p8affine_epi64_epi8(x, t.hi2, 0));
#else
    __m256i hi = _mm256_shuffle_epi8(t.popcount, _mm256_srli_epi16(_mm256_andnot_si256(t.lo4, x), 4));
#endif
    return _mm256_add_epi8(lo, hi);
}

// log-sum-exp in "minus log" units: min(a, b) - corr(|a - b| >> 3), no correction for D >= 128
inline __m256i Lse(const __m256i &a, const __m256i &b) {
    const Tables &t = tables_v;
    __m256i mn = _mm256_min_epu8(a, b), mx = _mm256_max_epu8(a, b), d = _mm256_sub_epi8(mx, mn);
#ifdef __GFNI__
    __m256i idx = _mm256_gf2p8affine_epi64_epi8(d, t.lse_idx, 0);
#else
    __m256i idx = _mm256_adds_epu8(_mm256_and_si256(_mm256_srli_epi16(d, 3), t.x1f), t.x70);
#endif
    return _mm256_subs_epu8(mn, _mm256_shuffle_epi8(t.corr, idx));
}

struct Packed {
    __m256i a, b;
};

// band-pair vectors [h_x | v_x] use bytes 0..8 of each lane; three of them are packed into two:
// a = [x0 digits 0..7 | x1 digits 0..7], b = [x2 digits 0..8 | x0 digit 8 | x1 digit 8 | 0..]
inline Packed Pack(const __m256i &x0, const __m256i &x1, const __m256i &x2) {
    return {_mm256_unpacklo_epi64(x0, x1),
            _mm256_or_si256(x2, _mm256_shuffle_epi8(_mm256_unpackhi_epi64(x0, x1), tables_v.pack_ctrl))};
}

// byte of (band b, digit d) in the packed layout
inline void Where(int b, int d, int &half, int &k) {
    int x = b % 3, lane = b < 3 ? 0 : 16;
    if (x == 2) { half = 1; k = lane + d; return; }
    if (d == 8) { half = 1; k = lane + 9 + x; return; }
    half = 0;
    k = lane + 8 * x + d;
}

inline int ByteOf(const __m256i &v, int k) {
    alignas(32) uint8_t a[32];
    _mm256_store_si256((__m256i *) a, v);
    return a[k];
}

// one packed half: per byte the best branch LSE over the digit's configurations, and per
// configuration its value (for the final choice of the configuration)
inline __m256i Half(const __m256i E[3][3], const __m256i &cfg, const __m256i &n, __m256i lv[6]) {
    const Tables &t = tables_v;
    const __m256i zero = _mm256_setzero_si256();
    __m256i member[6], nonmember[6];
    for (int c = 0; c < 6; c++) {
        nonmember[c] = _mm256_cmpeq_epi8(_mm256_and_si256(cfg, t.cbit[c]), zero);
        member[c] = _mm256_xor_si256(nonmember[c], t.ff);
    }
    __m256i fp = _mm256_shuffle_epi8(t.fp, n), fm = _mm256_shuffle_epi8(t.fm, n);
    // per peer: Q of the + sets (one element) and of the - sets (unions over the used elements:
    // X_e = all used elements but e, T = all used elements)
    __m256i qp[3][3], qx[3][3], qt[3];
#pragma GCC unroll 3
    for (int p = 0; p < 3; p++) {
        __m256i m[3];
#pragma GCC unroll 3
        for (int e = 0; e < 3; e++) {
            qp[p][e] = _mm256_shuffle_epi8(t.q, Pop6(E[p][e]));
            if (p == 0) qp[p][e] = _mm256_adds_epu8(qp[p][e], fp);  // own-band fraction, folded in
            m[e] = _mm256_andnot_si256(_mm256_cmpeq_epi8(_mm256_and_si256(cfg, t.ubit[p][e]), zero), E[p][e]);
        }
        __m256i x0 = _mm256_or_si256(m[1], m[2]), x1 = _mm256_or_si256(m[0], m[2]);
        __m256i x2 = _mm256_or_si256(m[0], m[1]), tt = _mm256_or_si256(x0, m[0]);
        qx[p][0] = _mm256_shuffle_epi8(t.q, Pop6(x0));
        qx[p][1] = _mm256_shuffle_epi8(t.q, Pop6(x1));
        qx[p][2] = _mm256_shuffle_epi8(t.q, Pop6(x2));
        qt[p] = _mm256_shuffle_epi8(t.q, Pop6(tt));
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
            int e = kElem[c][p];
            // - branch: |T| if c's partner in peer p is in C (it keeps element e used), else |X_e|;
            // Q(T) <= Q(X_e), so the choice is a min with Q(T) masked off when the partner is absent
            __m256i qm = _mm256_min_epu8(qx[p][e], _mm256_or_si256(qt[p], nonmember[kPartner[c][p]]));
            up = p == 0 ? qp[p][e] : _mm256_adds_epu8(up, qp[p][e]);
            um = p == 0 ? qm : _mm256_adds_epu8(um, qm);
        }
        __m256i l = _mm256_and_si256(Lse(up, um), member[c]);
        lv[c] = l;
        lmax = _mm256_max_epu8(lmax, l);
    }
    return lmax;
}

// false if every band is fixed
inline bool Choose(const State &state, int &out_b, int &out_d, int &out_c) {
    const Tables &t = tables_v;
    // configuration sets and sizes per digit, [h_x | v_x]
    __m256i cfg[3], n[3];
    for (int x = 0; x < 3; x++) {
        __m256i r = _mm256_set_m128i(state.bands[1][x].configurations.vec, state.bands[0][x].configurations.vec);
#ifdef __GFNI__
        cfg[x] = Transpose<6>(r, t.tr_cfg, nullptr);
#else
        cfg[x] = Transpose<6>(r, r, t.ex_cfg);
#endif
        n[x] = Pop6(cfg[x]);
    }
    Packed cfgp = Pack(cfg[0], cfg[1], cfg[2]), np = Pack(n[0], n[1], n[2]);
    __m256i valid_a = _mm256_shuffle_epi8(t.valid, np.a), valid_b = _mm256_shuffle_epi8(t.valid, np.b);
    if (__builtin_expect(_mm256_testz_si256(_mm256_or_si256(valid_a, valid_b), t.ff), 0)) return false;

    // per box: row sets of the columns [R0 | R2], [R1 | C1] and column sets [C0 | C2] (byte d = set of
    // positions holding digit d); then the crossing-compatible sets E[x][p][e] of band pair x at peer
    // p, element e: [configs of v_p compatible with row e of box (x, p) | of h_p with column e of (p, x)]
    __m256i r02[9], c02[9], rc1[9];
    for (int box = 0; box < 9; box++) {
        const __m256i cells = state.boxen[box].cells.vec;
        __m256i pc02 = _mm256_permutevar8x32_epi32(cells, t.perm_c02);
        __m256i prc1 = _mm256_permutevar8x32_epi32(cells, t.perm_r1c1);
#ifdef __GFNI__
        r02[box] = Transpose<3>(cells, t.tr_row02, nullptr);
        c02[box] = Transpose<3>(pc02, t.tr_c02, nullptr);
        rc1[box] = Transpose<3>(prc1, t.tr_r1c1, nullptr);
#else
        r02[box] = Transpose<3>(cells, cells, t.ex_row02);
        c02[box] = Transpose<3>(pc02, pc02, t.ex_c02);
        rc1[box] = Transpose<3>(prc1, prc1, t.ex_r1c1);
#endif
    }
    __m256i E[3][3][3];
    for (int p = 0; p < 3; p++) {
        __m256i mask = Swap(cfg[p]);  // [v_p | h_p]
        for (int x = 0; x < 3; x++) {
            const int hb = x * 3 + p, vb = p * 3 + x;
            __m256i i0 = _mm256_permute2x128_si256(r02[hb], c02[vb], 0x20);
            __m256i i2 = _mm256_permute2x128_si256(r02[hb], c02[vb], 0x31);
            __m256i i1 = _mm256_blend_epi32(rc1[hb], rc1[vb], 0xf0);
            E[x][p][0] = _mm256_and_si256(mask, _mm256_shuffle_epi8(t.compat[x], i0));
            E[x][p][1] = _mm256_and_si256(mask, _mm256_shuffle_epi8(t.compat[x], i1));
            E[x][p][2] = _mm256_and_si256(mask, _mm256_shuffle_epi8(t.compat[x], i2));
        }
    }
    // band term (R * kBandMul) >> 4 per byte, R = sum of the band's n
    __m256i rtx[3];
    for (int x = 0; x < 3; x++) {
        __m256i sad = _mm256_sad_epu8(n[x], _mm256_setzero_si256());
        __m256i r = _mm256_add_epi64(sad, _mm256_bsrli_epi128(sad, 8));
        rtx[x] = _mm256_shuffle_epi8(_mm256_srli_epi16(_mm256_mullo_epi16(r, t.band_mul), 4), t.rep9);
    }
    Packed rt = Pack(rtx[0], rtx[1], rtx[2]);
    // crossing normalisation sum_j Q[n_j]: the same for all bands of an orientation
    __m256i qn = _mm256_add_epi8(_mm256_add_epi8(_mm256_shuffle_epi8(t.qn, n[0]), _mm256_shuffle_epi8(t.qn, n[1])),
                                 _mm256_shuffle_epi8(t.qn, n[2]));
    __m256i nsw = Swap(qn);
    Packed N = Pack(nsw, nsw, nsw);
    __m256i EA[3][3], EB[3][3];
    for (int p = 0; p < 3; p++)
        for (int e = 0; e < 3; e++) {
            Packed pe = Pack(E[0][p][e], E[1][p][e], E[2][p][e]);
            EA[p][e] = pe.a;
            EB[p][e] = pe.b;
        }
    __m256i lva[6], lvb[6];
    __m256i la = Half(EA, cfgp.a, np.a, lva), lb = Half(EB, cfgp.b, np.b, lvb);

    // key (maximised): max(1, avg(L, 255 - B)), B = normalisation + g(n) + band term
    __m256i ba = _mm256_adds_epu8(_mm256_adds_epu8(N.a, _mm256_shuffle_epi8(t.g, np.a)), rt.a);
    __m256i bb = _mm256_adds_epu8(_mm256_adds_epu8(N.b, _mm256_shuffle_epi8(t.g, np.b)), rt.b);
    __m256i ya = _mm256_and_si256(_mm256_max_epu8(_mm256_avg_epu8(la, _mm256_xor_si256(ba, t.ff)), t.one), valid_a);
    __m256i yb = _mm256_and_si256(_mm256_max_epu8(_mm256_avg_epu8(lb, _mm256_xor_si256(bb, t.ff)), t.one), valid_b);
    __m256i m = _mm256_max_epu8(ya, yb);
    m = _mm256_max_epu8(m, Swap(m));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 8));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 4));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 2));
    m = _mm256_max_epu8(m, _mm256_bsrli_epi128(m, 1));
    __m256i target = _mm256_broadcastb_epi8(_mm256_castsi256_si128(m));
    uint32_t ma = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi8(ya, target));
    uint32_t mb = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi8(yb, target));
    const uint32_t dm[6] = {(ma & 0xff) | ((mb >> 9 & 1) << 8), (ma >> 8 & 0xff) | ((mb >> 10 & 1) << 8),
                            mb & 0x1ff, (ma >> 16 & 0xff) | ((mb >> 25 & 1) << 8),
                            (ma >> 24 & 0xff) | ((mb >> 26 & 1) << 8), mb >> 16 & 0x1ff};
    for (int b = 0; b < 6; b++) {
        if (!dm[b]) continue;
        int d = __builtin_ctz(dm[b]), half, k;
        Where(b, d, half, k);
        unsigned cs = (unsigned) ByteOf(half ? cfgp.b : cfgp.a, k);
        int bv = ByteOf(half ? bb : ba, k), y = ByteOf(target, 0);
        const __m256i *lv = half ? lvb : lva;
        out_b = b;
        out_d = d;
        out_c = __builtin_ctz(cs);
        for (int c = 0; c < 6; c++) {  // the lowest configuration reaching the key
            if (!(cs >> c & 1)) continue;
            int key = (ByteOf(lv[c], k) + (255 - bv) + 1) >> 1;
            if ((key > 1 ? key : 1) == y) { out_c = c; break; }
        }
        return true;
    }
    return false;  // unreachable
}

}  // namespace volume
#endif  // __AVX2__

template<int solution_mode>
struct SolverVolume {
    using Base = SolverDpllTriadSimd<solution_mode>;
    static constexpr uint32_t NONE = UINT32_MAX;

    State solution_{};
    size_t limit_ = 1;
    size_t num_solutions_ = 0;
    size_t num_guesses_ = 0;

    static const Cells08 &ConfigLaneMask(int config) {
        static const Cells08 masks[6]{
                {0xffff, 0, 0, 0, 0, 0, 0, 0}, {0, 0xffff, 0, 0, 0, 0, 0, 0},
                {0, 0, 0xffff, 0, 0, 0, 0, 0}, {0, 0, 0, 0xffff, 0, 0, 0, 0},
                {0, 0, 0, 0, 0xffff, 0, 0, 0}, {0, 0, 0, 0, 0, 0xffff, 0, 0}};
        return masks[config];
    }

    void Solution(State &state) {
        num_solutions_++;
        if (solution_mode == 1 && num_solutions_ == limit_) solution_ = state;
    }

    void CountSolutions(State &state) {
#ifdef __AVX2__
        int band, digit, config;
        if (!volume::Choose(state, band, digit, config)) {
            Solution(state);
            return;
        }
        if (band < 3) {
            BranchOnConfig<0>(band, digit, config, state);
        } else {
            BranchOnConfig<1>(band - 3, digit, config, state);
        }
#else
        auto band_and_value = Base::ChooseBandAndValueToBranch(state);
        if (band_and_value.first == NONE) {
            Solution(state);
            return;
        }
        if (band_and_value.first < 3) {
            BranchOnBandAndValue<0>(tables.mod3[band_and_value.first], band_and_value.second, state);
        } else {
            BranchOnBandAndValue<1>(tables.mod3[band_and_value.first], band_and_value.second, state);
        }
#endif
    }

    // the original's branching on the lowest configuration of a digit
    template<int vertical>
    void BranchOnBandAndValue(int band_idx, const Cells08 &value_mask, State &state) {
        Band &band = state.bands[vertical][band_idx];
        Cells08 value_configurations = band.configurations & value_mask;
        num_guesses_++;
        State state_copy = state;
        Cells08 assignment_elims = value_configurations.ClearLowBit();
        state_copy.bands[vertical][band_idx].eliminations |= assignment_elims;
        if (Base::template BandEliminate<vertical>(state_copy, band_idx)) {
            CountSolutions(state_copy);
            if (num_solutions_ == limit_) return;
        }
        Cells08 negation_elims = value_configurations ^ assignment_elims;
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (Base::template BandEliminate<vertical>(state, band_idx)) {
            CountSolutions(state);
        }
    }

    // branch on asserting vs. eliminating configuration `config` of `digit` in the band
    template<int vertical>
    void BranchOnConfig(int band_idx, int digit, int config, State &state) {
        Band &band = state.bands[vertical][band_idx];
        Cells08 value_configurations = band.configurations & tables.one_value_mask[digit];
        num_guesses_++;
        State state_copy = state;
        Cells08 assignment_elims = value_configurations.and_not(ConfigLaneMask(config));
        state_copy.bands[vertical][band_idx].eliminations |= assignment_elims;
        if (Base::template BandEliminate<vertical>(state_copy, band_idx)) {
            CountSolutions(state_copy);
            if (num_solutions_ == limit_) return;
        }
        Cells08 negation_elims = value_configurations & ConfigLaneMask(config);
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (Base::template BandEliminate<vertical>(state, band_idx)) {
            CountSolutions(state);
        }
    }

    size_t SolveSudoku(const char *input, size_t limit, char *solution, size_t *num_guesses) {
        limit_ = limit;
        num_solutions_ = 0;
        num_guesses_ = 0;
        bool pencilmark = input[81] >= '.';
        State state;
        if (pencilmark ? Base::InitPencilmarkByBox(input, state)
                       : Base::InitVanillaByBand(input, state)) {
            CountSolutions(state);
            if (solution_mode == 1) Base::ExtractSolution(solution_, solution);
        }
        *num_guesses = num_guesses_;
        return num_solutions_;
    }
};

SolverVolume<0> solver_volume_none{};
SolverVolume<1> solver_volume_last{};

} // namespace

size_t TdokuSolverVolume(const char *puzzle, size_t limit, bool return_last,
                         char *solution, size_t *num_guesses) {
    return return_last ? solver_volume_last.SolveSudoku(puzzle, limit, solution, num_guesses)
                       : solver_volume_none.SolveSudoku(puzzle, limit, solution, num_guesses);
}
