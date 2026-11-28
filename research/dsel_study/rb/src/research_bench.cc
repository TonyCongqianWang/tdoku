// research_bench: paired, per-puzzle comparison of tdoku branching heuristics.
//
//   research_bench -m count|time [options] puzzle_file...
//
// Variants are given with -v as a ';'-separated list of name=spec, where spec is
//   h:<id>                       a compiled heuristic id (0 = original tdoku)
//   r:k=v,k=v,...                the scalar research selector with parameters
//                                pair=<0..5> w=<soft_w> p=<soft_p> ix=<0..6> ixp=<p> scope=<0|1>
//
// count mode: deterministic search statistics per puzzle and variant (guesses, solution counts;
//             with the _stats build also nodes, failed branches, selection cycles, n histogram).
//             Verifies that every variant returns the same solution count as the first one and,
//             for limit-1 runs, that the returned solution is valid and consistent with the clues.
// time mode:  interleaved repetitions (variant order shuffled each repetition), rdtsc timing of
//             every puzzle; reports median/min/MAD time per puzzle and the paired ratio to the
//             first variant.
//
// Puzzles are used exactly as given unless -x <seed> is passed, in which case each puzzle is
// transformed by a random validity-preserving transformation (digit relabeling, band/stack
// permutation, row/column permutation within bands/stacks, optional transposition) drawn from
// a generator seeded with <seed>.

#include "../include/tdoku.h"
#include "branch_features.h"
#ifdef TDOKU_STATS
#include "tdoku_stats.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <x86intrin.h>

extern "C" void TdokuSetResearchParams(const TdokuResearchParams *params);
extern "C" void TdokuSetIxGate(int min_fails, int min_total, int open_decisions, int open_total);
extern "C" void TdokuSetIxGateReset(int reset_on_solution);
extern "C" void TdokuSetIxGateExtra(int loose_min, int loose_mean, int max_depth, int open_depth);
extern "C" void TdokuSetIxGateLoose(int slope, int floor, int reopen);

using namespace std;

