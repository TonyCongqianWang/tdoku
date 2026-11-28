#include "bitutil.h"
#include "simd_vectors.h"
#include "util.h"

#include <array>
#include <cstring>

#define LIKELY(x) __builtin_expect(!!(x),1)

using namespace std;

namespace {

using Cells08 = Bitvec08x16;
using Cells16 = Bitvec16x16;

constexpr uint16_t kAll = 0x1ff;

//  The state of each box is stored in a vector of 16 uint16_t,     +---+---+---+---+
//  arranged as a 4x4 matrix of 9-bit candidate sets (the high      | c | c | c | H |
//  7 bits of each value are always zero). The top-left 3x3 sub-    +---+---+---+---+
//  matrix stores candidate sets for the 9 cells("c") of the box,   | c | c | c | H |
//  while the right 3x1 column and bottom 1x3 row store candidate   +---+---+---+---+
//  sets representing negative horizontal("H") and vertical("V")    | c | c | c | H |
//  triads respectively. A negative triad candidate will be         +---+---+---+---+
//  eliminated whenever we know that the same value must exist      | V | V | V |   |
//  in one three regular cells to which the triad corresponds.      +---+---+---+---+
//
//  For each value bit there is an exactly-one constraint over the 4 cells in a row
//  or column of the matrix corresponding to the biconditional defining the triad.
//
//  Each cell also has a minimum. So there are three sets of clauses represented here.
//
struct Box {
    Cells16 cells{Cells16::All(kAll)};
};

// For a given value there are only 6 possible configurations for how that value can be
// placed in the triads of a band. Our primary representation for the state of a band will
// be in terms of these configurations rather than the triads themselves. The possible
// configurations are numbered according to the following table:
//
//            config       0       1       2       3       4       5
//             elem      0 1 2   0 1 2   0 1 2   0 1 2   0 1 2   0 1 2
//                     +-------+-------+-------+-------+-------+-------+
//            peer0    | X . . | . X . | . . X | . . X | X . . | . X . |
//            peer1    | . X . | . . X | X . . | . X . | . . X | X . . |
//            peer2    | . . X | X . . | . X . | X . . | . X . | . . X |
//                     +-------+-------+-------+-------+-------+-------+
//
// The primary state of the band is stored as 9-bit masks     elem    0   1   2
// in the first 6 elements of an 8 uint16_t vector.                 +---+---+---+---+
//                                                           peer0  | t | t | t |   |
// When constructing elimination masks to send to the boxes         +---+---+---+---+
// we'll convert the configuration vector into a 3x3 matrix  peer1  | t | t | t |   |
// of positive triad candidates, which are arranged with            +---+---+---+---+
// box peers along the rows of 4x4 matrix in a 16 uint16_t   peer2  | t | t | t |   |
// vector (for both horizontal and vertical bands).                 +---+---+---+---+
//                                                                  |   |   |   |   |
// We'll also store with the Band a vector of eliminations          +---+---+---+---+
// to be applied to the Band's configurations on the next
// call to BandEliminate. This allows us to apply all pending updates to a band at
// the first opportunity instead of individually depending on where in the call stack
// the update originates.
//
// Note that we might do the same thing for Boxes, and doing so is beneficial for easy
// puzzles. Unfortunately, it's a net loss for hard puzzles. The cost in State size
// is higher, and the benefit is lower (the benefit for Bands chiefly arises from the
// way we do puzzle initialization).
//
struct Band {
    Cells08 configurations{kAll, kAll, kAll, kAll, kAll, kAll, 0, 0};
    Cells08 eliminations{};
};

struct State {
    Band bands[2][3]{};
    Box boxen[9]{};
};

struct BoxIndexing {
    uint8_t box_i;
    uint8_t box_j;
    uint8_t box;
    uint8_t elem_i;
    uint8_t elem_j;
    uint8_t elem;

    BoxIndexing() = default;

    explicit BoxIndexing(int cell) : box_i(cell / 27), box_j((cell % 9) / 3),
                                     box(box_i * 3 + box_j),
                                     elem_i((cell / 9) % 3), elem_j(cell % 3),
                                     elem(elem_i * 4 + elem_j) {}
};

// We depend on low-level shuffle operations that address packed 8-bit integers, but we're
// always shuffling 16-bit logical cells. These constants are used for constructing shuffle
// control vectors that address these cells. We only require 8 of them since even 256-bit
// shuffles operate within 128-bit lanes.
constexpr uint16_t shuf00 = 0x0100, shuf01 = 0x0302, shuf02 = 0x0504, shuf03 = 0x0706;
constexpr uint16_t shuf04 = 0x0908, shuf05 = 0x0b0a, shuf06 = 0x0d0c, shuf07 = 0x0f0e;

struct Tables {
    // @formatter:off
    // used when assigning a candidate during initialization
    Cells16 cell_assignment_eliminations[9][16]{};

    //   config       0       1       2       3       4       5
    //    elem      0 1 2   0 1 2   0 1 2   0 1 2   0 1 2   0 1 2
    //            +-------+-------+-------+-------+-------+-------+
    //   peer0    | X . . | . X . | . . X | . . X | X . . | . X . |
    //   peer1    | . X . | . . X | X . . | . X . | . . X | X . . |
    //   peer2    | . . X | X . . | . X . | X . . | . X . | . . X |
    //            +-------+-------+-------+-------+-------+-------+
    //
    // A set of masks for eliminating band configurations inconsistent with the placement
    // of a digit in an element (minirow or minicol) of a box peer.
    //
    const Cells08 peer_x_elem_to_config_mask[3][4]{
            {{   0,   kAll,   kAll,   kAll,      0,   kAll,    0,    0},
             {kAll,      0,   kAll,   kAll,   kAll,      0,    0,    0},
             {kAll,   kAll,      0,      0,   kAll,   kAll,    0,    0},
             {   0,      0,      0,      0,      0,      0,    0,    0}},
            {{kAll,   kAll,      0,   kAll,   kAll,      0,    0,    0},
             {   0,   kAll,   kAll,      0,   kAll,   kAll,    0,    0},
             {kAll,      0,   kAll,   kAll,      0,   kAll,    0,    0},
             {   0,      0,      0,      0,      0,      0,    0,    0}},
            {{kAll,      0,   kAll,      0,   kAll,   kAll,    0,    0},
             {kAll,   kAll,      0,   kAll,      0,   kAll,    0,    0},
             {   0,   kAll,   kAll,   kAll,   kAll,      0,    0,    0},
             {   0,      0,      0,      0,      0,      0,    0,    0}}
    };

