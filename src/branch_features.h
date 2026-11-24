#ifndef TDOKU_BRANCH_FEATURES_H
#define TDOKU_BRANCH_FEATURES_H

// Intersection-aware branching features for Tdoku (research code).
//
// This header contains *pure* functions over a plain-array view of the solver state so that
// the compatibility primitive can be unit tested independently of the SIMD solver. See
// docs/BRANCHING_RESEARCH.md for the derivation.
//
// Geometry (verified against solver_dpll_triad_simd.cc):
//  * Band b in 0..5: b = 3 * vertical + idx. Horizontal band i = box row i (boxes 3i..3i+2);
//    vertical band j = box column j (boxes j, j+3, j+6).
//  * In horizontal band i the box (i, j) is "peer" j; in vertical band j the box (i, j) is
//    peer i. So the shared box of bands (H_i, V_j) is peer j of H_i and peer i of V_j.
//  * band configurations: lane c (0..5) of Band::configurations holds a 9-bit set of digits for
//    which configuration c is still possible.
//  * kElem[c][p]: the element (row-within-box for horizontal bands, column-within-box for
//    vertical bands) where configuration c places the digit in peer p. This is the table in
//    the comment above Band in solver_dpll_triad_simd.cc and is consistent with both
//    Tables::peer_x_elem_to_config_mask and Tables::shuffle_configs_to_triads.
//  * Box cells: lane r * 4 + c of Box::cells holds the 9-bit candidate set of the cell at
//    row r, column c within the box (lanes 3,7,11 and 12..14 hold negative triads).

#include <cstdint>

namespace tdoku_ix {

constexpr int kElem[6][3] = {
        {0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}, {0, 2, 1}, {1, 0, 2}};

// kCompat.t[p][A]: 6-bit mask of configurations c with kElem[c][p] in the 3-bit element set A.
// i.e., the configurations of a band that remain compatible when, in its peer p, the digit is
// restricted to the elements in A.
struct CompatTable {
    uint8_t t[3][8];
};

constexpr CompatTable MakeCompatTable() {
    CompatTable table{};
    for (int p = 0; p < 3; p++) {
        for (int a = 0; a < 8; a++) {
            uint8_t mask = 0;
            for (int c = 0; c < 6; c++) {
                if ((a >> kElem[c][p]) & 1) mask |= (uint8_t) (1u << c);
            }
            table.t[p][a] = mask;
        }
    }
    return table;
}

constexpr CompatTable kCompat = MakeCompatTable();

// kElemMask[c]: for configuration c, kElemMask[c][p] = 1 << kElem[c][p].
// kConfigsUsingElem[p][e]: 6-bit mask of configurations c with kElem[c][p] == e.
struct ElemTables {
    uint8_t configs_using_elem[3][3];
};

constexpr ElemTables MakeElemTables() {
    ElemTables tables{};
    for (int c = 0; c < 6; c++) {
        for (int p = 0; p < 3; p++) {
            tables.configs_using_elem[p][kElem[c][p]] |= (uint8_t) (1u << c);
        }
    }
    return tables;
}

constexpr ElemTables kElemTables = MakeElemTables();

// A plain-array copy of the parts of the solver state used by the features.
struct RawState {
    alignas(32) uint16_t box[9][16];  // box[b][lane]: candidate digits (bit d = digit d+1)
    alignas(16) uint16_t band[6][8];  // band[b][c]: digits for which configuration c is possible
};

inline int Popcount8(uint32_t x) { return __builtin_popcount(x); }

// 6-bit mask of remaining configurations of digit d in band b.
inline uint32_t ConfigMask(const RawState &s, int b, int d) {
    uint32_t mask = 0;
    for (int c = 0; c < 6; c++) mask |= ((s.band[b][c] >> d) & 1u) << c;
    return mask;
}

inline int OppositeBand(int b, int y) { return (b < 3 ? 3 : 0) + y; }

inline int SharedBox(int b, int y) {
    int x = b % 3;
    return b < 3 ? x * 3 + y : y * 3 + x;
}

// 3-bit set of elements of the *opposite* orientation (columns if b is horizontal, rows if b is
// vertical) where digit d is a candidate within element e of b's orientation (row e if b is
// horizontal, column e if b is vertical) of the box shared by band b and opposite band y.
inline uint32_t LocalMask(const RawState &s, int b, int y, int e, int d) {
    int box = SharedBox(b, y);
    uint32_t a = 0;
    for (int t = 0; t < 3; t++) {
        int lane = b < 3 ? e * 4 + t : t * 4 + e;
        a |= ((s.box[box][lane] >> d) & 1u) << t;
    }
    return a;
}

// Fast, table-driven compatibility masks.
//   M[y][e] = C_{opp(b,y),d} & kCompat.t[b%3][A(y,e,d)]
// is the set of configurations of the opposite band y (for digit d) that are locally compatible
// with placing d at element e of the shared box. K[y][e] = popcount(M[y][e]).
inline void CompatMasks(const RawState &s, int b, int d, uint8_t M[3][3]) {
    int x = b % 3;
    for (int y = 0; y < 3; y++) {
        uint32_t opp_cfg = ConfigMask(s, OppositeBand(b, y), d);
        for (int e = 0; e < 3; e++) {
            M[y][e] = (uint8_t) (opp_cfg & kCompat.t[x][LocalMask(s, b, y, e, d)]);
        }
    }
}

// Explicit reference implementation of k_y(pi): enumerate configurations sigma of the opposite
// band y and test directly whether the cell at which pi and sigma would both place digit d in the
// shared box still has d as a candidate.
inline int CompatCountReference(const RawState &s, int b, int d, int pi, int y) {
    int x = b % 3;
    int opp = OppositeBand(b, y);
    int box = SharedBox(b, y);
    int count = 0;
    for (int sigma = 0; sigma < 6; sigma++) {
        if (!((s.band[opp][sigma] >> d) & 1)) continue;
        int pi_elem = kElem[pi][y];       // b's element in peer y (= shared box)
        int sigma_elem = kElem[sigma][x]; // opposite band's element in its peer x (= shared box)
        int row = b < 3 ? pi_elem : sigma_elem;
        int col = b < 3 ? sigma_elem : pi_elem;
        if ((s.box[box][row * 4 + col] >> d) & 1) count++;
    }
    return count;
}

// Negative-branch estimate: configurations of opposite band y compatible with at least one of
// the elements still used in peer y by the configurations in `rest` (a 6-bit config mask).
inline uint32_t NegativeCompatMask(const uint8_t M[3][3], int y, uint32_t rest) {
    uint32_t mask = 0;
    for (int e = 0; e < 3; e++) {
        if (rest & kElemTables.configs_using_elem[y][e]) mask |= M[y][e];
    }
    return mask;
}

}  // namespace tdoku_ix

