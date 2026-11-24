#include "bitutil.h"
#include "branch_features.h"
#include "simd_vectors.h"
#include "util.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

#ifdef TDOKU_STATS
#include <vector>
#include <x86intrin.h>
#include "tdoku_stats.h"
#ifndef TDOKU_EXPERIMENTAL_TU
TdokuStats g_tdoku_stats{};
std::vector<TdokuTraceRecord> g_tdoku_trace{};
bool g_tdoku_trace_enabled = false;
std::vector<TdokuDecisionRecord> g_tdoku_decisions{};
bool g_tdoku_decisions_enabled = false;
char g_oracle_solution[81];
bool g_oracle_enabled = false;
#endif
#define STATS(x) do { x; } while (0)
// solution-oracle bookkeeping around the two children of a decision (stats builds only)
#define ORACLE_BEGIN(b, vc, asserted) \
    bool o_parent_ = on_path_; bool o_pos_ = OracleBegin(b, vc, asserted); \
    size_t o_g0_ = num_guesses_; on_path_ = o_parent_ && o_pos_;
#define ORACLE_NEG() \
    if (o_parent_ && !o_pos_) { g_tdoku_stats.w_wrong_first += num_guesses_ - o_g0_; \
        g_tdoku_stats.immediate_refutations += num_guesses_ == o_g0_; } \
    on_path_ = o_parent_ && !o_pos_; size_t o_g1_ = num_guesses_;
#define ORACLE_END() \
    if (o_parent_ && o_pos_) { g_tdoku_stats.w_after_solution += num_guesses_ - o_g1_; \
        g_tdoku_stats.immediate_refutations += num_guesses_ == o_g1_; } \
    on_path_ = o_parent_;
#else
#define STATS(x) do { } while (0)
#define ORACLE_BEGIN(b, vc, asserted)
#define ORACLE_NEG()
#define ORACLE_END()
#endif

// Branching heuristics selectable via bits 8..15 of the solver configuration argument.
// See docs/BRANCHING_RESEARCH.md.
enum TdokuHeuristic : int {
    kHeuristicOriginal = 0,  // tdoku's original band-then-digit hierarchy (baseline)
    kHeuristicResearch = 1,  // scalar selector driven by g_research_params (search-tree studies)
    kHeuristicIxTwoSided = 2,  // optimized: flat n=2 class, two-sided intersection fractions
    kHeuristicIxTwoSidedSimd = 3,  // same choices as 2, branch-free AVX2 implementation
    kHeuristicIxGated = 4,  // id 3 when the gate below is open, otherwise the original
    // final selected variant (revision 3; report sections 12-15): open for the first 4
    // decisions and after a refutation since the last solution; a node whose mean candidate
    // score is >= 340 closes the gate for its subtree; closed at depth >= 8.
    kHeuristicIxFinal = 5,
};

// Compile-time gate of kHeuristicIxFinal (defaults = the selected configuration). The gate is
// open for the first TDOKU_FINAL_OPEN_DECISIONS decisions, after a refuted branch, and (if
// < 1000) whenever the total of remaining band configurations is >= TDOKU_FINAL_OPEN_TOTAL.
// With TDOKU_FINAL_RESET_ON_SOLUTION the refutation count restarts at every solution, so only
// refutations since the last solution open the gate (aimed at solution enumeration).
#ifndef TDOKU_FINAL_OPEN_DECISIONS
#define TDOKU_FINAL_OPEN_DECISIONS 4
#endif
#ifndef TDOKU_FINAL_OPEN_TOTAL
#define TDOKU_FINAL_OPEN_TOTAL 1000
#endif
#ifndef TDOKU_FINAL_RESET_ON_SOLUTION
#define TDOKU_FINAL_RESET_ON_SOLUTION 1
#endif
// close the gate for the subtree below a node whose candidate scores show a loose position
#ifndef TDOKU_FINAL_LOOSE_MIN
#define TDOKU_FINAL_LOOSE_MIN 0
#endif
#ifndef TDOKU_FINAL_LOOSE_MEAN
#define TDOKU_FINAL_LOOSE_MEAN 340
#endif
#ifndef TDOKU_FINAL_LOOSE_SLOPE
#define TDOKU_FINAL_LOOSE_SLOPE 0
#endif
#ifndef TDOKU_FINAL_LOOSE_FLOOR
#define TDOKU_FINAL_LOOSE_FLOOR 0
#endif
#ifndef TDOKU_FINAL_LOOSE_REOPEN
#define TDOKU_FINAL_LOOSE_REOPEN 0
#endif
// gate closed at depth >= this
#ifndef TDOKU_FINAL_MAX_DEPTH
#define TDOKU_FINAL_MAX_DEPTH 8
#endif

// Gate for kHeuristicIxGated (general solver-state criteria; runtime-tunable for research):
//   use the intersection-aware selection only if  failed_branches >= g_ix_gate_min_fails
//   (refutations so far in the current solve) and  total band configurations >= g_ix_gate_min_total.
//   The gate is always open for the first g_ix_gate_open_decisions decisions of a solve.
#ifndef TDOKU_EXPERIMENTAL_TU
int g_ix_gate_min_fails = 1;
int g_ix_gate_min_total = 0;
int g_ix_gate_open_decisions = 0;
int g_ix_gate_open_total = 1000;  // open whenever total configurations >= this
int g_ix_gate_reset_on_solution = 0;  // restart the refutation count at every solution
int g_ix_gate_loose_min = 0;     // subtree closed when the node's min candidate score >= this
int g_ix_gate_loose_mean = 0;    // ... or its mean candidate score >= this (0 = off)
int g_ix_gate_max_depth = 1000;  // gate closed at depth >= this
int g_ix_gate_open_depth = 0;    // gate open at depth < this (in addition to open_decisions)
int g_ix_gate_loose_slope = 0;   // mean threshold at depth d: clamp(loose_mean + slope * d, floor, 360)
int g_ix_gate_loose_floor = 0;
int g_ix_gate_loose_reopen = 0;  // a refutation inside a loose subtree reopens the gate there
#else
extern int g_ix_gate_loose_slope, g_ix_gate_loose_floor, g_ix_gate_loose_reopen;
extern int g_ix_gate_reset_on_solution, g_ix_gate_loose_min, g_ix_gate_loose_mean;
extern int g_ix_gate_max_depth, g_ix_gate_open_depth;
extern int g_ix_gate_min_fails, g_ix_gate_min_total, g_ix_gate_open_decisions;
extern int g_ix_gate_open_total;
#endif

// W[n] = 60 / n: exact integer weights for residual fractions k / n (lcm(1..6) = 60).
constexpr int kWeight60[7] = {0, 60, 30, 20, 15, 12, 10};

// The experimental heuristics are instantiated in a separate translation unit
// (solver_dpll_triad_simd_experimental.cc includes this file with TDOKU_EXPERIMENTAL_TU) so that
// their presence does not perturb the compiler's inlining decisions for the original solver.
#ifndef TDOKU_EXPERIMENTAL_TU
TdokuResearchParams g_research_params{1, 0.5, 1.0, 0, 0.0, 0};
#else
extern TdokuResearchParams g_research_params;
#endif

#define LIKELY(x) __builtin_expect(!!(x),1)

// Experimental heuristics pin the original solver's call structure (branch functions are real
// functions; per-node logic is inlined into them) so that the comparison with the original does
// not depend on the inliner's decisions for a differently shaped recursion (see the report).
#if defined(__GNUC__) && !defined(TDOKU_NO_EXP_ATTRS)
#define TDOKU_EXP_NOINLINE __attribute__((noinline))
#define TDOKU_EXP_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define TDOKU_EXP_NOINLINE
#define TDOKU_EXP_ALWAYS_INLINE inline
#endif

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

}  // namespace
// initialization compiled in the original translation unit (State has the same definition in
// every translation unit that includes this file)
namespace { struct State; }
extern "C" bool TdokuInitState(const char *input, bool pencilmark, void *state);
namespace {
inline bool TdokuInitState(const char *input, bool pencilmark, State *state) {
    return ::TdokuInitState(input, pencilmark, (void *) state);
}

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

    // lane selection masks for asserting a specific configuration lane.
    const Cells08 config_lane_mask[6]{
            {0xffff, 0, 0, 0, 0, 0, 0, 0}, {0, 0xffff, 0, 0, 0, 0, 0, 0},
            {0, 0, 0xffff, 0, 0, 0, 0, 0}, {0, 0, 0, 0xffff, 0, 0, 0, 0},
            {0, 0, 0, 0, 0xffff, 0, 0, 0}, {0, 0, 0, 0, 0, 0xffff, 0, 0}};

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

template<int solution_mode, int heuristic = kHeuristicOriginal>
struct SolverDpllTriadSimd {
    State solution_{};
    size_t limit_ = 1;
    size_t num_solutions_ = 0;
    size_t num_guesses_ = 0;
    size_t num_fails_ = 0;  // refuted branches in the current solve (used by gated heuristics)
    int depth_ = 0;         // decisions on the current path (gated heuristics)
    uint8_t loose_[128]{};  // loose_[k]: node at depth k found (or inherited) a loose position
    uint32_t loose_fails_[128]{};  // num_fails_ when that looseness was established