    // tables for constructing band elimination messages from Cells08 containing
    // positive or negative triad views of a box stored positions 4, 5, and 6.
    // each table has three shuffle control vectors, one for each of the band's box
    // peers. there are three tables, each corresponding to a rotation of elements
    // in the peer. look first at the shift0 table to see the correspondence with
    // the configuration diagram reproduced above.
    //
    const Cells08 triads_shift0_to_config_elims[3]{
            {shuf04, shuf05, shuf06, shuf06, shuf04, shuf05, 0xffff, 0xffff},
            {shuf05, shuf06, shuf04, shuf05, shuf06, shuf04, 0xffff, 0xffff},
            {shuf06, shuf04, shuf05, shuf04, shuf05, shuf06, 0xffff, 0xffff}
    };
    const Cells08 triads_shift1_to_config_elims[3]{
            {shuf05, shuf06, shuf04, shuf04, shuf05, shuf06, 0xffff, 0xffff},
            {shuf06, shuf04, shuf05, shuf06, shuf04, shuf05, 0xffff, 0xffff},
            {shuf04, shuf05, shuf06, shuf05, shuf06, shuf04, 0xffff, 0xffff}
    };
    const Cells08 triads_shift2_to_config_elims[3]{
            {shuf06, shuf04, shuf05, shuf05, shuf06, shuf04, 0xffff, 0xffff},
            {shuf04, shuf05, shuf06, shuf04, shuf05, shuf06, 0xffff, 0xffff},
            {shuf05, shuf06, shuf04, shuf06, shuf04, shuf05, 0xffff, 0xffff}
    };

    // Cells16 shuffle control vectors constructed from the 9 pairings of 3x3 vectors in
    // the tables above (because this makes access more efficient in AssertionsToEliminations).
    Cells16 triads_shift0_to_config_elims16[9]{};
    Cells16 triads_shift1_to_config_elims16[9]{};
    Cells16 triads_shift2_to_config_elims16[9]{};

    // two Cells16 shuffle control vectors whose results are or'ed together to convert
    // a vector of configurations (reproduced across 128 bit lanes) into a 3x3 matrix of
    // positive triads (refer again to the configuration diagram above).
    const Cells16 shuffle_configs_to_triads[2]{
            {{shuf00, shuf01, shuf02, 0xffff,
              shuf02, shuf00, shuf01, 0xffff},
             {shuf01, shuf02, shuf00, 0xffff,
              0xffff, 0xffff, 0xffff, 0xffff}},
            {{shuf04, shuf05, shuf03, 0xffff,
              shuf05, shuf03, shuf04, 0xffff},
             {shuf03, shuf04, shuf05, 0xffff,
              0xffff, 0xffff, 0xffff, 0xffff}}
    };

    // two pairs of two Cells16 shuffle control vectors whose results are or'ed together to
    // convert vectors of positive triads in positions 0, 1, and 2 (reproduced across 128 bit
    // lanes) into box candidate sets. it is necessary to combine two shuffles because box
    // negative triads are eliminated when band positive triads have been eliminated in the
    // other two shifted positions. the shuffled input has 0xffff in position 3 to allow a
    // no-op for triads with opposite orientation.
    const Cells16 pos_triads_to_candidates[2][2]{
            // horizontal
            {{{shuf00, shuf00, shuf00, shuf01,
               shuf01, shuf01, shuf01, shuf02},
              {shuf02, shuf02, shuf02, shuf00,
               shuf03, shuf03, shuf03, shuf03}},
             {{shuf00, shuf00, shuf00, shuf02,
               shuf01, shuf01, shuf01, shuf00},
              {shuf02, shuf02, shuf02, shuf01,
               shuf03, shuf03, shuf03, shuf03}}},
            // vertical
            {{{shuf00, shuf01, shuf02, shuf03,
               shuf00, shuf01, shuf02, shuf03},
              {shuf00, shuf01, shuf02, shuf03,
               shuf01, shuf02, shuf00, shuf03}},
             {{shuf00, shuf01, shuf02, shuf03,
               shuf00, shuf01, shuf02, shuf03},
              {shuf00, shuf01, shuf02, shuf03,
               shuf02, shuf00, shuf01, shuf03}}}
    };

    const Cells16 cell3x3_mask{ kAll, kAll, kAll,    0,
                                kAll, kAll, kAll,    0,
                                kAll, kAll, kAll,    0,
                                   0,    0,    0,    0
    };
    // row rotation shuffle controls vectors for just the 3x3 submatrix of a Cells16
    const Cells16 row_rotate_3x3_1{
        shuf01, shuf02, shuf00, shuf03, shuf05, shuf06, shuf04, shuf07,
        shuf01, shuf02, shuf00, shuf03, shuf04, shuf05, shuf06, shuf07};
    const Cells16 row_rotate_3x3_2{
        shuf02, shuf00, shuf01, shuf03, shuf06, shuf04, shuf05, shuf07,
        shuf02, shuf00, shuf01, shuf03, shuf04, shuf05, shuf06, shuf07};