namespace {

struct Variant {
    string name;
    uint32_t heuristic = 0;
    TdokuResearchParams params{1, 0.5, 1.0, 0, 0.0, 0};
    bool research = false;
    int gate_fails = -1, gate_total = 0, gate_open = 0, gate_open_total = 1000, gate_reset = 0;
    int gate_loose_min = 0, gate_loose_mean = 0, gate_max_depth = 1000, gate_open_depth = 0;
    int gate_loose_slope = 0, gate_loose_floor = 0, gate_loose_reopen = 0;  // g:<min_fails>:<min_total>[:<open>[:<open_total>]]
};

Variant ParseVariant(const string &item) {
    Variant v;
    auto eq = item.find('=');
    if (eq == string::npos) { cerr << "bad variant " << item << endl; exit(1); }
    v.name = item.substr(0, eq);
    string spec = item.substr(eq + 1);
    if (spec.rfind("h:", 0) == 0) {
        v.heuristic = (uint32_t) stoi(spec.substr(2));
    } else if (spec.rfind("g:", 0) == 0) {
        v.heuristic = 4;
        stringstream gs(spec.substr(2));
        string tok;
        vector<int> vals;
        while (getline(gs, tok, ':')) vals.push_back(stoi(tok));
        v.gate_fails = vals.size() > 0 ? vals[0] : 1;
        v.gate_total = vals.size() > 1 ? vals[1] : 0;
        v.gate_open = vals.size() > 2 ? vals[2] : 0;
        v.gate_open_total = vals.size() > 3 ? vals[3] : 1000;
        v.gate_reset = vals.size() > 4 ? vals[4] : 0;
        v.gate_loose_min = vals.size() > 5 ? vals[5] : 0;
        v.gate_loose_mean = vals.size() > 6 ? vals[6] : 0;
        v.gate_max_depth = vals.size() > 7 ? vals[7] : 1000;
        v.gate_open_depth = vals.size() > 8 ? vals[8] : 0;
        v.gate_loose_slope = vals.size() > 9 ? vals[9] : 0;
        v.gate_loose_floor = vals.size() > 10 ? vals[10] : 0;
        v.gate_loose_reopen = vals.size() > 11 ? vals[11] : 0;
    } else if (spec.rfind("r:", 0) == 0) {
        v.research = true;
        v.heuristic = 1;
        stringstream ss(spec.substr(2));
        string kv;
        while (getline(ss, kv, ',')) {
            auto e = kv.find('=');
            if (e == string::npos) continue;
            string k = kv.substr(0, e);
            double val = stod(kv.substr(e + 1));
            if (k == "pair") v.params.pair_rule = (int) val;
            else if (k == "w") v.params.soft_w = val;
            else if (k == "p") v.params.soft_p = val;
            else if (k == "ix") v.params.ix_score = (int) val;
            else if (k == "ixp") v.params.ix_p = val;
            else if (k == "scope") v.params.ix_scope = (int) val;
            else { cerr << "unknown key " << k << endl; exit(1); }
        }
    } else {
        cerr << "bad variant spec " << spec << endl;
        exit(1);
    }
    return v;
}

void Activate(const Variant &v) {
    if (v.research) TdokuSetResearchParams(&v.params);
    if (v.gate_fails >= 0) {
        TdokuSetIxGate(v.gate_fails, v.gate_total, v.gate_open, v.gate_open_total);
        TdokuSetIxGateReset(v.gate_reset);
        TdokuSetIxGateExtra(v.gate_loose_min, v.gate_loose_mean, v.gate_max_depth, v.gate_open_depth);
        TdokuSetIxGateLoose(v.gate_loose_slope, v.gate_loose_floor, v.gate_loose_reopen);
    }
}

// random validity-preserving transformation of an 81-char puzzle
void Transform(char *puzzle, mt19937_64 &rng) {
    auto block_shuffle = [&](array<int, 9> &perm) {
        array<int, 3> blocks{0, 1, 2};
        shuffle(blocks.begin(), blocks.end(), rng);
        for (int i = 0; i < 3; i++) {
            array<int, 3> inner{0, 1, 2};
            shuffle(inner.begin(), inner.end(), rng);
            for (int j = 0; j < 3; j++) perm[i * 3 + j] = blocks[i] * 3 + inner[j];
        }
    };
    array<int, 9> digits{0, 1, 2, 3, 4, 5, 6, 7, 8}, rows{}, cols{};
    shuffle(digits.begin(), digits.end(), rng);
    block_shuffle(rows);
    block_shuffle(cols);
    bool transpose = rng() & 1u;
    char out[81];
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            char ch = puzzle[r * 9 + c];
            if (ch >= '1' && ch <= '9') ch = (char) ('1' + digits[ch - '1']);
            else ch = '.';
            int rr = rows[r], cc = cols[c];
            if (transpose) swap(rr, cc);
            out[rr * 9 + cc] = ch;
        }
    }
    memcpy(puzzle, out, 81);
}

bool ValidSolution(const char *puzzle, const char *solution) {
    for (int i = 0; i < 81; i++) {
        if (solution[i] < '1' || solution[i] > '9') return false;
        if (puzzle[i] != '.' && puzzle[i] != solution[i]) return false;
    }
    for (int g = 0; g < 27; g++) {
        uint32_t seen = 0;
        for (int k = 0; k < 9; k++) {
            int r, c;
            if (g < 9) { r = g; c = k; }
            else if (g < 18) { r = k; c = g - 9; }
            else { r = ((g - 18) / 3) * 3 + k / 3; c = ((g - 18) % 3) * 3 + k % 3; }
            seen |= 1u << (solution[r * 9 + c] - '1');
        }
        if (seen != 0x1ff) return false;
    }
    return true;
}

