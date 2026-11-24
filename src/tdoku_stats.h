#ifndef TDOKU_STATS_H
#define TDOKU_STATS_H

// Search statistics, compiled in only when TDOKU_STATS is defined (never in release builds).

#include <cstdint>
#include <vector>

struct TdokuStats {
    uint64_t nodes;          // calls to CountSolutionsConsistentWithPartialAssignment
    uint64_t decisions;      // branching decisions (== num_guesses)
    uint64_t fails;          // branches (positive or negative) refuted by propagation
    uint64_t select_cycles;  // rdtsc cycles spent choosing the branch (incl. the final no-op)
    uint64_t n_hist[7];      // configurations of the chosen band-digit pair at each decision
    uint64_t soundness_checks;
    uint64_t soundness_violations;  // cases with actual opposite count > estimated k_y
    uint64_t candidate_pairs;       // optimized selectors: candidate pairs scored
    uint64_t b_computations;        // optimized selectors: (box, digit) terms computed
    uint64_t selections;            // optimized selectors: calls that scored candidates
    // solution-oracle decomposition (unique puzzles, oracle enabled):
    uint64_t path_decisions;        // P: decisions taken in states consistent with the solution
    uint64_t path_correct_first;    // ... whose first (positive) branch contains the solution
    uint64_t offpath_decisions;     // decisions in refuted subtrees
    uint64_t w_wrong_first;         // W1: decisions in refuted positive branches of path nodes
    uint64_t w_after_solution;      // W2: decisions in negative branches after the solution
    uint64_t immediate_refutations; // path nodes whose off-path child needed zero decisions
};

extern char g_oracle_solution[81];
extern bool g_oracle_enabled;

// One record per decision (positive branch) when tracing is enabled.
struct TdokuTraceRecord {
    uint8_t n;            // configurations of the chosen pair
    uint8_t cfgmask;      // the pair's configuration mask before branching
    int8_t asserted;      // asserted configuration
    bool ok;              // positive branch survived propagation
    uint8_t ny[3];        // opposite band counts before
    uint8_t ny_after[3];  // opposite band counts after propagation (if ok)
    uint8_t K[3][3];      // compatibility matrix K[y][e]
    uint16_t total_before, total_after;  // all-band configuration totals
};

// One record per decision when decision tracing is enabled: state features at the time the
// decision is made, and the number of decisions in the subtree it spawns (itself included).
struct TdokuDecisionRecord {
    uint16_t depth;           // decisions on the path from the root (0 = root decision)
    uint32_t decisions_before;
    uint32_t fails_before;    // refuted branches so far in this solve
    uint32_t solutions_before;
    uint16_t total_configs;   // sum over bands of remaining configurations (54 = solved)
    uint8_t unfixed_bands;
    uint8_t n2_pairs;         // band-digit pairs with exactly two configurations
    uint32_t subtree;         // decisions in the subtree, including this one
    uint32_t refutations;     // refuted branches inside the subtree (incl. this node's children)
    uint32_t fails_since_solution;  // refuted branches since the last solution was found
    uint16_t n_candidates;    // n=2 pairs in unfixed bands (the intersection score's candidates)
    uint16_t score_min, score_max, score_mean;  // AM2 scores of the candidates (x60; max 360)
};

extern TdokuStats g_tdoku_stats;
extern std::vector<TdokuDecisionRecord> g_tdoku_decisions;
extern bool g_tdoku_decisions_enabled;
extern std::vector<TdokuTraceRecord> g_tdoku_trace;
extern bool g_tdoku_trace_enabled;

#endif  // TDOKU_STATS_H