    struct DepthGuard {
        int &d;
        explicit DepthGuard(int &x) : d(x) { ++d; }
        ~DepthGuard() { --d; }
    };
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
        // the approach below is faster than actually counting the configuration for each digit and
        // using MinPosGreaterThanOrEqual as above, but it is inexact in rare cases when all digits
        // have 4 or more configurations. the tradeoff is a net positive for Vanilla Sudoku and a
        // net negative for Pencilmark Sudoku.
        if ((config_minpos & 0xff00u) == 0) {
            best_band = config_minpos >> 16u;
            const auto &configurations =
                    state.bands[tables.div3[best_band]][tables.mod3[best_band]].configurations;
            Cells08 one = configurations;
            Cells08 shuffle_rotate = Cells08{shuf01, shuf02, shuf03, shuf04, shuf05, shuf00, 0xffff, 0xffff};
            Cells08 rotated = one.Shuffle(shuffle_rotate); // 1
            Cells08 two = one & rotated;
            one |= rotated;
            rotated = rotated.Shuffle(shuffle_rotate); // 2
            Cells08 three = two & rotated;
            two |= one & rotated;
            one |= rotated;
            rotated = rotated.Shuffle(shuffle_rotate); // 3
            Cells08 four = three & rotated;
            three |= two & rotated;
            two |= one & rotated;
            one |= rotated;
            rotated = rotated.Shuffle(shuffle_rotate); // 4
            four |= three & rotated;
            three |= two & rotated;
            two |= one & rotated;
            one |= rotated;
            rotated = rotated.Shuffle(shuffle_rotate); // 5
            four |= three & rotated;
            three |= two & rotated;
            two |= one & rotated;

            Cells08 only_two = two.and_not(three);
            if (!LIKELY(only_two.AllZero())) {
                return {best_band, only_two.GetLowBit()};
            } else {
                Cells08 only_three = three.and_not(four);
                if (LIKELY(!only_three.AllZero())) {
                    return {best_band, only_three.GetLowBit()};
                } else {
                    return {best_band, four.GetLowBit()};
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
#ifdef TDOKU_STATS
        TraceBegin(state, vertical * 3 + band_idx, value_configurations,
                   value_configurations ^ assignment_elims);
        ORACLE_BEGIN(vertical * 3 + band_idx, value_configurations,
                     value_configurations ^ assignment_elims)
        bool ok = BandEliminate<vertical>(state_copy, band_idx);
        TraceEnd(state_copy, ok);
        if (ok) {
#else
        if (BandEliminate<vertical>(state_copy, band_idx)) {
#endif
            CountSolutionsConsistentWithPartialAssignment(state_copy);
            if (num_solutions_ == limit_) return;
        }
        // now negate the first configuration
#ifdef TDOKU_STATS
        ORACLE_NEG()
#endif
        Cells08 negation_elims = value_configurations ^ assignment_elims;
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (BandEliminate<vertical>(state, band_idx)) {
            CountSolutionsConsistentWithPartialAssignment(state);
        }
#ifdef TDOKU_STATS
        else g_tdoku_stats.fails++;
        ORACLE_END()
#endif
    }

    // The original branching code plus refutation counting, used by gated heuristics when the
    // gate is closed (identical decisions to BranchOnBandAndValue; kept separate so the original
    // solver's code is untouched).
    template<int vertical>
    TDOKU_EXP_NOINLINE
    void BranchOnBandAndValueCounting(int band_idx, const Cells08 &value_mask, State &state) {
        DepthGuard depth_guard(depth_);
        Band &band = state.bands[vertical][band_idx];
        Cells08 value_configurations = band.configurations & value_mask;
        num_guesses_++;
        State state_copy = state;
        Cells08 assignment_elims = value_configurations.ClearLowBit();
        state_copy.bands[vertical][band_idx].eliminations |= assignment_elims;
#ifdef TDOKU_STATS
        TraceBegin(state, vertical * 3 + band_idx, value_configurations,
                   value_configurations ^ assignment_elims);
        ORACLE_BEGIN(vertical * 3 + band_idx, value_configurations,
                     value_configurations ^ assignment_elims)
        bool ok = BandEliminate<vertical>(state_copy, band_idx);
        TraceEnd(state_copy, ok);
#else
        bool ok = BandEliminate<vertical>(state_copy, band_idx);
#endif
        if (ok) {
            CountSolutionsConsistentWithPartialAssignment(state_copy);
            if (num_solutions_ == limit_) return;
        } else {
            num_fails_++;
        }
        ORACLE_NEG()
        Cells08 negation_elims = value_configurations ^ assignment_elims;
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (BandEliminate<vertical>(state, band_idx)) {
            CountSolutionsConsistentWithPartialAssignment(state);
        } else {
            num_fails_++;
            STATS(g_tdoku_stats.fails++);
        }
        ORACLE_END()
    }

    // Branch on a specific (band, digit, configuration) tuple: first the positive branch in which
    // `config` is asserted for `digit` in the band, then the negative branch in which it is
    // eliminated. Semantically identical to BranchOnBandAndValue except for the choice of config.
    template<int vertical>
    TDOKU_EXP_NOINLINE
    void BranchOnConfig(int band_idx, int digit, int config, State &state) {
        DepthGuard depth_guard(depth_);
        Band &band = state.bands[vertical][band_idx];
        Cells08 value_configurations = band.configurations & tables.one_value_mask[digit];
        num_guesses_++;
        State state_copy = state;
        Cells08 assignment_elims = value_configurations.and_not(tables.config_lane_mask[config]);
        state_copy.bands[vertical][band_idx].eliminations |= assignment_elims;
#ifdef TDOKU_STATS
        TraceBegin(state, vertical * 3 + band_idx, value_configurations,
                   value_configurations ^ assignment_elims);
        ORACLE_BEGIN(vertical * 3 + band_idx, value_configurations,
                     value_configurations ^ assignment_elims)
#endif
        bool ok = BandEliminate<vertical>(state_copy, band_idx);
#ifdef TDOKU_STATS
        TraceEnd(state_copy, ok);
#endif
        if (ok) {
            CountSolutionsConsistentWithPartialAssignment(state_copy);
            if (num_solutions_ == limit_) return;
        } else {
            num_fails_++;
        }
        ORACLE_NEG()
        Cells08 negation_elims = value_configurations & tables.config_lane_mask[config];
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (BandEliminate<vertical>(state, band_idx)) {
            CountSolutionsConsistentWithPartialAssignment(state);
        } else {
            num_fails_++;
            STATS(g_tdoku_stats.fails++);
        }
        ORACLE_END()
    }

    ///////////////////////////////////////////////////////////////////////////////////
    // Research selector (scalar, runtime parameterized). Intended for measuring search-tree
    // effects of alternative selection rules, not for speed.

    struct Choice {
        uint32_t band;  // 0..5 (3 * vertical + idx), or NONE if every band is fixed
        int digit;
        int config;
        // AM2 score aggregates over the candidate pairs (x60, max 360); set by the AVX2
        // intersection selector only (0 elsewhere, which never counts as 'loose').
        uint16_t score_min = 0;
        uint16_t score_mean = 0;
    };

    static void ToRaw(const State &state, tdoku_ix::RawState &raw) {
        static_assert(sizeof(Cells16) == 32 && sizeof(Cells08) == 16, "unexpected layout");
        for (int b = 0; b < 9; b++) memcpy(raw.box[b], &state.boxen[b].cells, 32);
        for (int b = 0; b < 6; b++) {
            memcpy(raw.band[b], &state.bands[b / 3][b % 3].configurations, 16);
        }
    }

    static int TotalConfigs(const State &state) {
        int total = 0;
        for (int b = 0; b < 6; b++) total += state.bands[b / 3][b % 3].configurations.Popcount();
        return total;
    }

    // Number of configurations eliminated (across all bands) by asserting (elims) in band b,
    // or -1 if propagation fails.
    static int Lookahead(const State &state, int b, const Cells08 &elims, int before) {
        State copy = state;
        copy.bands[b / 3][b % 3].eliminations |= elims;
        bool ok = b < 3 ? BandEliminate<0>(copy, b % 3) : BandEliminate<1>(copy, b % 3);
        if (!ok) return -1;
        return before - TotalConfigs(copy);
    }

    static double PairKey(const TdokuResearchParams &P, int b, int n, int total) {
        switch (P.pair_rule) {
            case 0: return 0.0;
            // the original picks the lowest-index band among those with minimal total *before*
            // considering digit classes, so the band index is part of the key.
            case 1: return total * 32.0 + b * 4.0 + (n == 2 ? 0 : n == 3 ? 1 : 2);
            case 2: return total * 8.0 + n;
            case 3: return n;
            case 4: return n * 64.0 + total;
            case 5: {
                double a = n, bt = total / 9.0, w = P.soft_w, p = P.soft_p;
                if (p == 0.0) return exp(w * log(a) + (1 - w) * log(bt));
                return pow(w * pow(a, p) + (1 - w) * pow(bt, p), 1.0 / p);
            }
            default: return 0.0;
        }
    }

    static double GenMean(const double q[3], double p) {
        if (p <= -100) return fmin(q[0], fmin(q[1], q[2]));
        if (p <= 0.0 && (q[0] == 0.0 || q[1] == 0.0 || q[2] == 0.0)) return 0.0;
        if (p == 0.0) return cbrt(q[0] * q[1] * q[2]);
        return pow((pow(q[0], p) + pow(q[1], p) + pow(q[2], p)) / 3.0, 1.0 / p);
    }

    static Choice ChooseResearch(const State &state) {
        return ChooseResearchWith(state, g_research_params);
    }

    static Choice ChooseResearchWith(const State &state, const TdokuResearchParams &P) {
        using namespace tdoku_ix;
        RawState rs;
        ToRaw(state, rs);
        uint8_t cfg[6][9], n[6][9];
        int total[6];
        bool any = false;
        for (int b = 0; b < 6; b++) {
            total[b] = 0;
            for (int c = 0; c < 6; c++) total[b] += __builtin_popcount(rs.band[b][c]);
            any |= total[b] >= 10;
            for (int d = 0; d < 9; d++) {
                cfg[b][d] = (uint8_t) ConfigMask(rs, b, d);
                n[b][d] = (uint8_t) Popcount8(cfg[b][d]);
            }
        }
        if (!any) return {NONE, 0, 0};

        // rank pairs; eligible pairs live in unfixed bands (preserving the original termination
        // test) and have two or more configurations.
        double key[6][9];
        double best_key = std::numeric_limits<double>::infinity();
        for (int b = 0; b < 6; b++) {
            for (int d = 0; d < 9; d++) {
                key[b][d] = std::numeric_limits<double>::infinity();
                if (total[b] < 10 || n[b][d] < 2) continue;
                key[b][d] = PairKey(P, b, n[b][d], total[b]);
                if (key[b][d] < best_key) best_key = key[b][d];
            }
        }
        const double eps = 1e-9;
        auto in_class = [&](int b, int d) { return key[b][d] <= best_key + eps; };

        Choice best{NONE, 0, 0};
        if (P.ix_score == 0 || P.ix_scope == 1) {
            for (int b = 0; b < 6 && best.band == NONE; b++) {
                for (int d = 0; d < 9; d++) {
                    if (in_class(b, d)) {
                        best = {(uint32_t) b, d, __builtin_ctz(cfg[b][d])};
                        break;
                    }
                }
            }
            if (P.ix_score == 0) return best;
        }

        int before = P.ix_score == 6 ? TotalConfigs(state) : 0;
        double best_score = -std::numeric_limits<double>::infinity();
        Choice first = best;
        for (int b = 0; b < 6; b++) {
            for (int d = 0; d < 9; d++) {
                if (!in_class(b, d)) continue;
                if (P.ix_scope == 1 && !(first.band == (uint32_t) b && first.digit == d)) continue;
                uint8_t M[3][3];
                CompatMasks(rs, b, d, M);
                int ny[3];
                for (int y = 0; y < 3; y++) ny[y] = n[OppositeBand(b, y)][d];
                for (uint32_t rest = cfg[b][d]; rest; rest &= rest - 1) {
                    int pi = __builtin_ctz(rest);
                    double score;
                    if (P.ix_score == 6) {
                        Cells08 value_cfgs = state.bands[b / 3][b % 3].configurations &
                                             tables.one_value_mask[d];
                        int dpos = Lookahead(state, b,
                                value_cfgs.and_not(tables.config_lane_mask[pi]), before);
                        int dneg = Lookahead(state, b,
                                value_cfgs & tables.config_lane_mask[pi], before);
                        if (dpos < 0 || dneg < 0) score = 1e18;
                        else score = (1.0 + dpos) * (1.0 + dneg);
                    } else {
                        int k[3];
                        double q[3];
                        for (int y = 0; y < 3; y++) {
                            k[y] = Popcount8(M[y][kElem[pi][y]]);
                            q[y] = ny[y] > 0 ? (double) k[y] / ny[y] : 0.0;
                        }
                        switch (P.ix_score) {
                            case 1: score = -(double) (k[0] + k[1] + k[2]); break;
                            case 2: score = -GenMean(q, P.ix_p); break;
                            case 3: score = (1.0 - q[0] * q[1] * q[2]) / n[b][d]; break;
                            case 7:
                            case 8: {
                                // two-sided, fraction based: summed residual fractions of the
                                // positive and (estimated) negative branch, in exact integer
                                // arithmetic scaled by 60 = lcm(1..6).
                                uint32_t others = cfg[b][d] & ~(1u << pi);
                                int pos = 0, neg = 0;
                                for (int y = 0; y < 3; y++) {
                                    pos += k[y] * kWeight60[ny[y]];
                                    neg += Popcount8(NegativeCompatMask(M, y, others)) *
                                           kWeight60[ny[y]];
                                }
                                score = P.ix_score == 7 ? -(double) (pos + neg)
                                                        : -(double) std::max(pos, neg);
                                break;
                            }
                            case 9: case 10: case 11: case 12: case 13: case 14: case 15:
                            case 16: case 17: case 18: {
                                // related two-sided scores over residual fractions q+ (asserting
                                // pi) and q- (eliminating pi), see branch_features.h
                                uint32_t others = cfg[b][d] & ~(1u << pi);
                                double qp[3], qn[3], cp[3], cn[3];
                                int informative = 0;
                                for (int y = 0; y < 3; y++) {
                                    int kn = Popcount8(NegativeCompatMask(M, y, others));
                                    qp[y] = ny[y] ? (double) k[y] / ny[y] : 1.0;
                                    qn[y] = ny[y] ? (double) kn / ny[y] : 1.0;
                                    informative += ny[y] >= 2;
                                    // own-band cell residuals in the box shared with band y
                                    int cnt[3], all = 0;
                                    for (int e = 0; e < 3; e++) {
                                        cnt[e] = Popcount8(LocalMask(rs, b, y, e, d));
                                        all += cnt[e];
                                    }
                                    int ep = kElem[pi][y], en = 0;
                                    for (int e = 0; e < 3; e++) {
                                        if (others & kElemTables.configs_using_elem[y][e]) en += cnt[e];
                                    }
                                    cp[y] = all ? (double) cnt[ep] / all : 1.0;
                                    cn[y] = all ? (double) en / all : 1.0;
                                }
                                double am_p = qp[0] + qp[1] + qp[2], am_n = qn[0] + qn[1] + qn[2];
                                double cell = cp[0] + cp[1] + cp[2] + cn[0] + cn[1] + cn[2];
                                double lam = P.ix_p;
                                switch (P.ix_score) {
                                    case 9: {  // GM2: sum of log residual fractions
                                        double t = 0;
                                        for (int y = 0; y < 3; y++) {
                                            t += log(fmax(qp[y], 1e-9)) + log(fmax(qn[y], 1e-9));
                                        }
                                        score = -t;
                                        break;
                                    }
                                    case 10: {  // SQ2 (lam = 2) / SQRT2 (lam = 0.5): sum q^lam
                                        double t = 0;
                                        for (int y = 0; y < 3; y++) t += pow(qp[y], lam) + pow(qn[y], lam);
                                        score = -t;
                                        break;
                                    }
                                    case 11:  // PROD2: product of the children's summed residuals
                                        score = -(am_p * am_n);
                                        break;
                                    case 12: {  // INF2: only opposite bands where d is unfixed
                                        double t = 0;
                                        for (int y = 0; y < 3; y++) if (ny[y] >= 2) t += qp[y] + qn[y];
                                        score = informative ? -t / informative : -2.0;
                                        break;
                                    }
                                    case 13:  // AM2 + lam * band total / 9
                                        score = -(am_p + am_n) - lam * total[b] / 9.0;
                                        break;
                                    case 14:  // CELL2: own-band candidate-cell residuals only
                                        score = -cell;
                                        break;
                                    case 15:  // AM2 + lam * CELL2
                                    default:
                                        score = -(am_p + am_n) - lam * cell;
                                        break;
                                    case 17: {  // EXC2: residual excess (k - 1) / (n_y - 1)
                                        double t = 0;
                                        for (int y = 0; y < 3; y++) {
                                            if (ny[y] < 2) { t += 2.0; continue; }
                                            int kn = Popcount8(NegativeCompatMask(M, y, others));
                                            t += (double) (k[y] - 1) / (ny[y] - 1) +
                                                 (double) (kn - 1) / (ny[y] - 1);
                                        }
                                        score = -t;
                                        break;
                                    }
                                    case 18: {  // FORCE2: AM2 - lam * #(branch fixes band y)
                                        int forced = 0;
                                        for (int y = 0; y < 3; y++) {
                                            if (ny[y] < 2) continue;
                                            int kn = Popcount8(NegativeCompatMask(M, y, others));
                                            forced += (k[y] == 1) + (kn == 1);
                                        }
                                        score = -(am_p + am_n) + lam * forced;
                                        break;
                                    }
                                    case 16: {  // AM2 restricted to opposite bands, weighting
                                        // each band by its own uncertainty log2(n_y)
                                        double t = 0;
                                        for (int y = 0; y < 3; y++) {
                                            double w = ny[y] >= 2 ? log2((double) ny[y]) : 0.0;
                                            t += w * (qp[y] + qn[y]);
                                        }
                                        score = -t;
                                        break;
                                    }
                                }
                                break;
                            }
                            default: {
                                uint32_t others = cfg[b][d] & ~(1u << pi);
                                int dpos = n[b][d] - 1, dneg = 1;
                                for (int y = 0; y < 3; y++) {
                                    dpos += ny[y] - k[y];
                                    dneg += ny[y] - Popcount8(NegativeCompatMask(M, y, others));
                                }
                                score = P.ix_score == 4 ? (1.0 + dpos) * (1.0 + dneg)
                                                        : (double) (dpos + dneg);
                            }
                        }
                    }
                    if (score > best_score) {
                        best_score = score;
                        best = {(uint32_t) b, d, pi};
                    }
                }
            }
        }
        return best;
    }

#ifdef TDOKU_STATS
    int stats_depth_ = 0;
    bool on_path_ = true;
    uint64_t stats_fails_at_solution_ = 0;

    // configuration of digit d in band b in the oracle solution
    static int SolutionConfig(int b, int d) {
        int o = b / 3, x = b % 3, elem[3];
        for (int p = 0; p < 3; p++) {
            int bi = o == 0 ? x : p, bj = o == 0 ? p : x;
            for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) {
                if (g_oracle_solution[(bi * 3 + r) * 9 + bj * 3 + c] == '1' + d) elem[p] = o == 0 ? r : c;
            }
        }
        for (int c = 0; c < 6; c++) {
            if (tdoku_ix::kElem[c][0] == elem[0] && tdoku_ix::kElem[c][1] == elem[1] &&
                tdoku_ix::kElem[c][2] == elem[2]) return c;
        }
        return -1;
    }

    // returns whether the positive branch contains the solution (only meaningful on the path)
    bool OracleBegin(int b, const Cells08 &value_configurations, const Cells08 &asserted) {
        if (!g_oracle_enabled) return false;
        if (!on_path_) { g_tdoku_stats.offpath_decisions++; return false; }
        uint16_t lanes[8], vl[8];
        memcpy(lanes, &asserted, 16);
        memcpy(vl, &value_configurations, 16);
        int digit = 0, config = -1;
        for (int c = 0; c < 6; c++) {
            if (vl[c]) digit = __builtin_ctz(vl[c]);
            if (lanes[c]) config = c;
        }
        g_tdoku_stats.path_decisions++;
        bool pos = config == SolutionConfig(b, digit);
        g_tdoku_stats.path_correct_first += pos;
        return pos;
    }

    size_t DecisionBegin(const State &state) {
        if (!g_tdoku_decisions_enabled) return SIZE_MAX;
        using namespace tdoku_ix;
        TdokuDecisionRecord r{};
        r.depth = (uint16_t) stats_depth_;
        r.decisions_before = (uint32_t) num_guesses_;
        r.fails_before = (uint32_t) g_tdoku_stats.fails;
        r.solutions_before = (uint32_t) num_solutions_;
        r.total_configs = (uint16_t) TotalConfigs(state);
        RawState rs;
        ToRaw(state, rs);
        int n2 = 0, unfixed = 0;
        for (int b = 0; b < 6; b++) {
            int total = 0;
            for (int c = 0; c < 6; c++) total += __builtin_popcount(rs.band[b][c]);
            if (total < 10) continue;
            unfixed++;
            for (int d = 0; d < 9; d++) n2 += Popcount8(ConfigMask(rs, b, d)) == 2;
        }
        r.unfixed_bands = (uint8_t) unfixed;
        r.n2_pairs = (uint8_t) n2;
        r.fails_since_solution = (uint32_t) (g_tdoku_stats.fails - stats_fails_at_solution_);
        r.refutations = (uint32_t) g_tdoku_stats.fails;  // start value; turned into a delta at End
        // exact AM2 scores (x60) of all candidate pairs
        int smin = 1 << 30, smax = 0, ssum = 0, ncand = 0;
        for (int b = 0; b < 6; b++) {
            int total = 0;
            for (int c = 0; c < 6; c++) total += __builtin_popcount(rs.band[b][c]);
            if (total < 10) continue;
            for (int d = 0; d < 9; d++) {
                uint32_t cfg = ConfigMask(rs, b, d);
                if (Popcount8(cfg) != 2) continue;
                uint8_t M[3][3];
                CompatMasks(rs, b, d, M);
                int score = 0;
                for (int y = 0; y < 3; y++) {
                    int ny = Popcount8(ConfigMask(rs, OppositeBand(b, y), d));
                    for (uint32_t rest = cfg; rest; rest &= rest - 1) {
                        score += kWeight60[ny] * Popcount8(M[y][kElem[__builtin_ctz(rest)][y]]);
                    }
                }
                smin = std::min(smin, score); smax = std::max(smax, score);
                ssum += score; ncand++;
            }
        }
        r.n_candidates = (uint16_t) ncand;
        r.score_min = (uint16_t) (ncand ? smin : 0);
        r.score_max = (uint16_t) smax;
        r.score_mean = (uint16_t) (ncand ? ssum / ncand : 0);
        g_tdoku_decisions.push_back(r);
        stats_depth_++;
        return g_tdoku_decisions.size() - 1;
    }

    void DecisionEnd(size_t idx, size_t guesses_before) {
        if (idx == SIZE_MAX) return;
        stats_depth_--;
        g_tdoku_decisions[idx].subtree = (uint32_t) (num_guesses_ - guesses_before);
        g_tdoku_decisions[idx].refutations =
                (uint32_t) (g_tdoku_stats.fails - g_tdoku_decisions[idx].refutations);
    }

    // Record, for the positive branch about to be explored, the estimated number of
    // configurations remaining in the three intersecting bands (k_y) and, after propagation,
    // the actual number. Also checks the soundness property  actual_y <= k_y  (if no conflict).
    TdokuTraceRecord pending_{};
    int pending_band_ = 0, pending_digit_ = 0;

    void TraceBegin(const State &state, int b, const Cells08 &value_configurations,
                    const Cells08 &asserted) {
        using namespace tdoku_ix;
        g_tdoku_stats.decisions++;
        int nconf = value_configurations.Popcount();
        g_tdoku_stats.n_hist[nconf < 7 ? nconf : 0]++;
        if (!g_tdoku_trace_enabled) return;
        RawState rs;
        ToRaw(state, rs);
        // recover digit and asserted (lowest remaining after elims) config from the state copy
        // made by the caller: the caller asserts one lane; we find it after propagation in End.
        uint16_t lanes[8];
        memcpy(lanes, &value_configurations, 16);
        int digit = 0;
        uint32_t cfgmask = 0;
        for (int c = 0; c < 6; c++) {
            if (lanes[c]) { digit = __builtin_ctz(lanes[c]); cfgmask |= 1u << c; }
        }
        pending_band_ = b;
        pending_digit_ = digit;
        pending_ = TdokuTraceRecord{};
        pending_.n = (uint8_t) nconf;
        uint8_t M[3][3];
        CompatMasks(rs, b, digit, M);
        for (int y = 0; y < 3; y++) {
            pending_.ny[y] = (uint8_t) Popcount8(ConfigMask(rs, OppositeBand(b, y), digit));
            // store compat counts for all elements; the asserted config is resolved in End
            for (int e = 0; e < 3; e++) pending_.K[y][e] = (uint8_t) Popcount8(M[y][e]);
        }
        pending_.total_before = (uint16_t) TotalConfigs(state);
        pending_.cfgmask = (uint8_t) cfgmask;
        uint16_t alanes[8];
        memcpy(alanes, &asserted, 16);
        pending_.asserted = -1;
        for (int c = 0; c < 6; c++) if (alanes[c]) pending_.asserted = (int8_t) c;
    }

    void TraceEnd(const State &copy, bool ok) {
        using namespace tdoku_ix;
        if (!ok) g_tdoku_stats.fails++;
        if (!g_tdoku_trace_enabled) return;
        int b = pending_band_, d = pending_digit_;
        pending_.ok = ok;
        if (ok) {
            RawState rs;
            ToRaw(copy, rs);
            for (int y = 0; y < 3; y++) {
                pending_.ny_after[y] = (uint8_t) Popcount8(ConfigMask(rs, OppositeBand(b, y), d));
                if (pending_.asserted >= 0) {
                    g_tdoku_stats.soundness_checks++;
                    if (pending_.ny_after[y] > pending_.K[y][kElem[pending_.asserted][y]]) {
                        g_tdoku_stats.soundness_violations++;
                    }
                }
            }
            pending_.total_after = (uint16_t) TotalConfigs(copy);
        }
        g_tdoku_trace.push_back(pending_);
    }
#endif


    // do not call this twice on the same state. for efficiency this count may modify the
    // given state instead of making copies. if called with limit > 1 this can leave the state
    // changed in a way that makes subsequent calls return different results.
    void CountSolutionsConsistentWithPartialAssignment(State &state) {
        // the original heuristic keeps the original code path verbatim (separate overload) so
        // that its generated code and inlining decisions are unaffected by the experiments.
        CountSolutionsImpl(state, std::integral_constant<bool, heuristic == kHeuristicOriginal>());
    }

    void CountSolutionsImpl(State &state, std::true_type /*original*/) {
        STATS(g_tdoku_stats.nodes++);
#ifdef TDOKU_STATS
        uint64_t t0 = __rdtsc();
#endif
        auto band_and_value = ChooseBandAndValueToBranch(state);
#ifdef TDOKU_STATS
        g_tdoku_stats.select_cycles += __rdtsc() - t0;
#endif
        if (band_and_value.first == NONE) {
            num_solutions_++;
#ifdef TDOKU_STATS
            stats_fails_at_solution_ = g_tdoku_stats.fails;
#endif
            if (solution_mode == 1 && num_solutions_ == limit_) solution_ = state;
            if (solution_mode == 2) ReportSolution(state);
        } else {
            if (band_and_value.first < 3) {
                {
#ifdef TDOKU_STATS
                size_t rec_ = DecisionBegin(state), g0_ = num_guesses_;
#endif
                BranchOnBandAndValue<0>(
                        tables.mod3[band_and_value.first], band_and_value.second, state);
#ifdef TDOKU_STATS
                DecisionEnd(rec_, g0_);
#endif
            }
            } else {
                {
#ifdef TDOKU_STATS
                size_t rec_ = DecisionBegin(state), g0_ = num_guesses_;
#endif
                BranchOnBandAndValue<1>(
                        tables.mod3[band_and_value.first], band_and_value.second, state);
#ifdef TDOKU_STATS
                DecisionEnd(rec_, g0_);
#endif
            }
            }
        }
    }

    TDOKU_EXP_ALWAYS_INLINE
    void CountSolutionsImpl(State &state, std::false_type /*experimental*/) {
        STATS(g_tdoku_stats.nodes++);
#ifdef TDOKU_STATS
        uint64_t t0 = __rdtsc();
#endif
        // the original's cheap choice first: solved leaves and closed-gate nodes then follow
        // exactly the original code path.
        auto band_and_value = ChooseBandAndValueToBranch(state);
        if (band_and_value.first == NONE) {
#ifdef TDOKU_STATS
            g_tdoku_stats.select_cycles += __rdtsc() - t0;
#endif
            num_solutions_++;
#ifdef TDOKU_STATS
            stats_fails_at_solution_ = g_tdoku_stats.fails;
#endif
            if (heuristic == kHeuristicIxFinal && TDOKU_FINAL_RESET_ON_SOLUTION) num_fails_ = 0;
            if (heuristic == kHeuristicIxGated && g_ix_gate_reset_on_solution) num_fails_ = 0;
            if (solution_mode == 1 && num_solutions_ == limit_) solution_ = state;
            if (solution_mode == 2) ReportSolution(state);
            return;
        }
        bool use_new = true;
        if (heuristic == kHeuristicIxFinal) {
            use_new = num_guesses_ < TDOKU_FINAL_OPEN_DECISIONS || num_fails_ >= 1;
            if (TDOKU_FINAL_OPEN_TOTAL < 1000 && !use_new) {
                use_new = TotalConfigs(state) >= TDOKU_FINAL_OPEN_TOTAL;
            }
            if (depth_ >= TDOKU_FINAL_MAX_DEPTH) use_new = false;
        } else if (heuristic == kHeuristicIxGated) {
            int total = TotalConfigs(state);
            use_new = num_guesses_ < (size_t) g_ix_gate_open_decisions ||
                      depth_ < g_ix_gate_open_depth ||
                      total >= g_ix_gate_open_total ||
                      (num_fails_ >= (size_t) g_ix_gate_min_fails && total >= g_ix_gate_min_total);
            if (depth_ >= g_ix_gate_max_depth) use_new = false;
        }
        // looseness inherited from the parent closes the gate for the whole subtree
        bool inherited_loose = false;
        if (heuristic == kHeuristicIxGated || heuristic == kHeuristicIxFinal) {
            int k = depth_ < 127 ? depth_ : 127;
            inherited_loose = k > 0 && loose_[k - 1];
            bool reopen = heuristic == kHeuristicIxGated ? g_ix_gate_loose_reopen
                                                         : TDOKU_FINAL_LOOSE_REOPEN;
            if (inherited_loose && reopen && num_fails_ != loose_fails_[k - 1]) {
                inherited_loose = false;  // refuted since the position looked loose
            }
            if (inherited_loose) {
                use_new = false;
                loose_fails_[k] = loose_fails_[k - 1];
            }
            loose_[k] = inherited_loose;
        }
#ifdef TDOKU_STATS
        size_t rec_ = DecisionBegin(state), g0_ = num_guesses_;
#endif
        if (use_new) {
            Choice choice = Choose(state);  // never NONE here: an unfixed band exists
            if (heuristic == kHeuristicIxGated) {
                int k = depth_ < 127 ? depth_ : 127;
                int t = g_ix_gate_loose_mean + g_ix_gate_loose_slope * depth_;
                t = t < g_ix_gate_loose_floor ? g_ix_gate_loose_floor : t > 360 ? 360 : t;
                loose_[k] = (g_ix_gate_loose_min && choice.score_min >= g_ix_gate_loose_min) ||
                            (g_ix_gate_loose_mean && choice.score_mean >= t);
                loose_fails_[k] = (uint32_t) num_fails_;
            } else if (heuristic == kHeuristicIxFinal) {
                int k = depth_ < 127 ? depth_ : 127;
                int t = TDOKU_FINAL_LOOSE_MEAN + TDOKU_FINAL_LOOSE_SLOPE * depth_;
                t = t < TDOKU_FINAL_LOOSE_FLOOR ? TDOKU_FINAL_LOOSE_FLOOR : t > 360 ? 360 : t;
                loose_[k] = (TDOKU_FINAL_LOOSE_MIN && choice.score_min >= TDOKU_FINAL_LOOSE_MIN) ||
                            (TDOKU_FINAL_LOOSE_MEAN && choice.score_mean >= t);
                loose_fails_[k] = (uint32_t) num_fails_;
            }
#ifdef TDOKU_STATS
            g_tdoku_stats.select_cycles += __rdtsc() - t0;
#endif
            if (choice.band < 3) {
                BranchOnConfig<0>(choice.band, choice.digit, choice.config, state);
            } else {
                BranchOnConfig<1>(choice.band - 3, choice.digit, choice.config, state);
            }
        } else {
#ifdef TDOKU_STATS
            g_tdoku_stats.select_cycles += __rdtsc() - t0;
#endif
            if (band_and_value.first < 3) {
                BranchOnBandAndValueCounting<0>(
                        tables.mod3[band_and_value.first], band_and_value.second, state);
            } else {
                BranchOnBandAndValueCounting<1>(
                        tables.mod3[band_and_value.first], band_and_value.second, state);
            }
        }
#ifdef TDOKU_STATS
        DecisionEnd(rec_, g0_);
#endif
    }

    ///////////////////////////////////////////////////////////////////////////////////
    // Optimized two-sided intersection-aware selection (heuristic id 2).
    //
    // Equivalent to the research selector with pair=3 (flat: minimal n over unfixed bands),
    // ix=7 (two-sided summed residual fractions). For an n=2 pair (b, d) with configurations
    // {p1, p2} the research score is  -sum_y W[n_y] * (k_y(p1) + k_y(p2)),  W[n] = 60 / n, and
    //     k_y(p1) + k_y(p2) = B(box(b, y), d),
    //     B(i, j, d) = sum_{pi in cfg(H_i,d)} sum_{sigma in cfg(V_j,d)}
    //                      [cell(row(pi, j), col(sigma, i)) of box (i, j) has d],
    // which is symmetric in the roles of H_i and V_j, so one value per (box, digit) serves the
    // horizontal and the vertical pairs through that box. B is computed lazily only for the
    // (box, digit) combinations required by candidate pairs. When no n=2 pair exists (rare)
    // we fall back to the generic research implementation with the same parameters.

    static inline Choice ChooseIxTwoSided(const State &state) {
        using namespace tdoku_ix;
        uint32_t unfixed = 0;
        for (int b = 0; b < 6; b++) {
            unfixed |= (uint32_t) (state.bands[b / 3][b % 3].configurations.Popcount() >= 10) << b;
        }
        if (!unfixed) return {NONE, 0, 0};

        // digit-major configuration masks: cfg[b][d] = 6-bit set of configurations
        uint8_t cfg[6][9];
        TransposeBandConfigs(state, cfg);

        // pairs with exactly two configurations, as a 9-bit digit mask per band
        uint32_t two[6];
        uint32_t any_two = 0;
        for (int b = 0; b < 6; b++) {
            uint32_t m = 0;
            if ((unfixed >> b) & 1) {
                for (int d = 0; d < 9; d++) m |= (uint32_t) (Popcount8(cfg[b][d]) == 2) << d;
            }
            two[b] = m;
            any_two |= m;
        }
        if (!any_two) {
            static const TdokuResearchParams fallback{3, 0.5, 1.0, 7, 0.0, 0};
            return ChooseResearchWith(state, fallback);
        }

        // lazily computed B(box, digit)
        int16_t bval[9][9];
        uint32_t have[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        auto B = [&](int box, int d) -> int {
            if ((have[box] >> d) & 1) return bval[box][d];
            int i = box / 3, j = box % 3;
            uint32_t pattern = BoxDigitPattern(state.boxen[box].cells, d);
            uint32_t h = cfg[i][d], v = cfg[3 + j][d];
            int total = 0;
            for (int r = 0; r < 3; r++) {
                int hr = Popcount8(h & kElemTables.configs_using_elem[j][r]);
                total += hr * Popcount8(v & kCompat.t[i][(pattern >> (4 * r)) & 7]);
            }
            have[box] |= 1u << d;
            bval[box][d] = (int16_t) total;
            STATS(g_tdoku_stats.b_computations++);
            return total;
        };

        int best_score = INT32_MAX;
        Choice best{NONE, 0, 0};
        STATS(g_tdoku_stats.selections++);
        for (int b = 0; b < 6; b++) {
            int x = b % 3;
            for (uint32_t ds = two[b]; ds; ds &= ds - 1) {
                int d = __builtin_ctz(ds);
                STATS(g_tdoku_stats.candidate_pairs++);
                int score = 0;
                for (int y = 0; y < 3; y++) {
                    int opp = (b < 3 ? 3 : 0) + y;
                    int box = b < 3 ? x * 3 + y : y * 3 + x;
                    score += kWeight60[Popcount8(cfg[opp][d])] * B(box, d);
                }
                if (score < best_score) {
                    best_score = score;
                    best = {(uint32_t) b, d, __builtin_ctz(cfg[b][d])};
                }
            }
        }
        return best;
    }

    // cfg[b][d]: bit c set iff configuration c of band b is possible for digit d
    static inline void TransposeBandConfigs(const State &state, uint8_t cfg[6][9]) {
#if defined(__AVX2__) && defined(__BMI2__)
        for (int x = 0; x < 3; x++) {
            // lanes 0..7: horizontal band x, lanes 8..15: vertical band x
            __m256i r = _mm256_set_m128i(state.bands[1][x].configurations.vec,
                                         state.bands[0][x].configurations.vec);
            // after shifting left by s, bit 7 of each lane holds digit 7 - s and bit 15 holds
            // digit 15 - s (only digit 8 at s = 7). movemask collects bits 7 and 15 of lanes.
            for (int s = 0; s < 8; s++) {
                uint32_t m = (uint32_t) _mm256_movemask_epi8(
                        _mm256_sll_epi16(r, _mm_cvtsi32_si128(s)));
                int d = 7 - s;
                cfg[x][d] = (uint8_t) _pext_u32(m, 0x00000555u);
                cfg[3 + x][d] = (uint8_t) _pext_u32(m, 0x05550000u);
                if (s == 7) {
                    cfg[x][8] = (uint8_t) _pext_u32(m, 0x00000aaau);
                    cfg[3 + x][8] = (uint8_t) _pext_u32(m, 0x0aaa0000u);
                }
            }
        }
#else
        for (int b = 0; b < 6; b++) {
            const Cells08 &c = state.bands[b / 3][b % 3].configurations;
            uint16_t lanes[6];
            for (int k = 0; k < 6; k++) lanes[k] = c.Extract(k);
            for (int d = 0; d < 9; d++) {
                uint32_t m = 0;
                for (int k = 0; k < 6; k++) m |= ((lanes[k] >> d) & 1u) << k;
                cfg[b][d] = (uint8_t) m;
            }
        }
#endif
    }

    // 12-bit pattern of the box's cells containing digit d: bit r * 4 + c for cell (r, c)
    // (bits 3, 7, 11, i.e. the negative-triad lanes, are cleared).
    static inline uint32_t BoxDigitPattern(const Cells16 &cells, int d) {
#if defined(__AVX2__) && defined(__BMI2__)
        uint32_t m = (uint32_t) _mm256_movemask_epi8(
                _mm256_sll_epi16(cells.vec, _mm_cvtsi32_si128(15 - d)));
        return _pext_u32(m, 0x00aaaaaau) & 0x777u;
#else
        uint32_t m = 0;
        for (int lane = 0; lane < 11; lane++) m |= ((cells.Extract(lane) >> d) & 1u) << lane;
        return m & 0x777u;
#endif
    }

#ifdef __AVX2__
    ///////////////////////////////////////////////////////////////////////////////////
    // Branch-free AVX2 implementation of ChooseIxTwoSided (heuristic id 3; identical choices).
    //
    // Works in a "digit-per-byte" layout: a byte vector whose byte d holds a small integer for
    // digit d (bytes 9..15 are zero). Band configurations and box rows are transposed into this
    // layout with pshufb bit expansion; compatibility masks are pshufb lookups into the 8-entry
    // kCompat rows; popcounts are nibble lookups. B is computed for all 81 (box, digit)
    // combinations, weighted 16-bit pair scores for all 54 pairs, and an argmin with the same
    // (band-major, digit-minor) tie order as the scalar and research implementations.

    // byte k of the result is 0xff iff bit (k & 7) of byte (k < 8 ? lo : hi) selected by ctrl is
    // set; ctrl selects, per 128-bit lane, the two bytes of one 16-bit element.
    static inline __m256i ExpandBits(const __m256i &x, const __m256i &ctrl, const __m256i &bitsel) {
        __m256i sel = _mm256_and_si256(_mm256_shuffle_epi8(x, ctrl), bitsel);
        return _mm256_cmpeq_epi8(sel, bitsel);
    }

    static inline __m256i PopcountBytes(const __m256i &x, const __m256i &lut, const __m256i &lo4) {
        __m256i lo = _mm256_shuffle_epi8(lut, _mm256_and_si256(x, lo4));
        __m256i hi = _mm256_shuffle_epi8(lut, _mm256_and_si256(_mm256_srli_epi16(x, 4), lo4));
        return _mm256_add_epi8(lo, hi);
    }

    static inline __m256i ElemCtrl(int element) {
        char lo = (char) (2 * element), hi = (char) (2 * element + 1);
        return _mm256_setr_epi8(lo, lo, lo, lo, lo, lo, lo, lo, hi, hi, hi, hi, hi, hi, hi, hi,
                                lo, lo, lo, lo, lo, lo, lo, lo, hi, hi, hi, hi, hi, hi, hi, hi);
    }

    struct IxSimdTables {
        __m256i elem_ctrl[8];   // ElemCtrl(e) for e = 0..7
        __m256i bit[8];         // set1_epi8(1 << c)
        __m256i tcompat[3];     // kCompat.t[i][0..7] in both lanes
        // the two configurations using element 0/2 (lanes) resp. 1 of peer j, as single bits
        __m256i ua02[3], ub02[3], ua1[3], ub1[3];
        // GFNI bit transposition: pshufb controls placing element e's low byte at qword byte
        // 7 - e and its high byte at byte 15 - e, for elements 0..5 (band configurations),
        // 0..2 (box row 0 / row 2) and 4..6 (box row 1); gf2p8affine with kTransposeX then
        // yields byte d = bit set of elements containing digit d (byte 8 for digit 8).
        __m256i gf_ctrl_cfg, gf_ctrl_row02, gf_ctrl_row1;
        IxSimdTables() noexcept {
            auto make = [](int first, int count) {
                alignas(32) int8_t b[32];
                for (int k = 0; k < 32; k++) b[k] = (int8_t) 0x80;
                for (int lane = 0; lane < 2; lane++) {
                    for (int e = 0; e < count; e++) {
                        b[lane * 16 + 7 - e] = (int8_t) (2 * (first + e));
                        b[lane * 16 + 15 - e] = (int8_t) (2 * (first + e) + 1);
                    }
                }
                return _mm256_load_si256((const __m256i *) b);
            };
            gf_ctrl_cfg = make(0, 6);
            gf_ctrl_row02 = make(0, 3);
            gf_ctrl_row1 = make(4, 3);
            using namespace tdoku_ix;
            for (int e = 0; e < 8; e++) {
                elem_ctrl[e] = ElemCtrl(e);
                bit[e] = _mm256_set1_epi8((char) (1 << e));
            }
            for (int i = 0; i < 3; i++) {
                alignas(32) uint8_t t[32] = {0};
                for (int k = 0; k < 8; k++) t[k] = t[16 + k] = kCompat.t[i][k];
                tcompat[i] = _mm256_load_si256((const __m256i *) t);
                auto lo = [](uint32_t m) { return (uint8_t) (m & (0u - m)); };
                auto hi = [](uint32_t m) { return (uint8_t) (m & (m - 1)); };
                const uint8_t *u = kElemTables.configs_using_elem[i];
                alignas(32) uint8_t a[32] = {0}, b[32] = {0}, c[32] = {0}, e[32] = {0};
                for (int k = 0; k < 16; k++) {
                    a[k] = lo(u[0]); a[16 + k] = lo(u[2]);
                    b[k] = hi(u[0]); b[16 + k] = hi(u[2]);
                    c[k] = c[16 + k] = lo(u[1]);
                    e[k] = e[16 + k] = hi(u[1]);
                }
                ua02[i] = _mm256_load_si256((const __m256i *) a);
                ub02[i] = _mm256_load_si256((const __m256i *) b);
                ua1[i] = _mm256_load_si256((const __m256i *) c);
                ub1[i] = _mm256_load_si256((const __m256i *) e);
            }
        }
    };

    // digit-per-byte transposition of 16-bit elements selected by ctrl (see IxSimdTables)
    static inline __m256i GfTranspose(const __m256i &x, const __m256i &ctrl) {
#ifdef __GFNI__
        return _mm256_gf2p8affine_epi64_epi8(
                _mm256_set1_epi64x((long long) 0x8040201008040201ull),
                _mm256_shuffle_epi8(x, ctrl), 0);
#else
        (void) x; (void) ctrl;
        return _mm256_setzero_si256();
#endif
    }

    // the mean candidate score is only needed by gates that use it
    static constexpr bool kNeedsMeanScore =
            heuristic == kHeuristicIxGated ||
            (heuristic == kHeuristicIxFinal && TDOKU_FINAL_LOOSE_MEAN != 0);

    static inline Choice ChooseIxTwoSidedSimd(const State &state) {
        using namespace tdoku_ix;
        static const IxSimdTables T{};
        uint32_t unfixed = 0;
        for (int b = 0; b < 6; b++) {
            unfixed |= (uint32_t) (state.bands[b / 3][b % 3].configurations.Popcount() >= 10) << b;
        }
        if (!unfixed) return {NONE, 0, 0};

        const __m256i bitsel = _mm256_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128,
                                                1, 2, 4, 8, 16, 32, 64, -128,
                                                1, 2, 4, 8, 16, 32, 64, -128,
                                                1, 2, 4, 8, 16, 32, 64, -128);
        const __m256i lo4 = _mm256_set1_epi8(0x0f);
        const __m256i popcnt_lut = _mm256_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
                                                    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4);
        const __m256i w60_lut = _mm256_setr_epi8(0, 60, 30, 20, 15, 12, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                 0, 60, 30, 20, 15, 12, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0);

        // A. digit-major configuration bytes. cfgv[x]: low lane H_x, high lane V_x.
        __m256i cfgv[3], nv[3], wv[3];
        for (int x = 0; x < 3; x++) {
            __m256i r = _mm256_set_m128i(state.bands[1][x].configurations.vec,
                                         state.bands[0][x].configurations.vec);
#ifdef __GFNI__
            __m256i acc = GfTranspose(r, T.gf_ctrl_cfg);
#else
            __m256i acc = _mm256_setzero_si256();
#pragma GCC unroll 6
            for (int c = 0; c < 6; c++) {
                acc = _mm256_or_si256(acc, _mm256_and_si256(
                        ExpandBits(r, T.elem_ctrl[c], bitsel), T.bit[c]));
            }
#endif
            cfgv[x] = acc;
            nv[x] = PopcountBytes(acc, popcnt_lut, lo4);     // B. configuration counts
            wv[x] = _mm256_shuffle_epi8(w60_lut, nv[x]);      // C. weights 60 / n
        }

        // per-band 16-bit weight vectors (digits in lanes 0..15)
        __m256i w16[6];
        for (int x = 0; x < 3; x++) {
            w16[x] = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(wv[x]));
            w16[3 + x] = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(wv[x], 1));
        }
        __m256i score[6];
        for (int b = 0; b < 6; b++) score[b] = _mm256_setzero_si256();

