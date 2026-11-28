// SIMD front end shared by the branching-score candidates (AVX2 + GFNI).
//
// Layout: "digit per byte" as in ChooseIx: byte d (0..8) of a 128-bit lane holds a small set or
// count for digit d. For every box (i, j) -- peer j of horizontal band i, peer i of vertical
// band j -- it computes the crossing-compatible configuration sets of each element:
//   S_row_e = configurations of vertical band j compatible with the digit in row e of the box
//   S_col_e = configurations of horizontal band i compatible with the digit in column e
// and their sizes K (the k_j of the score for a + branch whose configuration uses element e).
// Vectors per box:  A = [row 0 | row 2]  (h_i)   B = [row 1 | col 1]   C = [col 0 | col 2]  (v_j)
#pragma once
#include <immintrin.h>

#include <cstdint>

namespace sfront {

struct Tables;
inline const Tables &T();

struct Tables {
    __m256i gf_cfg, gf_row02, gf_row1;  // GFNI transposition controls (as in ChooseIx)
    __m256i compat_rr[3];              // pshufb: element set A -> configs with element in peer p in A
    __m256i compat_pq[3][3];           // [compat[i] | compat[j]] for box (i, j)
    __m256i affx02, affy02, affx1, affy1;  // row sets -> column sets
    __m256i lo4, pc_lut, ident, hi2_aff;

    Tables() noexcept {
        ident = _mm256_set1_epi64x((long long) 0x8040201008040201ull);
        hi2_aff = _mm256_set1_epi64x((long long) (((uint64_t) 0x10 << 56) | ((uint64_t) 0x20 << 48)));
        auto make_gf = [](int first, int count) {
            alignas(32) int8_t b[32];
            for (int k = 0; k < 32; k++) b[k] = (int8_t) 0x80;
            for (int lane = 0; lane < 2; lane++)
                for (int e = 0; e < count; e++) {
                    b[lane * 16 + 7 - e] = (int8_t) (2 * (first + e));
                    b[lane * 16 + 15 - e] = (int8_t) (2 * (first + e) + 1);
                }
            return _mm256_load_si256((const __m256i *) b);
        };
        gf_cfg = make_gf(0, 6);
        gf_row02 = make_gf(0, 3);
        gf_row1 = make_gf(4, 3);
        const int kE[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}, {0, 2, 1}, {1, 0, 2}};
        alignas(32) uint8_t t[3][16];
        for (int p = 0; p < 3; p++) {
            uint8_t using_elem[3] = {0, 0, 0};
            for (int c = 0; c < 6; c++) using_elem[kE[c][p]] |= (uint8_t) (1u << c);
            for (int a = 0; a < 16; a++) {
                uint8_t m = 0;
                for (int e = 0; e < 3; e++) if ((a >> e) & 1) m |= using_elem[e];
                t[p][a] = a < 8 ? m : 0;
            }
            compat_rr[p] = _mm256_broadcastsi128_si256(_mm_load_si128((const __m128i *) t[p]));
        }
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                compat_pq[i][j] = _mm256_set_m128i(_mm_load_si128((const __m128i *) t[j]),
                                                   _mm_load_si128((const __m128i *) t[i]));
        // gf2p8affine: result bit k = parity(A.byte[7 - k] & x)
        auto ax = [](int e) {  // from X = R0 | R1 << 3: bit 0 <- R0 bit e, bit 1 <- R1 bit e
            return (long long) (((uint64_t) (1u << e) << 56) | ((uint64_t) (1u << (3 + e)) << 48));
        };
        auto ay = [](int e) { return (long long) ((uint64_t) (1u << e) << 40); };  // bit 2 <- R2 bit e
        affx02 = _mm256_setr_epi64x(ax(0), ax(0), ax(2), ax(2));
        affy02 = _mm256_setr_epi64x(ay(0), ay(0), ay(2), ay(2));
        affx1 = _mm256_set1_epi64x(ax(1));
        affy1 = _mm256_set1_epi64x(ay(1));
        lo4 = _mm256_set1_epi8(0x0f);
        pc_lut = _mm256_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
                                  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4);
    }
};

inline const Tables g_tables{};  // initialised at startup: no guard on the hot path
inline const Tables &T() { return g_tables; }

inline __m256i GfT(const __m256i &x, const __m256i &ctrl) {
    return _mm256_gf2p8affine_epi64_epi8(T().ident, _mm256_shuffle_epi8(x, ctrl), 0);
}

// popcount of bytes holding at most 6 bits: low nibble and (via a GF bit permutation) bits 4, 5
inline __m256i Pop6(const __m256i &x) {
    const Tables &t = T();
    __m256i lo = _mm256_shuffle_epi8(t.pc_lut, _mm256_and_si256(x, t.lo4));
    __m256i hi = _mm256_shuffle_epi8(t.pc_lut, _mm256_gf2p8affine_epi64_epi8(x, t.hi2_aff, 0));
    return _mm256_add_epi8(lo, hi);
}

