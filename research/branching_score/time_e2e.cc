// End-to-end timing of the branching heuristics inside the real Tdoku search.
//
// Policies:  orig   the original solver (SolverDpllTriadSimd, unchanged)
//            ixg    the PR's gated intersection-aware solver (TdokuSolverIntersection)
//            ds dv2 lv2   the SIMD candidates, ungated, at every decision
// The candidate solver is the ix solver's search with its choice replaced (same propagation,
// same branching on one configuration). Policies are timed round robin, `reps` passes over the
// whole set each, and the median pass is reported as ns per puzzle (plus min).
//
// usage: time_e2e <puzzles> <max_puzzles> <limit> <reps> <policy,...>
// prints: policy n guesses_per_puzzle median_ns_per_puzzle min_ns_per_puzzle
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "features.h"
#include "simd_ds.h"
#include "simd_dv2.h"
#include "simd_lv2.h"

namespace {

SolverDpllTriadSimd<0> g_orig{};

const Cells08 &ConfigLaneMask(int config) {
    static const Cells08 masks[6]{
            {0xffff, 0, 0, 0, 0, 0, 0, 0}, {0, 0xffff, 0, 0, 0, 0, 0, 0}, {0, 0, 0xffff, 0, 0, 0, 0, 0},
            {0, 0, 0, 0xffff, 0, 0, 0, 0}, {0, 0, 0, 0, 0xffff, 0, 0, 0}, {0, 0, 0, 0, 0, 0xffff, 0, 0}};
    return masks[config];
}

template <typename Policy>
struct ScoreSolver {
    const typename Policy::Tables *T;
    size_t limit = 2, num_solutions = 0, num_guesses = 0;

    void Count(State &state) {
        int b, d, c;
        if (!Policy::Choose(state, *T, b, d, c)) {
            num_solutions++;
            return;
        }
        if (b < 3) Branch<0>(b, d, c, state);
        else Branch<1>(b - 3, d, c, state);
    }

    template <int vertical>
    void Branch(int band_idx, int digit, int config, State &state) {
        Band &band = state.bands[vertical][band_idx];
        Cells08 value_configurations = band.configurations & tables.one_value_mask[digit];
        num_guesses++;
        State state_copy = state;
        Cells08 assignment_elims = value_configurations.and_not(ConfigLaneMask(config));
        state_copy.bands[vertical][band_idx].eliminations |= assignment_elims;
        if (Base::template BandEliminate<vertical>(state_copy, band_idx)) {
            Count(state_copy);
            if (num_solutions == limit) return;
        }
        Cells08 negation_elims = value_configurations & ConfigLaneMask(config);
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (Base::template BandEliminate<vertical>(state, band_idx)) Count(state);
    }

    size_t Solve(const char *input, size_t lim, size_t *guesses) {
        limit = lim;
        num_solutions = num_guesses = 0;
        State state;
        bool pencilmark = input[81] >= '.';
        if (pencilmark ? Base::InitPencilmarkByBox(input, state) : Base::InitVanillaByBand(input, state))
            Count(state);
        *guesses = num_guesses;
        return num_solutions;
    }
};

struct PDS {
    using Tables = sds::Tables;
    static bool Choose(const State &s, const Tables &t, int &b, int &d, int &c) { return sds::Choose(s, t, b, d, c); }
};
struct PDV2 {
    using Tables = sdv2::Tables;
    static bool Choose(const State &s, const Tables &t, int &b, int &d, int &c) { return sdv2::Choose(s, t, b, d, c); }
};
struct PLV2 {
    using Tables = slv2::Tables;
    static bool Choose(const State &s, const Tables &t, int &b, int &d, int &c) { return slv2::Choose(s, t, b, d, c); }
};

const sds::Tables g_ds_t{sds::Params{}};
const sdv2::Tables g_dv2_t{sdv2::Params{}};
const slv2::Tables g_lv2_t{slv2::Params{}};
ScoreSolver<PDS> g_ds{&g_ds_t};
ScoreSolver<PDV2> g_dv2{&g_dv2_t};
ScoreSolver<PLV2> g_lv2{&g_lv2_t};

size_t SolveWith(const std::string &policy, const char *p, size_t limit, size_t *g) {
    char sol[81];
    if (policy == "orig") return g_orig.SolveSudoku(p, limit, sol, g);
    if (policy == "ixg") return TdokuSolverIntersection(p, limit, false, sol, g);
    if (policy == "ds") return g_ds.Solve(p, limit, g);
    if (policy == "dv2") return g_dv2.Solve(p, limit, g);
    if (policy == "lv2") return g_lv2.Solve(p, limit, g);
    fprintf(stderr, "unknown policy %s\n", policy.c_str());
    exit(1);
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: time_e2e <puzzles> <max_puzzles> <limit> <reps> <policy,...>\n");
        return 1;
    }
    size_t max_p = (size_t) atol(argv[2]), limit = (size_t) atol(argv[3]);
    int reps = atoi(argv[4]);
    std::vector<std::string> policies;
    for (char *t = strtok(argv[5], ","); t; t = strtok(nullptr, ",")) policies.emplace_back(t);
    std::vector<std::vector<char>> puzzles;
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }
    static char line[8192];
    while (puzzles.size() < max_p && fgets(line, sizeof line, f)) {
        size_t n = strcspn(line, " \t\r\n,:");
        if (n != 81 && n != 729) continue;
        std::vector<char> buf(736, 0);
        memcpy(buf.data(), line, n);
        puzzles.push_back(buf);
    }
    fclose(f);
    size_t np = puzzles.size();
    std::vector<std::vector<double>> times(policies.size());
    std::vector<size_t> guesses(policies.size(), 0), sols(policies.size(), 0);
    volatile size_t sink = 0;
    for (int r = -1; r < reps; r++) {  // r = -1: warm-up pass, also collects guess counts
        for (size_t k = 0; k < policies.size(); k++) {
            size_t gsum = 0, ssum = 0;
            auto t0 = std::chrono::steady_clock::now();
            for (auto &p : puzzles) {
                size_t g;
                ssum += SolveWith(policies[k], p.data(), limit, &g);
                gsum += g;
            }
            auto t1 = std::chrono::steady_clock::now();
            sink += ssum;
            if (r < 0) { guesses[k] = gsum; sols[k] = ssum; continue; }
            times[k].push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / (double) np);
        }
    }
    for (size_t k = 0; k < policies.size(); k++) {
        auto v = times[k];
        std::sort(v.begin(), v.end());
        printf("%-5s n %zu sols %zu guesses %.2f median_ns %.1f min_ns %.1f\n", policies[k].c_str(), np, sols[k],
               (double) guesses[k] / (double) np, v[v.size() / 2], v[0]);
    }
    return (int) (sink & 0);
}