        // D/E. B(i, j, d) for every box
#pragma GCC unroll 3
        for (int i = 0; i < 3; i++) {
            __m128i h128 = _mm256_castsi256_si128(cfgv[i]);
            __m256i hh = _mm256_set_m128i(h128, h128);
            const __m256i tcompat = T.tcompat[i];
#pragma GCC unroll 3
            for (int j = 0; j < 3; j++) {
                const __m256i cells = state.boxen[i * 3 + j].cells.vec;
                // rows 0 (low lane) and 2 (high lane): cells at lanes c and 8 + c
                // row 1 (low lane): cells at lanes 4 + c
#ifdef __GFNI__
                __m256i r02 = GfTranspose(cells, T.gf_ctrl_row02);
                __m256i r1 = GfTranspose(cells, T.gf_ctrl_row1);
#else
                __m256i r02 = _mm256_setzero_si256(), r1 = _mm256_setzero_si256();
#pragma GCC unroll 3
                for (int c = 0; c < 3; c++) {
                    r02 = _mm256_or_si256(r02, _mm256_and_si256(
                            ExpandBits(cells, T.elem_ctrl[c], bitsel), T.bit[c]));
                    r1 = _mm256_or_si256(r1, _mm256_and_si256(
                            ExpandBits(cells, T.elem_ctrl[4 + c], bitsel), T.bit[c]));
                }
#endif
                __m128i v128 = _mm256_extracti128_si256(cfgv[j], 1);
                __m256i vv = _mm256_set_m128i(v128, v128);
                // configurations of H_i using rows 0/2 (resp. 1) in peer j
                __m256i k02 = PopcountBytes(
                        _mm256_and_si256(vv, _mm256_shuffle_epi8(tcompat, r02)), popcnt_lut, lo4);
                __m256i k1 = PopcountBytes(
                        _mm256_and_si256(vv, _mm256_shuffle_epi8(tcompat, r1)), popcnt_lut, lo4);
                // hm * k where hm counts which of the two configurations (a, b) of H_i using this
                // row of peer j are still possible:  (a in h ? k : 0) + (b in h ? k : 0)
                auto has = [&](const __m256i &bitc) {
                    return _mm256_cmpeq_epi8(_mm256_and_si256(hh, bitc), bitc);
                };
                __m256i p02 = _mm256_add_epi8(_mm256_and_si256(has(T.ua02[j]), k02),
                                              _mm256_and_si256(has(T.ub02[j]), k02));
                __m256i p1 = _mm256_add_epi8(_mm256_and_si256(has(T.ua1[j]), k1),
                                             _mm256_and_si256(has(T.ub1[j]), k1));
                __m128i bsum = _mm_add_epi8(
                        _mm_add_epi8(_mm256_castsi256_si128(p02), _mm256_extracti128_si256(p02, 1)),
                        _mm256_castsi256_si128(p1));
                __m256i b16 = _mm256_cvtepu8_epi16(bsum);
                // F. weighted pair scores
                score[i] = _mm256_add_epi16(score[i], _mm256_mullo_epi16(w16[3 + j], b16));
                score[3 + j] = _mm256_add_epi16(score[3 + j], _mm256_mullo_epi16(w16[i], b16));
            }
        }