inline __m256i Swap(const __m256i &x) { return _mm256_permute2x128_si256(x, x, 0x01); }
inline __m256i Lo2(const __m256i &x) { return _mm256_permute2x128_si256(x, x, 0x00); }
inline __m256i Hi2(const __m256i &x) { return _mm256_permute2x128_si256(x, x, 0x11); }

struct Front {
    __m256i cfg[3];  // [h_x | v_x] configuration sets per digit
    __m256i n[3];    // their sizes
    __m256i SA[9], SB[9], SC[9];  // compatible sets per box, layout above
    __m256i KA[9], KB[9], KC[9];  // their sizes

    template <typename BandsT, typename BoxenT>
    void Compute(const BandsT &bands, const BoxenT &boxen) {
        const Tables &t = T();
        for (int x = 0; x < 3; x++) {
            __m256i r = _mm256_set_m128i(bands[1][x].configurations.vec, bands[0][x].configurations.vec);
            cfg[x] = GfT(r, t.gf_cfg);
            n[x] = Pop6(cfg[x]);
        }
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                const int box = i * 3 + j;
                const __m256i cells = boxen[box].cells.vec;
                __m256i r02 = GfT(cells, t.gf_row02);  // [R0 | R2]: column sets of rows 0, 2
                __m256i r1 = GfT(cells, t.gf_row1);    // [R1 | junk]
                // column sets (row sets of each column)
                __m256i xx = Lo2(_mm256_or_si256(r02, _mm256_slli_epi16(r1, 3)));
                __m256i yy = Hi2(r02);
                __m256i c02 = _mm256_or_si256(_mm256_gf2p8affine_epi64_epi8(xx, t.affx02, 0),
                                              _mm256_gf2p8affine_epi64_epi8(yy, t.affy02, 0));
                __m256i c1 = _mm256_or_si256(_mm256_gf2p8affine_epi64_epi8(xx, t.affx1, 0),
                                             _mm256_gf2p8affine_epi64_epi8(yy, t.affy1, 0));
                __m256i vv = Hi2(cfg[j]);  // [v_j | v_j]
                __m256i hh = Lo2(cfg[i]);  // [h_i | h_i]
                __m256i vh = _mm256_permute2x128_si256(cfg[j], cfg[i], 0x21);  // [v_j | h_i]
                __m256i b_idx = _mm256_blend_epi32(r1, c1, 0xf0);               // [R1 | C1]
                SA[box] = _mm256_and_si256(vv, _mm256_shuffle_epi8(t.compat_rr[i], r02));
                SB[box] = _mm256_and_si256(vh, _mm256_shuffle_epi8(t.compat_pq[i][j], b_idx));
                SC[box] = _mm256_and_si256(hh, _mm256_shuffle_epi8(t.compat_rr[j], c02));
                KA[box] = Pop6(SA[box]);
                KB[box] = Pop6(SB[box]);
                KC[box] = Pop6(SC[box]);
            }
        }
    }

    // scalar accessors (testing): K for band b, peer p, element e, digit d; and n
    static uint8_t Byte(const __m256i &v, int k) {
        alignas(32) uint8_t a[32];
        _mm256_store_si256((__m256i *) a, v);
        return a[k];
    }
    int K(int b, int p, int e, int d) const {
        if (b < 3) {  // box (b, p)
            int box = b * 3 + p;
            if (e == 0) return Byte(KA[box], d);
            if (e == 2) return Byte(KA[box], 16 + d);
            return Byte(KB[box], d);
        }
        int box = p * 3 + (b - 3);
        if (e == 0) return Byte(KC[box], d);
        if (e == 2) return Byte(KC[box], 16 + d);
        return Byte(KB[box], 16 + d);
    }
    int N(int b, int d) const { return Byte(n[b % 3], (b < 3 ? 0 : 16) + d); }
    int Cfg(int b, int d) const { return Byte(cfg[b % 3], (b < 3 ? 0 : 16) + d); }
};


// Band-pair layout: E[x][p][e] = [h_x | v_x] compatible sets of element e at peer p, i.e.
//   low lane:  configurations of vertical band p compatible with the digit in row e of box (x, p)
//   high lane: configurations of horizontal band p compatible with the digit in column e of box (p, x)
// Row sets come from one GF transposition of the box; column sets from a vpermd that gathers the
// three rows of a column into one lane, then one GF transposition.
struct Tables2 {
    __m256i gf_row02, gf_c02, gf_r1c1;  // byte shuffles before the 8x8 bit transposition
    __m256i perm_c02, perm_r1c1;        // vpermd controls
    __m256i compat[3];                  // [compat_x | compat_x]
    __m256i lo4, pc_lut, hi2_aff;