    const Cells08 one_value_mask[9]{
            Cells08::All(1u << 0u), Cells08::All(1u << 1u), Cells08::All(1u << 2u),
            Cells08::All(1u << 3u), Cells08::All(1u << 4u), Cells08::All(1u << 5u),
            Cells08::All(1u << 6u), Cells08::All(1u << 7u), Cells08::All(1u << 8u),
    };

    const int box_peers[2][3][3]{
            {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}},
            {{0, 3, 6}, {1, 4, 7}, {2, 5, 8}}
    };
    const int div3[9]{ 0, 0, 0, 1, 1, 1, 2, 2, 2 };
    const int mod3[9]{ 0, 1, 2, 0, 1, 2, 0, 1, 2 };

    BoxIndexing box_indexing[81]{};
    // @formatter:on

    Tables() noexcept {
        for (int i : {0, 1, 2, 4, 5, 6, 8, 9, 10}) {  // only need for cells, not triads
            for (uint32_t value = 0; value < 9; value++) {
                Cells16 &mask = cell_assignment_eliminations[value][i];
                for (int j = 0; j < 15; j++) {
                    if (j == i) { // asserted cell: clear all bits but the one asserted
                        mask.Insert(j, kAll ^ (1u << value));
                    } else if (j / 4 < 3 && j % 4 < 3) { // conflict cell: clear the asserted bit
                        mask.Insert(j, 1u << value);
                    } else if (j / 4 == i / 4 || j % 4 == i % 4) { // clear 2 negative triads
                        mask.Insert(j, 1u << value);
                    }
                }
            }
        }
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                triads_shift0_to_config_elims16[i * 3 + j] =
                        Cells16{triads_shift0_to_config_elims[i],triads_shift0_to_config_elims[j]};
                triads_shift1_to_config_elims16[i * 3 + j] =
                        Cells16{triads_shift1_to_config_elims[i],triads_shift1_to_config_elims[j]};
                triads_shift2_to_config_elims16[i * 3 + j] =
                        Cells16{triads_shift2_to_config_elims[i],triads_shift2_to_config_elims[j]};
            }
        }
        for (int i = 0; i < 81; i++) box_indexing[i] = BoxIndexing{i};
    }
};

const Tables tables{};

// Branching heuristic ("branch volume"). A decision splits the configurations C (n = |C| >= 2) of
// a digit in a band into {c} | C \ {c}. It is chosen to minimize the estimated remaining search
// volume of the two branches,
//     s = ln(n^-be P+ + ((n-1)/n)^be P-) + g(n) + gR R,     P = prod_j (k_j / n_j)^al,
// where, for each of the three crossing bands j, n_j is the number of configurations of the digit
// in band j and k_j the number of those that stay locally compatible with the branch (they place
// the digit in the shared box at a cell that the branch's rows / columns allow; for the - branch
// the union of the rows of C \ {c}). R is the band's number of configurations and g(2) = 0.
// Parameters were fitted on end-to-end guess counts: al 1.1, be 0.6, g(3) -0.25, g(>=4) -0.1,
// gR 0.015 (natural log units).
//
// All 6 x 9 x 6 candidates are scored at once in 8-bit log arithmetic (1/24 bit): crossing counts
// are table lookups of popcounts in a "digit per byte" layout, products are saturating sums and
// sums of volumes a log-sum-exp with a 16-entry correction table. The six bands' digits are packed
// into two AVX2 vectors. Ties go to the lowest band, digit and configuration. GFNI is used when
// available (with an exact AVX2 fallback). Without AVX2 the original heuristic is used.
#ifdef __AVX2__
namespace branch_volume {

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

struct VolumeTables {
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

    VolumeTables() noexcept {
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

const VolumeTables volume_tables{};

inline __m256i Swap(const __m256i &x) { return _mm256_permute2x128_si256(x, x, 0x01); }

// byte d (digit) of each lane: bit k set iff digit d is in the k-th selected element of the lane
template <int count>
inline __m256i Transpose(const __m256i &x, const __m256i &gf_ctrl, const __m256i *expand) {
#ifdef __GFNI__
    (void) expand;
    return _mm256_gf2p8affine_epi64_epi8(volume_tables.ident, _mm256_shuffle_epi8(x, gf_ctrl), 0);
#else
    (void) gf_ctrl;
    __m256i acc = _mm256_setzero_si256();
    for (int k = 0; k < count; k++) {
        __m256i sel = _mm256_and_si256(_mm256_shuffle_epi8(x, expand[k]), volume_tables.bitsel);
        acc = _mm256_or_si256(acc, _mm256_and_si256(_mm256_cmpeq_epi8(sel, volume_tables.bitsel),
                                                     _mm256_set1_epi8((char) (1 << k))));
    }
    return acc;
#endif
}

// popcount of bytes holding at most 6 bits
inline __m256i Pop6(const __m256i &x) {
    const VolumeTables &t = volume_tables;
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
    const VolumeTables &t = volume_tables;
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
            _mm256_or_si256(x2, _mm256_shuffle_epi8(_mm256_unpackhi_epi64(x0, x1), volume_tables.pack_ctrl))};
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
    const VolumeTables &t = volume_tables;
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
    const VolumeTables &t = volume_tables;
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

}  // namespace branch_volume
#endif  // __AVX2__

template<int solution_mode>
struct SolverDpllTriadSimd {
    State solution_{};
    size_t limit_ = 1;
    size_t num_solutions_ = 0;
    size_t num_guesses_ = 0;
    bool original_heuristic_ = false;  // branch with ChooseBandAndValueToBranch instead
    void (*callback_)(const char *, void *) = nullptr;
    void *callback_arg_ = nullptr;