        // G. argmin over candidate pairs (unfixed band, exactly two configurations)
        const __m256i two16 = _mm256_set1_epi16(2);
        __m256i key[6];
        __m256i mn = _mm256_set1_epi16(-1);
        __m256i sum32 = _mm256_setzero_si256();
        int ncand = 0;
        for (int b = 0; b < 6; b++) {
            __m128i n8 = b < 3 ? _mm256_castsi256_si128(nv[b])
                               : _mm256_extracti128_si256(nv[b - 3], 1);
            __m256i cand = _mm256_cmpeq_epi16(_mm256_cvtepu8_epi16(n8), two16);
            if (!((unfixed >> b) & 1)) cand = _mm256_setzero_si256();
            key[b] = _mm256_or_si256(score[b], _mm256_xor_si256(cand, _mm256_set1_epi16(-1)));
            mn = _mm256_min_epu16(mn, key[b]);
            if (kNeedsMeanScore) {
                sum32 = _mm256_add_epi32(sum32, _mm256_madd_epi16(
                        _mm256_and_si256(score[b], cand), _mm256_set1_epi16(1)));
                ncand += __builtin_popcount((uint32_t) _mm256_movemask_epi8(cand)) >> 1;
            }
        }
        uint32_t score_sum = 0;
        if (kNeedsMeanScore) {
            __m128i s128 = _mm_add_epi32(_mm256_castsi256_si128(sum32),
                                         _mm256_extracti128_si256(sum32, 1));
            s128 = _mm_add_epi32(s128, _mm_shuffle_epi32(s128, 0x4e));
            s128 = _mm_add_epi32(s128, _mm_shuffle_epi32(s128, 0xb1));
            score_sum = (uint32_t) _mm_cvtsi128_si32(s128);
        }
        __m128i mn128 = _mm_min_epu16(_mm256_castsi256_si128(mn), _mm256_extracti128_si256(mn, 1));
        uint32_t min_val = (uint32_t) _mm_cvtsi128_si32(_mm_minpos_epu16(mn128)) & 0xffffu;
        if (min_val == 0xffffu) {
            static const TdokuResearchParams fallback{3, 0.5, 1.0, 7, 0.0, 0};
            return ChooseResearchWith(state, fallback);
        }
        __m256i target = _mm256_set1_epi16((short) min_val);
        for (int b = 0; b < 6; b++) {
            uint32_t m = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi16(key[b], target));
            if (m) {
                int d = __builtin_ctz(m) >> 1;
                alignas(32) uint8_t bytes[32];
                _mm256_store_si256((__m256i *) bytes, cfgv[b % 3]);
                return {(uint32_t) b, d, __builtin_ctz(bytes[(b < 3 ? 0 : 16) + d]),
                        (uint16_t) min_val,
                        (uint16_t) (kNeedsMeanScore ? score_sum / (uint32_t) ncand : 0)};
            }
        }
        return {NONE, 0, 0};  // unreachable
    }