struct Options {
    string mode = "count";
    vector<Variant> variants;
    size_t max_puzzles = SIZE_MAX;
    uint64_t transform_seed = 0;
    size_t limit = 2;
    int reps = 5;
    double warmup_seconds = 1.0;
    string per_puzzle_out;
    bool validate = true;
    string tag;  // free-form label echoed in output rows
    string trace_out;  // (stats build) per-decision trace of estimates vs. propagation
    string decisions_out;  // (stats build) per-decision state features and subtree sizes
    bool oracle = false;   // (stats build) solution-oracle decomposition of the search
};

vector<string> LoadPuzzles(const string &filename, const Options &opt) {
    ifstream file(filename);
    if (!file) { cerr << "cannot open " << filename << endl; exit(1); }
    vector<string> puzzles;
    string line;
    mt19937_64 rng(opt.transform_seed);
    while (getline(file, line) && puzzles.size() < opt.max_puzzles) {
        if (line.empty() || line[0] == '#') continue;
        if (line.size() < 81) continue;
        // pencilmark puzzles: 729 characters (patched for this study)
        size_t plen = line.find_first_of(" \t\r") ;
        string p = line.substr(0, (plen == 729 || (plen == string::npos && line.size() >= 729)) ? 729 : 81);
        for (char &ch : p) if (ch < '1' || ch > '9') ch = '.';
        if (opt.transform_seed) Transform(&p[0], rng);
        puzzles.push_back(p);
    }
    return puzzles;
}

double TscGhz() {
    auto t0 = chrono::steady_clock::now();
    uint64_t c0 = __rdtsc();
    while (chrono::steady_clock::now() - t0 < chrono::milliseconds(200)) {}
    uint64_t c1 = __rdtsc();
    double ns = chrono::duration<double, nano>(chrono::steady_clock::now() - t0).count();
    return (c1 - c0) / ns;
}