    // restrict the cell, minirow, and minicol clauses of the box to contain only the given
    // cell and triad candidates.
    template<int from_vertical>
    static bool BoxRestrict(State &state, int box_idx, const Cells16 &candidates) {
        // return immediately if there are no new eliminations
        Box &box = state.boxen[box_idx];
        if (box.cells.SubsetOf(candidates)) return true;
        auto eliminating = box.cells.and_not(candidates);

        int box_i = tables.div3[box_idx];
        int box_j = tables.mod3[box_idx];

        Band &h_band = state.bands[0][box_i];
        Band &v_band = state.bands[1][box_j];
        do {
            // apply eliminations and check that no cell clause now violates its minimum
            box.cells = box.cells.and_not(eliminating);
            Cells16 counts = box.cells.Popcounts9();
            const Cells16 box_minimums{1, 1, 1, 6, 1, 1, 1, 6, 1, 1, 1, 6, 6, 6, 6, 0};
            if (counts.AnyLessThan(box_minimums)) return false;

            // gather literals asserted by triggered cell clauses
            Cells16 triggered = counts.WhichEqual(box_minimums);
            Cells16 all_assertions = box.cells & triggered;
            // and add literals asserted by triggered triad definition clauses
            GatherTriadClauseAssertions(
                    box.cells, [](const Cells16 &x) { return x.RotateRows(); }, all_assertions);
            GatherTriadClauseAssertions(
                    box.cells, [](const Cells16 &x) { return x.RotateCols(); }, all_assertions);

            // construct elimination messages for this box and for our band peers
            AssertionsToEliminations(all_assertions, box_i, box_j, eliminating,
                                     h_band.eliminations, v_band.eliminations);

        } while (eliminating.Intersects(box.cells));

        // send elimination messages to horizontal and vertical peers. Prefer to send the first
        // of these messages to the peer whose orientation is opposite that of the inbound peer.
        if (from_vertical) {
            return BandEliminate<0>(state, box_i, box_j) &&
                   BandEliminate<1>(state, box_j, box_i);
        } else {
            return BandEliminate<1>(state, box_j, box_i) &&
                   BandEliminate<0>(state, box_i, box_j);
        }
    }

    // input Cells16 contains zeros where nothing is asserted, a single candidate for regular cells
    // that are being asserted, and either 1 or 6 candidates for negative triad literals that are
    // being asserted (due to an unsatisfiable triad definition, or due to a 6/ minimum).
    static inline void AssertionsToEliminations(const Cells16 &assertions, int box_i, int box_j,
                                                Cells16 &box_eliminations,
                                                Cells08 &h_band_eliminations,
                                                Cells08 &v_band_eliminations) {
        // we could guard some or all of the code below with checks that the assertion vector as
        // a whole, or cell or negative triad components of it, are nonzero. but the branches
        // would most often be taken. it's cheaper to compute no-op updates than it is to pay the
        // branch cost.

        // update the self eliminations for new assertions in the box.
        auto cell_assertions_only = assertions & tables.cell3x3_mask;
        // compute matrices broadcasting assertions across rows and columns in which they occur.
        Cells16 across_rows = cell_assertions_only;
        across_rows |= across_rows.RotateRows();
        across_rows |= across_rows.RotateRows2();
        Cells16 across_cols = cell_assertions_only;
        across_cols |= across_cols.RotateCols();
        across_cols |= across_cols.RotateCols2();
        // let 3x3 submatrix have assertions occuring anywhere
        Cells16 new_box_eliminations = Cells16::X_Y_or_Z_or(across_cols,
                                                            across_cols.Shuffle(tables.row_rotate_3x3_1),
                                                            across_cols.Shuffle(tables.row_rotate_3x3_2));
        // join 3x3 submatrix, row/col margins, and all elimination bits in asserted cells
        new_box_eliminations = Cells16::X_Y_or_Z_or(
                new_box_eliminations, across_rows, cell_assertions_only.WhichNonZero());
        // then apply after clearing elimination bits for the asserted candidates.
        box_eliminations = Cells16::X_Y_xor_Z_or(
                new_box_eliminations, cell_assertions_only, box_eliminations);

        // below we'll update band eliminations to reflect assertion of negative triads or positive
        // literals within this box. in the case of asserted negative triads we'll eliminate the
        // corresponding positive triads in the band (at shift 0).
        Cells16 hv_neg_triad_assertions{HorizontalTriads(assertions),
                                        VerticalTriads(assertions)};
        // in the case of asserted positive literals, which imply the assertion of corresponding
        // shift 0 positive triads, we'll eliminate the triads at shifts 1 and 2 in the band.
        Cells16 hv_pos_triad_assertions{HorizontalTriads(new_box_eliminations),
                                        VerticalTriads(new_box_eliminations)};
        Cells16 new_eliminations = Cells16::X_Y_or_Z_or(
                hv_neg_triad_assertions.Shuffle(
                        tables.triads_shift0_to_config_elims16[box_j * 3 + box_i]),
                hv_pos_triad_assertions.Shuffle(
                        tables.triads_shift1_to_config_elims16[box_j * 3 + box_i]),
                hv_pos_triad_assertions.Shuffle(
                        tables.triads_shift2_to_config_elims16[box_j * 3 + box_i]));
        h_band_eliminations |= new_eliminations.GetLo();
        v_band_eliminations |= new_eliminations.GetHi();
    }

    // extracts a Cells08 containing (positive or negative) vertical triad literals in positions
    // 4, 5, and 6 for use in shuffling an elimination message to send the vertical band peer. the
    // contents of other cells are ignored by the shuffle.
    static inline Cells08 VerticalTriads(const Cells16 &cells) {
        return cells.GetHi();
    }

    // extracts a Cells08 containing (positive or negative) horizontal triad literals in positions
    // 4, 5, and 6 for use in shuffling an elimination message to send the horizontal band peer. we
    // use positions 4,5,6 so we can use the same tables in creating horizontal and vertical
    // elimination messages (and so the vertical triads can be extracted at no cost).
    static inline Cells08 HorizontalTriads(const Cells16 &cells) {
        Cells16 split_triads = cells.Shuffle(
                Bitvec16x16{{0xffff, 0xffff, 0xffff, 0xffff, shuf03, shuf07, 0xffff, 0xffff},
                            {0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, shuf03, 0xffff}});
        return split_triads.GetLo() | split_triads.GetHi();
    }