#endif

    static inline Choice Choose(const State &state) {
        switch (heuristic) {
            case kHeuristicIxTwoSided:
                return ChooseIxTwoSided(state);
            case kHeuristicIxTwoSidedSimd:
            case kHeuristicIxGated:
            case kHeuristicIxFinal:
#ifdef __AVX2__
                return ChooseIxTwoSidedSimd(state);
#else
                return ChooseIxTwoSided(state);
#endif
            default:
                return ChooseResearch(state);
        }
    }

    size_t SafeCountSolutionsConsistentWithPartialAssignment(State state, size_t limit) {
        limit_ = limit;
        num_solutions_ = 0;
        num_fails_ = 0;
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
        num_fails_ = 0;
        depth_ = 0;
        loose_[0] = 0;
#ifdef TDOKU_STATS
        stats_depth_ = 0;
        on_path_ = true;
        stats_fails_at_solution_ = 0;
#endif
        bool pencilmark = input[81] >= '.';

        State state;
        bool consistent;
        if (heuristic == kHeuristicOriginal) {
            consistent = pencilmark ? InitPencilmarkByBox(input, state)
                                    : InitVanillaByBand(input, state);
        } else {
            // experimental heuristics reuse the original translation unit's compiled
            // initialization so that puzzle setup costs exactly the same (see the report).
            consistent = TdokuInitState(input, pencilmark, &state);
        }
        if (consistent) {
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


#ifndef TDOKU_EXPERIMENTAL_TU
SolverDpllTriadSimd<0> solver_none{};
SolverDpllTriadSimd<1> solver_last{};
SolverDpllTriadSimd<2> solver_enum{};

GeneratorDpllTriadSimd generator{};

} // namespace

// implemented in the experimental translation unit
extern "C" size_t TdokuExperimentalSolve(uint32_t heuristic, const char *puzzle, size_t limit,
                                         bool return_last, char *solution, size_t *num_guesses);
extern "C" size_t TdokuExperimentalEnumerate(uint32_t heuristic, const char *puzzle, size_t limit,
                                             void (*callback)(const char *, void *),
                                             void *callback_arg);
extern "C" size_t TdokuFinalSolve(const char *puzzle, size_t limit, bool return_last,
                                  char *solution, size_t *num_guesses);
extern "C" size_t TdokuFinalEnumerate(const char *puzzle, size_t limit,
                                      void (*callback)(const char *, void *), void *callback_arg);


// configuration: bits 0..7 nonzero => return a solution (as before); bits 8..15 select the
// branching heuristic (0 = original tdoku). Configurations < 256 behave exactly as before.
extern "C"
size_t TdokuSolverDpllTriadSimd(const char *puzzle, size_t limit,
                                uint32_t configuration,
                                char *solution, size_t *num_guesses) {
    bool return_last = limit == 1 || (configuration & 0xffu) > 0;
    uint32_t heuristic = (configuration >> 8u) & 0xffu;
    if (heuristic == kHeuristicIxFinal) {
        return TdokuFinalSolve(puzzle, limit, return_last, solution, num_guesses);
    }
    if (heuristic != kHeuristicOriginal && heuristic < kHeuristicIxFinal) {
        return TdokuExperimentalSolve(heuristic, puzzle, limit, return_last, solution,
                                      num_guesses);
    }
    if (return_last) {
        return solver_last.SolveSudoku(puzzle, limit, solution, num_guesses);
    } else {
        return solver_none.SolveSudoku(puzzle, limit, solution, num_guesses);
    }
}

extern "C"
bool TdokuInitState(const char *input, bool pencilmark, void *state) {
    State &st = *(State *) state;
    return pencilmark ? SolverDpllTriadSimd<0>::InitPencilmarkByBox(input, st)
                      : SolverDpllTriadSimd<0>::InitVanillaByBand(input, st);
}

extern "C"
size_t TdokuEnumerate(const char *puzzle, size_t limit,
                      void (*callback)(const char *, void *), void *callback_arg) {
    solver_enum.callback_ = callback;
    solver_enum.callback_arg_ = callback_arg;
    return solver_enum.SolveSudoku(puzzle, limit, nullptr, nullptr);
}

// As TdokuEnumerate, but with a selectable branching heuristic (see above).
extern "C"
size_t TdokuEnumerateWithHeuristic(const char *puzzle, size_t limit, uint32_t heuristic,
                                   void (*callback)(const char *, void *), void *callback_arg) {
    if (heuristic == kHeuristicOriginal || heuristic > kHeuristicIxFinal) {
        return TdokuEnumerate(puzzle, limit, callback, callback_arg);
    }
    if (heuristic == kHeuristicIxFinal) {
        return TdokuFinalEnumerate(puzzle, limit, callback, callback_arg);
    }
    return TdokuExperimentalEnumerate(heuristic, puzzle, limit, callback, callback_arg);
}

extern "C"
void TdokuSetResearchParams(const TdokuResearchParams *params) {
    g_research_params = *params;
}

extern "C"
void TdokuSetIxGateReset(int reset_on_solution) {
    g_ix_gate_reset_on_solution = reset_on_solution;
}

extern "C"
void TdokuSetIxGateLoose(int slope, int floor, int reopen) {
    g_ix_gate_loose_slope = slope;
    g_ix_gate_loose_floor = floor;
    g_ix_gate_loose_reopen = reopen;
}

extern "C"
void TdokuSetIxGateExtra(int loose_min, int loose_mean, int max_depth, int open_depth) {
    g_ix_gate_loose_min = loose_min;
    g_ix_gate_loose_mean = loose_mean;
    g_ix_gate_max_depth = max_depth;
    g_ix_gate_open_depth = open_depth;
}

extern "C"
void TdokuSetIxGate(int min_fails, int min_total, int open_decisions, int open_total) {
    g_ix_gate_min_fails = min_fails;
    g_ix_gate_min_total = min_total;
    g_ix_gate_open_decisions = open_decisions;
    g_ix_gate_open_total = open_total;
}

extern "C"
bool TdokuConstrain(bool pencilmark, char *puzzle) {
    return generator.Constrain(pencilmark, puzzle);
}

extern "C"
bool TdokuMinimize(bool pencilmark, bool monotonic, char *puzzle) {
    return generator.Minimize(pencilmark, monotonic, puzzle);
}
#else  // TDOKU_EXPERIMENTAL_TU
// solver instances for the experimental heuristics
template<int h>
struct SolverSet {
    SolverDpllTriadSimd<0, h> none{};
    SolverDpllTriadSimd<1, h> last{};
    SolverDpllTriadSimd<2, h> enumerate{};

    size_t Solve(const char *puzzle, size_t limit, bool return_last,
                 char *solution, size_t *num_guesses) {
        return return_last ? last.SolveSudoku(puzzle, limit, solution, num_guesses)
                           : none.SolveSudoku(puzzle, limit, solution, num_guesses);
    }

    size_t Enumerate(const char *puzzle, size_t limit,
                     void (*callback)(const char *, void *), void *callback_arg) {
        enumerate.callback_ = callback;
        enumerate.callback_arg_ = callback_arg;
        return enumerate.SolveSudoku(puzzle, limit, nullptr, nullptr);
    }
};

#ifdef TDOKU_FINAL_TU
// the selected heuristic alone in its own TU, so its code is compiled under the same
// conditions as the original (3 solution-mode instantiations per TU)
SolverSet<kHeuristicIxFinal> solvers_ix_final{};

} // namespace