double Median(vector<double> v) {
    sort(v.begin(), v.end());
    size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

void RunCount(const string &filename, const vector<string> &puzzles, const Options &opt) {
    size_t n = puzzles.size();
    vector<vector<size_t>> guesses(opt.variants.size(), vector<size_t>(n));
    vector<vector<size_t>> counts(opt.variants.size(), vector<size_t>(n));
#ifdef TDOKU_STATS
    vector<vector<uint64_t>> nodes(opt.variants.size(), vector<uint64_t>(n));
    vector<vector<uint64_t>> fails(opt.variants.size(), vector<uint64_t>(n));
    vector<vector<uint64_t>> sel(opt.variants.size(), vector<uint64_t>(n));
#endif
    size_t mismatches = 0, invalid = 0;
    for (size_t vi = 0; vi < opt.variants.size(); vi++) {
        const Variant &v = opt.variants[vi];
        Activate(v);
#ifdef TDOKU_STATS
        TdokuStats agg{};
#endif
        for (size_t i = 0; i < n; i++) {
            char solution[82] = {0};
            size_t g = 0;
#ifdef TDOKU_STATS
            g_tdoku_stats = TdokuStats{};
#endif
            uint32_t config = (v.heuristic << 8u) | (opt.limit == 1 ? 1u : 0u);
            size_t count = TdokuSolverDpllTriadSimd(puzzles[i].c_str(), opt.limit, config,
                                                    solution, &g);
            guesses[vi][i] = g;
            counts[vi][i] = count;
#ifdef TDOKU_STATS
            nodes[vi][i] = g_tdoku_stats.nodes;
            fails[vi][i] = g_tdoku_stats.fails;
            sel[vi][i] = g_tdoku_stats.select_cycles;
            agg.nodes += g_tdoku_stats.nodes;
            agg.decisions += g_tdoku_stats.decisions;
            agg.fails += g_tdoku_stats.fails;
            agg.select_cycles += g_tdoku_stats.select_cycles;
            agg.soundness_checks += g_tdoku_stats.soundness_checks;
            agg.soundness_violations += g_tdoku_stats.soundness_violations;
            for (int k = 0; k < 7; k++) agg.n_hist[k] += g_tdoku_stats.n_hist[k];
            agg.candidate_pairs += g_tdoku_stats.candidate_pairs;
            agg.b_computations += g_tdoku_stats.b_computations;
            agg.selections += g_tdoku_stats.selections;
            if (g_tdoku_stats.decisions != g) {
                cerr << "decision count mismatch" << endl;
                exit(1);
            }
#endif
            if (opt.validate && opt.limit == 1 && count == 1 &&
                !ValidSolution(puzzles[i].c_str(), solution)) {
                invalid++;
            }
            if (vi > 0 && count != counts[0][i]) mismatches++;
        }
        double tg = 0, sols[3] = {0, 0, 0};
        for (size_t i = 0; i < n; i++) {
            tg += guesses[vi][i];
            sols[min<size_t>(counts[vi][i], 2)]++;
        }
        // geometric mean of (guesses + 1)
        double lg = 0;
        for (size_t i = 0; i < n; i++) lg += log(guesses[vi][i] + 1.0);
        printf("count,%s,%s,%s,%zu,limit=%zu,guesses/puzzle=%.4f,geo(g+1)=%.4f,"
               "sol0=%.0f,sol1=%.0f,sol2+=%.0f",
               opt.tag.c_str(), filename.c_str(), v.name.c_str(), n, opt.limit,
               tg / n, exp(lg / n), sols[0], sols[1], sols[2]);
#ifdef TDOKU_STATS
        printf(",nodes/puzzle=%.4f,fails/puzzle=%.4f,selcyc/node=%.1f,"
               "n2=%.4f,n3=%.4f,n4+=%.4f,sound=%llu/%llu",
               agg.nodes / (double) n, agg.fails / (double) n,
               agg.select_cycles / (double) max<uint64_t>(agg.nodes, 1),
               agg.n_hist[2] / (double) max<uint64_t>(agg.decisions, 1),
               agg.n_hist[3] / (double) max<uint64_t>(agg.decisions, 1),
               (agg.n_hist[4] + agg.n_hist[5] + agg.n_hist[6]) /
               (double) max<uint64_t>(agg.decisions, 1),
               (unsigned long long) agg.soundness_violations,
               (unsigned long long) agg.soundness_checks);
        if (agg.selections) {
            printf(",pairs/sel=%.2f,bcomp/sel=%.2f", agg.candidate_pairs / (double) agg.selections,
                   agg.b_computations / (double) agg.selections);
        }
#endif
        printf("\n");
        fflush(stdout);
    }
#ifdef TDOKU_STATS
    if (!opt.trace_out.empty()) {
        // re-run each variant with tracing enabled and dump one row per decision
        ofstream out(opt.trace_out, ios::app);
        for (size_t vi = 0; vi < opt.variants.size(); vi++) {
            Activate(opt.variants[vi]);
            g_tdoku_trace_enabled = true;
            uint64_t checks = 0, violations = 0;
            for (size_t i = 0; i < n; i++) {
                char solution[82];
                size_t g;
                g_tdoku_trace.clear();
                g_tdoku_stats = TdokuStats{};
                TdokuSolverDpllTriadSimd(puzzles[i].c_str(), opt.limit,
                                         opt.variants[vi].heuristic << 8u, solution, &g);
                checks += g_tdoku_stats.soundness_checks;
                violations += g_tdoku_stats.soundness_violations;
                for (const auto &t : g_tdoku_trace) {
                    out << opt.tag << "," << filename << "," << i << "," << opt.variants[vi].name
                        << "," << (int) t.n << "," << (int) t.cfgmask << "," << (int) t.asserted
                        << "," << (int) t.ok << "," << t.total_before << "," << t.total_after;
                    for (int y = 0; y < 3; y++) out << "," << (int) t.ny[y];
                    for (int y = 0; y < 3; y++) out << "," << (int) t.ny_after[y];
                    for (int y = 0; y < 3; y++) for (int e = 0; e < 3; e++) out << "," << (int) t.K[y][e];
                    out << "\n";
                }
            }
            g_tdoku_trace_enabled = false;
            printf("trace,%s,%s,%s,soundness_violations=%llu,checks=%llu\n", opt.tag.c_str(),
                   filename.c_str(), opt.variants[vi].name.c_str(),
                   (unsigned long long) violations, (unsigned long long) checks);
        }
    }
#endif
#ifdef TDOKU_STATS
    if (opt.oracle) {
        // decomposition of the complete (limit 2) search of uniquely solvable puzzles into
        // P (decisions on the solution path), W1 (refuted subtrees explored before the solution
        // because the first branch was wrong) and W2 (refuted subtrees after the solution).
        vector<string> sols(n);
        vector<bool> unique(n);
        for (size_t i = 0; i < n; i++) {
            char sol[82] = {0};
            size_t g;
            TdokuSolverDpllTriadSimd(puzzles[i].c_str(), 1, 1, sol, &g);
            unique[i] = TdokuSolverDpllTriadSimd(puzzles[i].c_str(), 2, 0, sol + 0, &g) == 1;
            TdokuSolverDpllTriadSimd(puzzles[i].c_str(), 1, 1, sol, &g);
            sols[i] = string(sol, 81);
        }
        ofstream pp;
        if (!opt.per_puzzle_out.empty()) pp.open(opt.per_puzzle_out + ".oracle", ios::app);
        for (size_t vi = 0; vi < opt.variants.size(); vi++) {
            Activate(opt.variants[vi]);
            double T = 0, P = 0, W1 = 0, W2 = 0, C = 0, L1 = 0, m = 0, IMM = 0;
            size_t inconsistent = 0;
            for (size_t i = 0; i < n; i++) {
                if (!unique[i]) continue;
                memcpy(g_oracle_solution, sols[i].data(), 81);
                g_oracle_enabled = true;
                char sol[82];
                size_t g2, g1;
                g_tdoku_stats = TdokuStats{};
                TdokuSolverDpllTriadSimd(puzzles[i].c_str(), 2, opt.variants[vi].heuristic << 8u,
                                         sol, &g2);
                TdokuStats st = g_tdoku_stats;
                TdokuSolverDpllTriadSimd(puzzles[i].c_str(), 1, (opt.variants[vi].heuristic << 8u) | 1u,
                                         sol, &g1);
                g_oracle_enabled = false;
                if (st.path_decisions + st.w_wrong_first + st.w_after_solution != g2 ||
                    st.path_decisions + st.w_wrong_first != g1) inconsistent++;
                T += g2; P += st.path_decisions; W1 += st.w_wrong_first; W2 += st.w_after_solution;
                C += st.path_correct_first; L1 += g1; m++; IMM += st.immediate_refutations;
                if (pp) pp << opt.tag << "," << filename << "," << i << "," << opt.variants[vi].name
                           << "," << g2 << "," << st.path_decisions << "," << st.w_wrong_first
                           << "," << st.w_after_solution << "," << st.path_correct_first << ","
                           << g1 << "\n";
            }
            printf("oracle,%s,%s,%s,unique=%.0f,T=%.3f,P=%.3f,W1=%.3f,W2=%.3f,"
                   "first_branch_correct=%.4f,L1=%.3f,immediate_refutation=%.4f,inconsistent=%zu\n",
                   opt.tag.c_str(), filename.c_str(), opt.variants[vi].name.c_str(), m, T / m,
                   P / m, W1 / m, W2 / m, P > 0 ? C / P : 0.0, L1 / m, P > 0 ? IMM / P : 0.0, inconsistent);
        }
    }
    if (!opt.decisions_out.empty()) {
        ofstream out(opt.decisions_out, ios::app);
        for (size_t vi = 0; vi < opt.variants.size(); vi++) {
            Activate(opt.variants[vi]);
            g_tdoku_decisions_enabled = true;
            for (size_t i = 0; i < n; i++) {
                char solution[82];
                size_t g;
                g_tdoku_decisions.clear();
                g_tdoku_stats = TdokuStats{};
                TdokuSolverDpllTriadSimd(puzzles[i].c_str(), opt.limit,
                                         opt.variants[vi].heuristic << 8u, solution, &g);
                for (const auto &d : g_tdoku_decisions) {
                    out << opt.tag << "," << filename << "," << i << "," << opt.variants[vi].name
                        << "," << d.depth << "," << d.decisions_before << "," << d.fails_before
                        << "," << d.solutions_before << "," << d.total_configs << ","
                        << (int) d.unfixed_bands << "," << (int) d.n2_pairs << "," << d.subtree
                        << "," << d.refutations << "," << d.fails_since_solution << ","
                        << d.n_candidates << "," << d.score_min << "," << d.score_max << ","
                        << d.score_mean << "\n";
                }
            }
            g_tdoku_decisions_enabled = false;
        }
    }
#endif
    if (mismatches || invalid) {
        printf("ERROR,%s,solution count mismatches=%zu,invalid solutions=%zu\n",
               filename.c_str(), mismatches, invalid);
    }
    if (!opt.per_puzzle_out.empty()) {
        ofstream out(opt.per_puzzle_out, ios::app);
        for (size_t i = 0; i < n; i++) {
            for (size_t vi = 0; vi < opt.variants.size(); vi++) {
                out << opt.tag << "," << filename << "," << i << "," << opt.variants[vi].name << ","
                    << guesses[vi][i] << "," << counts[vi][i];
#ifdef TDOKU_STATS
                out << "," << nodes[vi][i] << "," << fails[vi][i] << "," << sel[vi][i];
#endif
                out << "\n";
            }
        }
    }
}

void RunTime(const string &filename, const vector<string> &puzzles, const Options &opt,
             double ghz) {
    size_t n = puzzles.size();
    size_t nv = opt.variants.size();
    // contiguous, aligned puzzle buffer like run_benchmark
    vector<char> buf(n * 96, 0);
    for (size_t i = 0; i < n; i++) memcpy(&buf[i * 96], puzzles[i].c_str(), 81);
    bool per_puzzle = !opt.per_puzzle_out.empty();
    vector<vector<uint32_t>> cycles;  // [variant*reps + rep][puzzle]
    if (per_puzzle) cycles.assign(nv * opt.reps, vector<uint32_t>(n));
    vector<vector<double>> totals(nv);
    char solution[82];
    size_t g;
    uint32_t sink = 0;

    auto run_pass = [&](size_t vi, uint32_t *record) {
        const Variant &v = opt.variants[vi];
        Activate(v);
        uint32_t config = (v.heuristic << 8u) | (opt.limit == 1 ? 1u : 0u);
        uint64_t total = 0;
        for (size_t i = 0; i < n; i++) {
            uint64_t t0 = __rdtsc();
            sink += (uint32_t) TdokuSolverDpllTriadSimd(&buf[i * 96], opt.limit, config,
                                                        solution, &g);
            uint64_t dt = __rdtsc() - t0;
            total += dt;
            if (record) record[i] = (uint32_t) min<uint64_t>(dt, UINT32_MAX);
        }
        return total;
    };

    // warmup: each variant for ~warmup_seconds (at least one pass)
    for (size_t vi = 0; vi < nv; vi++) {
        auto start = chrono::steady_clock::now();
        do { run_pass(vi, nullptr); }
        while (chrono::duration<double>(chrono::steady_clock::now() - start).count() <
               opt.warmup_seconds);
    }
    mt19937_64 rng(12345);
    vector<size_t> order(nv);
    for (size_t vi = 0; vi < nv; vi++) order[vi] = vi;
    for (int rep = 0; rep < opt.reps; rep++) {
        shuffle(order.begin(), order.end(), rng);
        for (size_t vi : order) {
            uint32_t *rec = per_puzzle ? cycles[vi * opt.reps + rep].data() : nullptr;
            totals[vi].push_back((double) run_pass(vi, rec));
        }
    }
    for (size_t vi = 0; vi < nv; vi++) {
        vector<double> ns;
        for (double t : totals[vi]) ns.push_back(t / ghz / n);
        double med = Median(ns);
        vector<double> dev;
        for (double x : ns) dev.push_back(fabs(x - med));
        double mad = Median(dev);
        vector<double> ratio;
        for (int rep = 0; rep < opt.reps; rep++) ratio.push_back(totals[vi][rep] / totals[0][rep]);
        sort(ratio.begin(), ratio.end());
        printf("time,%s,%s,%s,%zu,limit=%zu,median_ns=%.1f,min_ns=%.1f,mad_ns=%.1f,"
               "ratio_med=%.4f,ratio_min=%.4f,ratio_max=%.4f,reps=%d\n",
               opt.tag.c_str(), filename.c_str(), opt.variants[vi].name.c_str(), n, opt.limit,
               med, *min_element(ns.begin(), ns.end()), mad, Median(ratio), ratio.front(),
               ratio.back(), opt.reps);
    }
    fflush(stdout);
    if (per_puzzle) {
        ofstream out(opt.per_puzzle_out, ios::app);
        for (size_t i = 0; i < n; i++) {
            for (size_t vi = 0; vi < nv; vi++) {
                vector<double> c;
                for (int rep = 0; rep < opt.reps; rep++) c.push_back(cycles[vi * opt.reps + rep][i]);
                out << opt.tag << "," << filename << "," << i << "," << opt.variants[vi].name
                    << "," << Median(c) / ghz << "\n";
            }
        }
    }
    if (sink == 0xdeadbeef) printf(" ");
}

}  // namespace