    template<typename RotateFn>
    static inline void GatherTriadClauseAssertions(const Cells16 &cells,
                                                   RotateFn rotate, Cells16 &assertions) {
        // find 'one_or_more' and 'two_or_more', each a set of 4 row/col vectors depending on the
        // given rotation function, where each cell in a row/col contains the bits that occur 1+ or
        // 2+ times across the cells of the corresponding source row/col.
        auto one_or_more = cells;
        auto rotated = rotate(cells);
        auto two_or_more = one_or_more & rotated;
        one_or_more |= rotated;
        rotated = rotate(rotated);
        two_or_more = Cells16::X_Y_and_Z_or(one_or_more, rotated, two_or_more);
        one_or_more |= rotated;
        rotated = rotate(rotated);
        two_or_more = Cells16::X_Y_and_Z_or(one_or_more, rotated, two_or_more);
        // we might rotate again and check that one_or_more == kAll, but the check is a net loss.
        // now assert (in cells where they remain) candidates that occur only once an a row/col.
        assertions = Cells16::X_Y_andnot_Z_or(cells, two_or_more, assertions);
    }

    template<int vertical>
    static bool BandEliminate(State &state, int band_idx, int from_peer = 0) {
        Band &band = state.bands[vertical][band_idx];
        if (LIKELY(!band.configurations.Intersects(band.eliminations))) return true;
        // after eliminating we might check that every value is still consistent with some
        // configuration, but the check is a net loss.
        band.configurations = band.configurations.and_not(band.eliminations);

        Cells16 triads = ConfigurationsToPositiveTriads(band.configurations);
        // we might check here that every cell (corresponding to a minirow or minicol) still has
        // at least three triad candidates, but the check is a net loss.
        Cells16 counts = triads.Popcounts9();

        // we might repeat the updating of triads below until we no longer trigger new triad 3/
        // clauses. however, just once delivers most of the benefit, and it's best not to branch.
        Cells16 asserting = triads & counts.WhichEqual(Cells16::All(3));
        Cells08 lo = asserting.GetLo();
        Cells08 hi = asserting.GetHi();
        band.configurations = band.configurations.and_not(Cells08::X_Y_or_Z_or(
                lo.RotateCols().Shuffle(tables.triads_shift1_to_config_elims[0]),
                lo.RotateCols().Shuffle(tables.triads_shift2_to_config_elims[0]),
                lo.Shuffle(tables.triads_shift1_to_config_elims[1])));
        band.configurations = band.configurations.and_not(Cells08::X_Y_or_Z_or(
                lo.Shuffle(tables.triads_shift2_to_config_elims[1]),
                hi.RotateCols().Shuffle(tables.triads_shift1_to_config_elims[2]),
                hi.RotateCols().Shuffle(tables.triads_shift2_to_config_elims[2])));
        triads = ConfigurationsToPositiveTriads(band.configurations);

        // convert positive triads to box restriction messages and send to the three box peers.
        // send these messages in order so that we return to the inbound peer last.
        int peer[3]{tables.mod3[from_peer + 1], tables.mod3[from_peer + 2], from_peer};
        auto &box_peers = tables.box_peers[vertical][band_idx];
        Cells08 peer_triads[3]{ triads.GetLo(), triads.GetLo().RotateCols(), triads.GetHi() };
        return (BoxRestrict<vertical>(state, box_peers[peer[0]],
                        PositiveTriadsToBoxCandidates(peer_triads[peer[0]], vertical)) &&
                BoxRestrict<vertical>(state, box_peers[peer[1]],
                        PositiveTriadsToBoxCandidates(peer_triads[peer[1]], vertical)) &&
                BoxRestrict<vertical>(state, box_peers[peer[2]],
                        PositiveTriadsToBoxCandidates(peer_triads[peer[2]], vertical)));
    }

    // convert band configuration into an equivalent 3x3 matrix of positive triad candidates,
    // where each row represents the constraints the band imposes on a given box peer.
    static inline Cells16 ConfigurationsToPositiveTriads(const Cells08 &configurations) {
        Cells16 tmp{configurations, configurations};
        return tmp.Shuffle(tables.shuffle_configs_to_triads[0]) |
               tmp.Shuffle(tables.shuffle_configs_to_triads[1]);
    }

    // convert 3 sets of positive triads (found in cells 0,1,2 of the given Cells08) into a
    // mask for restricting the corresponding box peer.
    static inline Cells16 PositiveTriadsToBoxCandidates(const Cells08 &triads, int orientation) {
        Cells08 triads_with_kAll = triads | Cells08{0x0, 0x0, 0x0, kAll, 0x0, 0x0, 0x0, 0x0};
        Cells16 tmp{triads_with_kAll, triads_with_kAll};
        return tmp.Shuffle(tables.pos_triads_to_candidates[orientation][0]) |
               tmp.Shuffle(tables.pos_triads_to_candidates[orientation][1]);
    }

    ///////////////////////////////////////////////////////////////////////////////////

    static constexpr uint32_t NONE = UINT32_MAX;