// Runtime parameters for the research (scalar, parameterized) selector.
// Only used when the solver is invoked with heuristic id kHeuristicResearch.
struct TdokuResearchParams {
    // how to rank band-digit pairs (lower key is better; ties form the candidate class)
    //   0 NONE          all eligible pairs form one class
    //   1 ORIGINAL      (band total, band index, class{2},{3},{4+}) -- reproduces tdoku exactly
    //   2 EXACT_LEX     (band total, n)
    //   3 FLAT          n
    //   4 FLAT_LEX      (n, band total)
    //   5 SOFT          weighted generalized mean M_p^w(n, band_total/9)
    int pair_rule;
    double soft_w;
    double soft_p;
    // how to score tuples (band, digit, configuration); higher is better.
    //   0 FIRST         no scoring: first pair in index order, lowest configuration
    //   1 SUM           -(k0+k1+k2)
    //   2 GENMEAN       -M_p(q0,q1,q2)       (p = ix_p; p <= -100 means min)
    //   3 G             (1 - q0 q1 q2) / n
    //   4 TWO_PROD      (1 + d+) (1 + d-)  with d+/d- estimated eliminations
    //   5 TWO_SUM       d+ + d-
    //   6 LOOKAHEAD     (1 + D+) (1 + D-) with D+/D- *actual* eliminations after propagation
    //                   (an expensive analysis baseline, not a candidate heuristic)
    //   7 TWO_AM        -(AM(q+) + AM(q-))   arithmetic-mean residual fractions, both branches
    //   8 TWO_AM_MAX    -max(AM(q+), AM(q-))
    //   9 GM2           -sum_y [log q+_y + log q-_y]
    //  10 POW2          -sum_y [(q+_y)^p + (q-_y)^p]          (p = ix_p)
    //  11 PROD2         -(sum_y q+_y) * (sum_y q-_y)
    //  12 INF2          AM2 over opposite bands with n_y >= 2 only (mean over them)
    //  13 AM2_NB        AM2 + ix_p * band_total / 9
    //  14 CELL2         -sum_y [c+_y + c-_y], c = surviving fraction of d-cells in the box shared
    //                   with band y (own-band information instead of intersecting bands)
    //  15 AM2_CELL      AM2 + ix_p * CELL2
    //  16 AM2_LOGW      sum_y log2(n_y) * (q+_y + q-_y)
    //  17 EXC2          -sum_y [(k+_y - 1) + (k-_y - 1)] / (n_y - 1)   (fixed opposite: 1 each)
    //  18 FORCE2        AM2 + ix_p * #(branches that reduce an opposite band to one config)
    int ix_score;
    double ix_p;
    // 0: score tuples from all pairs in the best class; 1: pick the first pair of the best
    // class, then score only its configurations ("configuration choice only").
    int ix_scope;
};

#endif  // TDOKU_BRANCH_FEATURES_H