int main(int argc, char **argv) {
    Options opt;
    vector<string> files;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        auto next = [&]() -> string {
            if (i + 1 >= argc) { cerr << "missing value for " << a << endl; exit(1); }
            return argv[++i];
        };
        if (a == "-m") opt.mode = next();
        else if (a == "-v") {
            stringstream ss(next());
            string item;
            while (getline(ss, item, ';')) if (!item.empty()) opt.variants.push_back(ParseVariant(item));
        }
        else if (a == "-n") opt.max_puzzles = stoull(next());
        else if (a == "-x") opt.transform_seed = stoull(next());
        else if (a == "-l") opt.limit = stoull(next());
        else if (a == "-r") opt.reps = stoi(next());
        else if (a == "-w") opt.warmup_seconds = stod(next());
        else if (a == "-o") opt.per_puzzle_out = next();
        else if (a == "-t") opt.tag = next();
        else if (a == "-T") opt.trace_out = next();
        else if (a == "-D") opt.decisions_out = next();
        else if (a == "-O") opt.oracle = true;
        else if (a == "-V") opt.validate = stoi(next()) != 0;
        else if (a == "-h") {
            cout << "see header comment in src/research_bench.cc" << endl;
            return 0;
        }
        else files.push_back(a);
    }
    if (opt.variants.empty()) opt.variants.push_back(ParseVariant("orig=h:0"));
    double ghz = opt.mode == "time" ? TscGhz() : 0.0;
    if (opt.mode == "time") fprintf(stderr, "tsc GHz %.4f\n", ghz);
    for (const string &f : files) {
        auto puzzles = LoadPuzzles(f, opt);
        if (opt.mode == "count") RunCount(f, puzzles, opt);
        else RunTime(f, puzzles, opt, ghz);
    }
    return 0;
}