    static inline pair<uint32_t, Cells08> ChooseBandAndValueToBranch(const State &state) {
        uint32_t best_band = NONE, best_band_count = NONE;
        uint32_t best_value = NONE, best_value_count = NONE;

        // first find the unfixed band with the fewest possible configurations across all values.
        // a minimum unfixed band will have 0 <= count-10 <= 44. if all bands are fixed then the
        // minimum after subtracting 10 and interpreting as a uint will be 0xfffa.
        uint32_t config_minpos = Bitvec08x16{
                (uint16_t)state.bands[0][0].configurations.Popcount(),
                (uint16_t)state.bands[0][1].configurations.Popcount(),
                (uint16_t)state.bands[0][2].configurations.Popcount(),
                (uint16_t)state.bands[1][0].configurations.Popcount(),
                (uint16_t)state.bands[1][1].configurations.Popcount(),
                (uint16_t)state.bands[1][2].configurations.Popcount(),
                (uint16_t)0xffff,
                (uint16_t)0xffff
        }.MinPosGreaterThanOrEqual(10);

        // if we have an unfixed band then find a digit in the band with the fewest possibilities.
        // the configurations of each digit are counted in binary (up to 6) with carry-save adders,
        // and a digit with exactly two, else exactly three, else four or more configurations is
        // chosen. this is faster than counting exactly and taking the minimum position as above,
        // but it is inexact in rare cases when all digits have 4 or more configurations. the
        // tradeoff is a net positive for Vanilla Sudoku and a net negative for Pencilmark Sudoku.
        if ((config_minpos & 0xff00u) == 0) {
            best_band = config_minpos >> 16u;
            const auto &configurations =
                    state.bands[tables.div3[best_band]][tables.mod3[best_band]].configurations;
            // a full adder over lanes {i, i+1, i+2}; rotating its outputs by three lanes gives the
            // one over lanes {i+3, i+4, i+5}, so every lane 0..5 ends up with the full count
            // 4 * b2 + 2 * b1 + b0 of each digit.
            Cells08 rotate1 = Cells08{shuf01, shuf02, shuf03, shuf04, shuf05, shuf00, 0xffff, 0xffff};
            Cells08 rotate3 = Cells08{shuf03, shuf04, shuf05, shuf00, shuf01, shuf02, 0xffff, 0xffff};
            Cells08 r1 = configurations.Shuffle(rotate1);
            Cells08 r2 = r1.Shuffle(rotate1);
            Cells08 t = configurations ^ r1;
            Cells08 s = t ^ r2;
            Cells08 c = (configurations & r1) | (t & r2);
            Cells08 s3 = s.Shuffle(rotate3);
            Cells08 c3 = c.Shuffle(rotate3);
            Cells08 b0 = s ^ s3;
            Cells08 cc = s & s3;
            Cells08 t2 = c ^ c3;
            Cells08 b1 = t2 ^ cc;
            Cells08 b2 = (c & c3) | (t2 & cc);

            Cells08 only_two = b1.and_not(b0 | b2);
            if (!LIKELY(only_two.AllZero())) {
                return {best_band, only_two.GetLowBit()};
            } else {
                Cells08 only_three = (b1 & b0).and_not(b2);
                if (LIKELY(!only_three.AllZero())) {
                    return {best_band, only_three.GetLowBit()};
                } else {
                    return {best_band, b2.GetLowBit()};
                }
            }
        }
        return {best_band, Cells08::All(0)};
    }

    template<int vertical>
    void BranchOnBandAndValue(int band_idx, const Cells08 &value_mask, State &state) {
        Band &band = state.bands[vertical][band_idx];
        // we enter with two or more possible configurations for this value
        Cells08 value_configurations = band.configurations & value_mask;
        // assign the first configuration by eliminating the others
        num_guesses_++;
        State state_copy = state;
        Cells08 assignment_elims = value_configurations.ClearLowBit();
        state_copy.bands[vertical][band_idx].eliminations |= assignment_elims;
        if (BandEliminate<vertical>(state_copy, band_idx)) {
            CountSolutionsConsistentWithPartialAssignment(state_copy);
            if (num_solutions_ == limit_) return;
        }
        // now negate the first configuration
        Cells08 negation_elims = value_configurations ^ assignment_elims;
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (BandEliminate<vertical>(state, band_idx)) {
            CountSolutionsConsistentWithPartialAssignment(state);
        }
    }

    // branch on asserting vs. eliminating configuration `config` of `digit` in the band
    template<int vertical>
    void BranchOnConfig(int band_idx, int digit, int config, State &state) {
        static const Cells08 config_lane[6]{
                {0xffff, 0, 0, 0, 0, 0, 0, 0}, {0, 0xffff, 0, 0, 0, 0, 0, 0},
                {0, 0, 0xffff, 0, 0, 0, 0, 0}, {0, 0, 0, 0xffff, 0, 0, 0, 0},
                {0, 0, 0, 0, 0xffff, 0, 0, 0}, {0, 0, 0, 0, 0, 0xffff, 0, 0}};
        Band &band = state.bands[vertical][band_idx];
        Cells08 value_configurations = band.configurations & tables.one_value_mask[digit];
        num_guesses_++;
        State state_copy = state;
        Cells08 assignment_elims = value_configurations.and_not(config_lane[config]);
        state_copy.bands[vertical][band_idx].eliminations |= assignment_elims;
        if (BandEliminate<vertical>(state_copy, band_idx)) {
            CountSolutionsConsistentWithPartialAssignment(state_copy);
            if (num_solutions_ == limit_) return;
        }
        Cells08 negation_elims = value_configurations & config_lane[config];
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (BandEliminate<vertical>(state, band_idx)) {
            CountSolutionsConsistentWithPartialAssignment(state);
        }
    }

    void RecordSolution(State &state) {
        num_solutions_++;
        if (solution_mode == 1 && num_solutions_ == limit_) solution_ = state;
        if (solution_mode == 2) ReportSolution(state);
    }