    Tables2() noexcept {
        // shuffle control: lane l takes the low bytes of relative elements sel[l][0..2] to
        // qword bytes 7, 6, 5 and their high bytes to 15, 14, 13 (bit k = k-th element)
        auto make = [](const int sel[2][3]) {
            alignas(32) int8_t b[32];
            for (int k = 0; k < 32; k++) b[k] = (int8_t) 0x80;
            for (int lane = 0; lane < 2; lane++)
                for (int k = 0; k < 3; k++) {
                    b[lane * 16 + 7 - k] = (int8_t) (2 * sel[lane][k]);
                    b[lane * 16 + 15 - k] = (int8_t) (2 * sel[lane][k] + 1);
                }
            return _mm256_load_si256((const __m256i *) b);
        };
        const int row02[2][3] = {{0, 1, 2}, {0, 1, 2}};  // lane 0: elements 0..2, lane 1: 8..10
        gf_row02 = make(row02);
        // vpermd [d0 d2 d4 d7 | d1 d3 d5 d7]: lane 0 relative elements 0,2,4 = elements 0,4,8
        // (column 0); lane 1 relative 0,2,4 = elements 2,6,10 (column 2)
        perm_c02 = _mm256_setr_epi32(0, 2, 4, 7, 1, 3, 5, 7);
        const int c02[2][3] = {{0, 2, 4}, {0, 2, 4}};
        gf_c02 = make(c02);
        // vpermd [d2 d3 d7 d7 | d0 d2 d4 d7]: lane 0 relative 0,1,2 = elements 4,5,6 (row 1);
        // lane 1 relative 1,3,5 = elements 1,5,9 (column 1)
        perm_r1c1 = _mm256_setr_epi32(2, 3, 7, 7, 0, 2, 4, 7);
        const int r1c1[2][3] = {{0, 1, 2}, {1, 3, 5}};
        gf_r1c1 = make(r1c1);
        for (int x = 0; x < 3; x++) compat[x] = T().compat_rr[x];
        lo4 = _mm256_set1_epi8(0x0f);
        pc_lut = T().pc_lut;
        // bits 4, 5 -> bits 0, 1 (result bit k = parity(A.byte[7 - k] & x))
        hi2_aff = _mm256_set1_epi64x((long long) (((uint64_t) 0x10 << 56) | ((uint64_t) 0x20 << 48)));
    }
};

inline const Tables2 g_tables2{};

inline __m256i Pop6b(const __m256i &x) { return Pop6(x); }

struct Front2 {
    __m256i cfg[3], n[3];  // [h_x | v_x]
    __m256i E[3][3][3];    // [x][p][e]

    template <typename BandsT, typename BoxenT>
    void Compute(const BandsT &bands, const BoxenT &boxen) {
        const Tables &t = T();
        const Tables2 &t2 = g_tables2;
        for (int x = 0; x < 3; x++) {
            __m256i r = _mm256_set_m128i(bands[1][x].configurations.vec, bands[0][x].configurations.vec);
            cfg[x] = GfT(r, t.gf_cfg);
            n[x] = Pop6b(cfg[x]);
        }
        __m256i r02[9], c02[9], rc1[9];
        for (int box = 0; box < 9; box++) {
            const __m256i cells = boxen[box].cells.vec;
            r02[box] = GfT(cells, t2.gf_row02);                                      // [R0 | R2]
            c02[box] = GfT(_mm256_permutevar8x32_epi32(cells, t2.perm_c02), t2.gf_c02);  // [C0 | C2]
            rc1[box] = GfT(_mm256_permutevar8x32_epi32(cells, t2.perm_r1c1), t2.gf_r1c1);  // [R1 | C1]
        }
        __m256i mask[3];
        for (int p = 0; p < 3; p++) mask[p] = Swap(cfg[p]);  // [v_p | h_p]
        for (int x = 0; x < 3; x++)
            for (int p = 0; p < 3; p++) {
                const int hb = x * 3 + p, vb = p * 3 + x;  // box of h_x at peer p, of v_x at peer p
                __m256i i0 = _mm256_permute2x128_si256(r02[hb], c02[vb], 0x20);
                __m256i i2 = _mm256_permute2x128_si256(r02[hb], c02[vb], 0x31);
                __m256i i1 = _mm256_blend_epi32(rc1[hb], rc1[vb], 0xf0);
                E[x][p][0] = _mm256_and_si256(mask[p], _mm256_shuffle_epi8(t2.compat[x], i0));
                E[x][p][1] = _mm256_and_si256(mask[p], _mm256_shuffle_epi8(t2.compat[x], i1));
                E[x][p][2] = _mm256_and_si256(mask[p], _mm256_shuffle_epi8(t2.compat[x], i2));
            }
    }
};

}  // namespace sfront