extern "C"
size_t TdokuFinalSolve(const char *puzzle, size_t limit, bool return_last, char *solution,
                       size_t *num_guesses) {
    return solvers_ix_final.Solve(puzzle, limit, return_last, solution, num_guesses);
}

extern "C"
size_t TdokuFinalEnumerate(const char *puzzle, size_t limit,
                           void (*callback)(const char *, void *), void *callback_arg) {
    return solvers_ix_final.Enumerate(puzzle, limit, callback, callback_arg);
}
#else
SolverSet<kHeuristicResearch> solvers_research{};
SolverSet<kHeuristicIxTwoSided> solvers_ix_two_sided{};
SolverSet<kHeuristicIxTwoSidedSimd> solvers_ix_two_sided_simd{};
SolverSet<kHeuristicIxGated> solvers_ix_gated{};

} // namespace

extern "C"
size_t TdokuExperimentalSolve(uint32_t heuristic, const char *puzzle, size_t limit,
                              bool return_last, char *solution, size_t *num_guesses) {
    switch (heuristic) {
        case kHeuristicResearch:
            return solvers_research.Solve(puzzle, limit, return_last, solution, num_guesses);
        case kHeuristicIxTwoSided:
            return solvers_ix_two_sided.Solve(puzzle, limit, return_last, solution, num_guesses);
        case kHeuristicIxTwoSidedSimd:
            return solvers_ix_two_sided_simd.Solve(puzzle, limit, return_last, solution,
                                                   num_guesses);
        case kHeuristicIxGated:
        default:
            return solvers_ix_gated.Solve(puzzle, limit, return_last, solution, num_guesses);
    }
}

extern "C"
size_t TdokuExperimentalEnumerate(uint32_t heuristic, const char *puzzle, size_t limit,
                                  void (*callback)(const char *, void *), void *callback_arg) {
    switch (heuristic) {
        case kHeuristicResearch:
            return solvers_research.Enumerate(puzzle, limit, callback, callback_arg);
        case kHeuristicIxTwoSided:
            return solvers_ix_two_sided.Enumerate(puzzle, limit, callback, callback_arg);
        case kHeuristicIxTwoSidedSimd:
            return solvers_ix_two_sided_simd.Enumerate(puzzle, limit, callback, callback_arg);
        case kHeuristicIxGated:
        default:
            return solvers_ix_gated.Enumerate(puzzle, limit, callback, callback_arg);
    }
}
#endif  // TDOKU_FINAL_TU
#endif  // TDOKU_EXPERIMENTAL_TU