    // do not call this twice on the same state. for efficiency this count may modify the
    // given state instead of making copies. if called with limit > 1 this can leave the state
    // changed in a way that makes subsequent calls return different results.
    void CountSolutionsConsistentWithPartialAssignment(State &state) {
#ifdef __AVX2__
        if (!original_heuristic_) {
            int band, digit, config;
            if (!branch_volume::Choose(state, band, digit, config)) {
                RecordSolution(state);
            } else if (band < 3) {
                BranchOnConfig<0>(band, digit, config, state);
            } else {
                BranchOnConfig<1>(band - 3, digit, config, state);
            }
            return;
        }
#endif
        auto band_and_value = ChooseBandAndValueToBranch(state);
        if (band_and_value.first == NONE) {
            RecordSolution(state);
        } else {
            if (band_and_value.first < 3) {
                BranchOnBandAndValue<0>(
                        tables.mod3[band_and_value.first], band_and_value.second, state);
            } else {
                BranchOnBandAndValue<1>(
                        tables.mod3[band_and_value.first], band_and_value.second, state);
            }
        }
    }

    size_t SafeCountSolutionsConsistentWithPartialAssignment(State state, size_t limit) {
        limit_ = limit;
        num_solutions_ = 0;
        CountSolutionsConsistentWithPartialAssignment(state);
        return num_solutions_;
    }

    ///////////////////////////////////////////////////////////////////////////////////

    static inline void InitClue(const char *input, State &state, int pos) {
        const BoxIndexing &indexing = tables.box_indexing[pos];
        char digit = input[pos];
        uint16_t candidate = 1u << (uint32_t) (digit - '1');
        // perform eliminations for the clue in its own box, but don't propagate. this is
        // not strictly necessary since band eliminations will constrain the puzzle, but it
        // turns out to be important for performance on invalid zero-solution puzzles.
        state.boxen[indexing.box].cells = state.boxen[indexing.box].cells.and_not(
                tables.cell_assignment_eliminations[digit - '1'][indexing.elem]);
        // merge band eliminations; we'll propagate after all clue are processed.
        state.bands[0][indexing.box_i].eliminations = Cells08::X_Y_and_Z_or(
                tables.peer_x_elem_to_config_mask[indexing.box_j][indexing.elem_i],
                Cells08::All(candidate),
                state.bands[0][indexing.box_i].eliminations);
        state.bands[1][indexing.box_j].eliminations = Cells08::X_Y_and_Z_or(
                tables.peer_x_elem_to_config_mask[indexing.box_i][indexing.elem_j],
                Cells08::All(candidate),
                state.bands[1][indexing.box_j].eliminations);
    }

    // We could set the initial clues in other ways, including one box update for each clue, or
    // one batch box update for each box. But it's fastest to start with 6 batched band updates.
    static bool InitVanillaByBand(const char *input, State &state) {
        uint64_t clues64 = WhichDots64(input) ^ (uint64_t)-1ll;
        while (clues64) {
            int cell_idx = LowOrderBitIndex64(clues64);
            InitClue(input, state, cell_idx);
            clues64 = ClearLowBit64(clues64);
        }
        uint32_t clues16 = WhichDots16(input + 64) ^ 0xffffu;
        while (clues16) {
            int cell_idx = 64 + LowOrderBitIndex(clues16);
            InitClue(input, state, cell_idx);
            clues16 = ClearLowBit(clues16);
        }
        if (input[80] != '.') {
            InitClue(input, state, 80);
        }
        // thanks to the merging of band updates the puzzle is almost always fully initialized
        // after the first of these calls. most will be no-ops, but we've still got to do them
        // since this cannot be guaranteed.
        return BandEliminate<0>(state, 0, 1) && BandEliminate<1>(state, 0, 1) &&
               BandEliminate<0>(state, 1, 2) && BandEliminate<1>(state, 1, 2) &&
               BandEliminate<0>(state, 2, 0) && BandEliminate<1>(state, 2, 0);
    }

    static bool InitPencilmarkByBox(const char *input, State &state) {
        char buf[736]; // make sure unaligned 16 byte cell reads won't go past end of buffer
        memcpy(&buf, input, 729);
        for (int box_i = 0; box_i < 3; box_i++) {
            for (int box_j = 0; box_j < 3; box_j++) {
                Cells16 box_candidates = Cells16::All(kAll);
                for (int elm_i = 0; elm_i < 3; elm_i++) {
                    for (int elm_j = 0; elm_j < 3; elm_j++) {
                        int cell = box_i * 27 + elm_i * 9 + box_j * 3 + elm_j;
                        auto cell_eliminations = WhichDots16(&buf[cell * 9]);
                        box_candidates.Insert(elm_i * 4 + elm_j, kAll & ~cell_eliminations);
                    }
                }
                if (!BoxRestrict<0>(state, box_i * 3 + box_j, box_candidates)) return false;
            }
        }
        return true;
    }

    static inline void ExtractMiniRow(uint64_t minirow, int minirow_base, char *solution) {
        solution[minirow_base + 0] = char('1' + LowOrderBitIndex(minirow >> 0u));
        solution[minirow_base + 1] = char('1' + LowOrderBitIndex(minirow >> 16u));
        solution[minirow_base + 2] = char('1' + LowOrderBitIndex(minirow >> 32u));
    }

    static void ExtractSolution(const State &state, char *solution) {
        for (int box_idx = 0; box_idx < 9; box_idx++) {
            const Box &box = state.boxen[box_idx];
            auto box_minirows = box.cells.As_4x64();
            int box_base = tables.div3[box_idx] * 27 + tables.mod3[box_idx] * 3;
            ExtractMiniRow(box_minirows.x0, box_base, solution);
            ExtractMiniRow(box_minirows.x1, box_base + 9, solution);
            ExtractMiniRow(box_minirows.x2, box_base + 18, solution);
        }
    }

    void ReportSolution(const State &state) {
        char solution[81];
        if (callback_) {
            ExtractSolution(state, solution);
            callback_(solution, callback_arg_);
        }
    }

    size_t SolveSudoku(const char *input, size_t limit,
                       char *solution, size_t *num_guesses) {
        limit_ = limit;
        num_solutions_ = 0;
        num_guesses_ = 0;
        bool pencilmark = input[81] >= '.';

        State state;
        if (pencilmark ? InitPencilmarkByBox(input, state) : InitVanillaByBand(input, state)) {
            CountSolutionsConsistentWithPartialAssignment(state);
            if (solution_mode == 1) ExtractSolution(solution_, solution);
        }
        if (solution_mode != 2) *num_guesses = num_guesses_;
        return num_solutions_;
    };
};


struct GeneratorDpllTriadSimd {
    SolverDpllTriadSimd<0> solver_{};
    Util util_;

    // takes a partial puzzle (vanilla or pencilmark) and adds random clues to reconstrain it
    // until there is a unique solution. this procedure is fast, but biased in the sense that
    // different puzzles may arise with widely varying probabilities and we make no effort
    // to adjust for these differences or to estimate these probabilities. it also does not
    // guarantee that the resulting puzzle is minimal.
    bool Constrain(bool pencilmark, char *puzzle) {
        State state;
        if (pencilmark) {
            SolverDpllTriadSimd<0>::InitPencilmarkByBox(puzzle, state);
        } else {
            SolverDpllTriadSimd<0>::InitVanillaByBand(puzzle, state);
        }
        vector<int> permutation = util_.Permutation(729);
        for (int literal : permutation) {
            int cell = literal / 9;

            // skip over clues that were given as part of the partial puzzle.
            if (pencilmark) {
                if (puzzle[literal] == '.') continue;
            } else {
                if (puzzle[cell] != '.') continue;
            }

            int row = cell / 9, col = cell % 9;
            int box_idx = (row / 3) * 3 + (col / 3);
            int elm_idx = (row % 3) * 4 + (col % 3);
            Box &box = state.boxen[box_idx];
            uint16_t candidates = box.cells.Extract(elm_idx);
            uint16_t candidate = 1u << (literal % 9u);

            // quick check that the candidate is not trivially included or excluded before
            // adding as a clue.
            if ((candidates & candidate) && (candidates != candidate)) {
                Cells16 restrict = box.cells;
                restrict.Insert(elm_idx, pencilmark ? candidates ^ candidate : candidate);
                State test_state = state;
                if (SolverDpllTriadSimd<0>::BoxRestrict<0>(test_state, box_idx, restrict)) {
                    int cell_or_literal = pencilmark ? literal : cell;
                    char prior_unconstrained_value = puzzle[cell_or_literal];
                    puzzle[cell_or_literal] = pencilmark ? '.' : (char)('1' + (literal % 9));
                    switch (solver_.SafeCountSolutionsConsistentWithPartialAssignment(
                            test_state, 2)) {
                        case 0:
                            puzzle[cell_or_literal] = prior_unconstrained_value;
                            continue;
                        case 1:
                            return true;
                        default:
                            state = test_state;
                            continue;
                    }
                }
            }
        }
        return false;
    }

    // minimizes a vanilla or pencilmark puzzle by testing removal of all clues in random order,
    // restoring any clue that's required to keep the solution unique. if the 'monotonic' flag
    // is passed, returns true only if we had a minimal puzzle after the first restored clue.
    bool Minimize(bool pencilmark, bool monotonic, char *puzzle) {
        bool restored_clue = false;
        vector<int> permutation = util_.Permutation(729);
        for (int cell_or_literal : permutation) {
            if (pencilmark) {
                if (puzzle[cell_or_literal] != '.') continue;
            } else {
                if (cell_or_literal >= 81 || puzzle[cell_or_literal] == '.') continue;
            }
            char constraint = puzzle[cell_or_literal];
            State state;
            if (pencilmark) {
                puzzle[cell_or_literal] = (char)('1' + (cell_or_literal % 9));
                SolverDpllTriadSimd<0>::InitPencilmarkByBox(puzzle, state);
            } else {
                puzzle[cell_or_literal] = '.';
                SolverDpllTriadSimd<0>::InitVanillaByBand(puzzle, state);
            }
            if (solver_.SafeCountSolutionsConsistentWithPartialAssignment(state, 2) > 1) {
                puzzle[cell_or_literal] = constraint;
                restored_clue = true;
            } else if (monotonic && restored_clue) {
                return false;
            }
        }
        return true;
    }
};


SolverDpllTriadSimd<0> solver_none{};
SolverDpllTriadSimd<1> solver_last{};
SolverDpllTriadSimd<2> solver_enum{};

GeneratorDpllTriadSimd generator{};

} // namespace

// configuration: bits 0..7 nonzero => return a solution even if limit > 1; bits 8..15 == 1 =>
// branch with the original heuristic (ChooseBandAndValueToBranch) instead of the branch-volume one.
extern "C"
size_t TdokuSolverDpllTriadSimd(const char *puzzle, size_t limit,
                                uint32_t configuration,
                                char *solution, size_t *num_guesses) {
    bool return_last = limit == 1 || (configuration & 0xffu) > 0;
    bool original_heuristic = ((configuration >> 8u) & 0xffu) == 1u;
    if (return_last) {
        solver_last.original_heuristic_ = original_heuristic;
        return solver_last.SolveSudoku(puzzle, limit, solution, num_guesses);
    } else {
        solver_none.original_heuristic_ = original_heuristic;
        return solver_none.SolveSudoku(puzzle, limit, solution, num_guesses);
    }
}

extern "C"
size_t TdokuEnumerate(const char *puzzle, size_t limit,
                      void (*callback)(const char *, void *), void *callback_arg) {
    solver_enum.callback_ = callback;
    solver_enum.callback_arg_ = callback_arg;
    return solver_enum.SolveSudoku(puzzle, limit, nullptr, nullptr);
}

extern "C"
bool TdokuConstrain(bool pencilmark, char *puzzle) {
    return generator.Constrain(pencilmark, puzzle);
}

extern "C"
bool TdokuMinimize(bool pencilmark, bool monotonic, char *puzzle) {
    return generator.Minimize(pencilmark, monotonic, puzzle);
}
