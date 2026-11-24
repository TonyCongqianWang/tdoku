// Tests for the intersection-aware branching research code.
//
//  1. geometry tables (configuration permutations, compatibility table)
//  2. fast compatibility masks vs. an explicit reference on random states
//     (all bands/orientations, digits, configurations, opposite bands; empty/single/full masks)
//  3. negative-branch support masks vs. an explicit reference
//  4. solver contract for every heuristic: solution counts (limit up to 100000), identical
//     solution *sets* via enumeration, solution validity, invariance under random
//     transformations and transposition, pencilmark inputs, empty/underconstrained puzzles,
//     invalid puzzles.
//
// usage: ix_tests [puzzle_file [max_puzzles]]   (default: test/test_puzzles)

#include "../include/tdoku.h"
#include "../src/branch_features.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

extern "C" void TdokuSetResearchParams(const TdokuResearchParams *params);

using namespace std;
using namespace tdoku_ix;

static int g_failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { g_failures++; \
    if (g_failures < 50) cerr << "FAIL: " << msg << " (" #cond ") line " << __LINE__ << endl; } } while (0)

// ---------------------------------------------------------------------------------------------
// 1. geometry

void TestGeometry() {
    set<array<int, 3>> perms;
    for (int c = 0; c < 6; c++) {
        array<int, 3> p{kElem[c][0], kElem[c][1], kElem[c][2]};
        array<int, 3> sorted = p;
        sort(sorted.begin(), sorted.end());
        CHECK((sorted == array<int, 3>{0, 1, 2}), "config is not a permutation");
        perms.insert(p);
    }
    CHECK(perms.size() == 6, "configs are not the 6 distinct permutations");
    // compat table: popcount(t[p][1<<e]) == 2 (each element used by 2 configs per peer)
    for (int p = 0; p < 3; p++) {
        CHECK(kCompat.t[p][0] == 0, "empty mask must be incompatible with everything");
        CHECK(kCompat.t[p][7] == 0x3f, "full mask must be compatible with everything");
        uint32_t uni = 0;
        for (int e = 0; e < 3; e++) {
            CHECK(__builtin_popcount(kCompat.t[p][1 << e]) == 2, "2 configs per element");
            CHECK((kCompat.t[p][1 << e] & uni) == 0, "configs per element must be disjoint");
            uni |= kCompat.t[p][1 << e];
            CHECK(kCompat.t[p][1 << e] == kElemTables.configs_using_elem[p][e], "elem tables");
        }
        for (int a = 0; a < 8; a++) {
            for (int b2 = 0; b2 < 8; b2++) {
                CHECK(kCompat.t[p][a | b2] == (kCompat.t[p][a] | kCompat.t[p][b2]), "union");
            }
        }
    }
    // the solver's peer_x_elem_to_config_mask table (copied from solver_dpll_triad_simd.cc)
    // lists, for (peer, elem), the configs *inconsistent* with the digit at that elem.
    const int K = 1;
    const int peer_x_elem_to_config_mask[3][3][6]{
            {{0, K, K, K, 0, K}, {K, 0, K, K, K, 0}, {K, K, 0, 0, K, K}},
            {{K, K, 0, K, K, 0}, {0, K, K, 0, K, K}, {K, 0, K, K, 0, K}},
            {{K, 0, K, 0, K, K}, {K, K, 0, K, 0, K}, {0, K, K, K, K, 0}}};
    for (int p = 0; p < 3; p++) {
        for (int e = 0; e < 3; e++) {
            for (int c = 0; c < 6; c++) {
                bool consistent = peer_x_elem_to_config_mask[p][e][c] == 0;
                CHECK(consistent == (kElem[c][p] == e), "kElem vs solver elimination table");
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// 2./3. compatibility primitive vs. reference

void RandomState(RawState &s, mt19937_64 &rng, double cell_density, double cfg_density) {
    uniform_real_distribution<double> u(0.0, 1.0);
    memset(&s, 0, sizeof(s));
    for (int b = 0; b < 9; b++) {
        for (int lane = 0; lane < 16; lane++) {
            uint16_t m = 0;
            for (int d = 0; d < 9; d++) if (u(rng) < cell_density) m |= 1u << d;
            // fill negative-triad lanes with junk too: they must be ignored
            s.box[b][lane] = m;
        }
    }
    for (int b = 0; b < 6; b++) {
        for (int c = 0; c < 6; c++) {
            uint16_t m = 0;
            for (int d = 0; d < 9; d++) if (u(rng) < cfg_density) m |= 1u << d;
            s.band[b][c] = m;
        }
    }
}

void TestCompat() {
    mt19937_64 rng(42);
    const double densities[] = {0.0, 0.1, 0.3, 0.5, 0.8, 1.0};
    long checks = 0;
    for (int trial = 0; trial < 3000; trial++) {
        RawState s;
        RandomState(s, rng, densities[trial % 6], densities[(trial / 6) % 6]);
        for (int b = 0; b < 6; b++) {
            for (int d = 0; d < 9; d++) {
                uint8_t M[3][3];
                CompatMasks(s, b, d, M);
                for (int pi = 0; pi < 6; pi++) {
                    for (int y = 0; y < 3; y++) {
                        int fast = __builtin_popcount(M[y][kElem[pi][y]]);
                        int ref = CompatCountReference(s, b, d, pi, y);
                        CHECK(fast == ref, "compat count b=" << b << " d=" << d << " pi=" << pi
                                           << " y=" << y << " fast=" << fast << " ref=" << ref);
                        checks++;
                    }
                }
                // negative branch support: sigma compatible with at least one config in rest
                uint32_t cfg = ConfigMask(s, b, d);
                for (uint32_t rest = 0; rest < 64; rest++) {
                    if ((rest & cfg) != rest) continue;
                    for (int y = 0; y < 3; y++) {
                        uint32_t fast = NegativeCompatMask(M, y, rest);
                        int opp = OppositeBand(b, y), box = SharedBox(b, y), x = b % 3;
                        uint32_t ref = 0;
                        for (int sigma = 0; sigma < 6; sigma++) {
                            if (!((s.band[opp][sigma] >> d) & 1)) continue;
                            for (int rho = 0; rho < 6; rho++) {
                                if (!((rest >> rho) & 1)) continue;
                                int re = kElem[rho][y], se = kElem[sigma][x];
                                int row = b < 3 ? re : se, col = b < 3 ? se : re;
                                if ((s.box[box][row * 4 + col] >> d) & 1) ref |= 1u << sigma;
                            }
                        }
                        CHECK(fast == ref, "negative compat mask");
                        checks++;
                    }
                }
            }
        }
    }
    // structured edge cases: single candidate cell per shared box, each permutation
    for (int b = 0; b < 6; b++) {
        for (int y = 0; y < 3; y++) {
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    RawState s;
                    memset(&s, 0, sizeof(s));
                    for (int bb = 0; bb < 6; bb++) for (int c = 0; c < 6; c++) s.band[bb][c] = 0x1ff;
                    s.box[SharedBox(b, y)][row * 4 + col] = 1u << 4;  // digit 5 only here
                    uint8_t M[3][3];
                    CompatMasks(s, b, 4, M);
                    int e_this = b < 3 ? row : col;  // b's element at which the cell lies
                    for (int pi = 0; pi < 6; pi++) {
                        int k = __builtin_popcount(M[y][kElem[pi][y]]);
                        // pi compatible iff it uses the cell's element; then exactly 2 opposite
                        // configs (those using the other coordinate in the shared box) remain.
                        CHECK(k == (kElem[pi][y] == e_this ? 2 : 0), "single-cell edge case");
                        CHECK(k == CompatCountReference(s, b, 4, pi, y), "single-cell ref");
                        checks++;
                    }
                }
            }
        }
    }
    cout << "compat checks: " << checks << endl;
}

// ---------------------------------------------------------------------------------------------
// 4. solver contract across heuristics

struct H {
    string name;
    uint32_t id;
    TdokuResearchParams params;
};

vector<H> Heuristics() {
    vector<H> hs;
    hs.push_back({"orig", 0, {}});
    // research selector configurations spanning every rule and score
    hs.push_back({"r_orig", 1, {1, 0.5, 1.0, 0, 0.0, 0}});
    hs.push_back({"r_exact", 1, {2, 0.5, 1.0, 0, 0.0, 0}});
    hs.push_back({"r_flat", 1, {3, 0.5, 1.0, 0, 0.0, 0}});
    hs.push_back({"r_soft", 1, {5, 0.5, 0.0, 0, 0.0, 0}});
    hs.push_back({"r_sum", 1, {3, 0.5, 1.0, 1, 0.0, 0}});
    hs.push_back({"r_gm", 1, {3, 0.5, 1.0, 2, 0.0, 0}});
    hs.push_back({"r_min", 1, {1, 0.5, 1.0, 2, -1000, 1}});
    hs.push_back({"r_G", 1, {0, 0.5, 1.0, 3, 0.0, 0}});
    hs.push_back({"r_two", 1, {3, 0.5, 1.0, 4, 0.0, 0}});
    hs.push_back({"r_twosum", 1, {0, 0.5, 1.0, 5, 0.0, 0}});
    hs.push_back({"r_look", 1, {3, 0.5, 1.0, 6, 0.0, 0}});
    for (uint32_t id = 2; id <= 16; id++) hs.push_back({"h" + to_string(id), id, {}});
    return hs;
}

void Activate(const H &h) {
    if (h.id == 1) TdokuSetResearchParams(&h.params);
}

size_t Count(const H &h, const string &puzzle, size_t limit, char *solution = nullptr) {
    char sol[82];
    size_t g;
    Activate(h);
    return TdokuSolverDpllTriadSimd(puzzle.c_str(), limit, (h.id << 8u) | (solution ? 1u : 0u),
                                    solution ? solution : sol, &g);
}

set<string> SolutionSet(const H &h, const string &puzzle, size_t limit) {
    set<string> out;
    Activate(h);
    TdokuEnumerateWithHeuristic(puzzle.c_str(), limit, h.id,
            [](const char *s, void *arg) { ((set<string> *) arg)->insert(string(s, 81)); }, &out);
    return out;
}

bool Valid(const string &puzzle, const char *sol) {
    for (int i = 0; i < 81; i++) {
        if (sol[i] < '1' || sol[i] > '9') return false;
        if (puzzle[i] != '.' && puzzle[i] != sol[i]) return false;
    }
    for (int i = 0; i < 9; i++) {
        uint32_t r = 0, c = 0, b = 0;
        for (int j = 0; j < 9; j++) {
            r |= 1u << (sol[i * 9 + j] - '1');
            c |= 1u << (sol[j * 9 + i] - '1');
            b |= 1u << (sol[((i / 3) * 3 + j / 3) * 9 + (i % 3) * 3 + j % 3] - '1');
        }
        if (r != 0x1ff || c != 0x1ff || b != 0x1ff) return false;
    }
    return true;
}

string Transpose(const string &p) {
    string t(81, '.');
    for (int r = 0; r < 9; r++) for (int c = 0; c < 9; c++) t[c * 9 + r] = p[r * 9 + c];
    return t;
}

string RandomTransform(const string &p, mt19937_64 &rng) {
    array<int, 9> digits{0, 1, 2, 3, 4, 5, 6, 7, 8}, rows{}, cols{};
    shuffle(digits.begin(), digits.end(), rng);
    auto bs = [&](array<int, 9> &perm) {
        array<int, 3> blocks{0, 1, 2};
        shuffle(blocks.begin(), blocks.end(), rng);
        for (int i = 0; i < 3; i++) {
            array<int, 3> in{0, 1, 2};
            shuffle(in.begin(), in.end(), rng);
            for (int j = 0; j < 3; j++) perm[i * 3 + j] = blocks[i] * 3 + in[j];
        }
    };
    bs(rows);
    bs(cols);
    string out(81, '.');
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            char ch = p[r * 9 + c];
            if (ch != '.') ch = (char) ('1' + digits[ch - '1']);
            out[rows[r] * 9 + cols[c]] = ch;
        }
    }
    return (rng() & 1) ? Transpose(out) : out;
}

// vanilla -> pencilmark, removing a random subset of candidates that are not in `keep`
string ToPencilmark(const string &p, const string &keep, mt19937_64 &rng, double drop) {
    uniform_real_distribution<double> u(0, 1);
    string pm(729, '.');
    for (int cell = 0; cell < 81; cell++) {
        for (int d = 0; d < 9; d++) {
            char ch = (char) ('1' + d);
            bool allowed = p[cell] == '.' ? true : p[cell] == ch;
            if (allowed && !keep.empty() && keep[cell] != ch && u(rng) < drop) allowed = false;
            pm[cell * 9 + d] = allowed ? ch : '.';
        }
    }
    return pm;
}

void TestSolverContract(const vector<pair<string, long>> &cases) {
    auto hs = Heuristics();
    mt19937_64 rng(7);
    long checks = 0;
    for (const auto &tc : cases) {
        const string &puzzle = tc.first;
        long expect = tc.second;  // -1 if unknown
        size_t limit = 100000;
        size_t ref_count = Count(hs[0], puzzle, limit);
        if (expect >= 0) CHECK((long) ref_count == expect, "baseline count " << puzzle);
        set<string> ref_set = SolutionSet(hs[0], puzzle, 2000);
        string transposed = Transpose(puzzle);
        string transformed = RandomTransform(puzzle, rng);
        for (const H &h : hs) {
            size_t c = Count(h, puzzle, limit);
            CHECK(c == ref_count, h.name << " count " << c << " vs " << ref_count << " " << puzzle);
            CHECK(Count(h, transposed, limit) == ref_count, h.name << " transposed count");
            CHECK(Count(h, transformed, limit) == ref_count, h.name << " transformed count");
            for (size_t lim : {1, 2, 3, 7}) {
                size_t cl = Count(h, puzzle, lim);
                CHECK(cl == min(ref_count, lim), h.name << " limited count");
            }
            if (ref_count >= 1) {
                char sol[82] = {0};
                CHECK(Count(h, puzzle, 1, sol) == 1, h.name << " limit 1");
                CHECK(Valid(puzzle, sol), h.name << " invalid solution");
                if (ref_count == 1) {
                    CHECK(*ref_set.begin() == string(sol, 81), h.name << " unique solution differs");
                }
            }
            set<string> sset = SolutionSet(h, puzzle, 2000);
            if (ref_count <= 2000) {
                CHECK(sset == ref_set, h.name << " solution set differs " << puzzle);
            } else {
                CHECK(sset.size() == 2000, h.name << " enumeration limit");
                for (const string &s : sset) CHECK(Valid(puzzle, s.c_str()), "enum valid");
            }
            // pencilmark variant keeping one solution's candidates (if any)
            string keep = ref_count >= 1 ? *ref_set.begin() : string();
            string pm = ToPencilmark(puzzle, keep, rng, 0.3);
            size_t pm_ref = Count(hs[0], pm, limit);
            CHECK(Count(h, pm, limit) == pm_ref, h.name << " pencilmark count");
            if (ref_count >= 1) CHECK(pm_ref >= 1, "pencilmark keeps a solution");
            checks++;
        }
    }
    // empty and highly underconstrained puzzles
    string empty(81, '.');
    for (const H &h : hs) {
        for (size_t lim : {1, 2, 10, 1000}) {
            CHECK(Count(h, empty, lim) == lim, h.name << " empty puzzle");
        }
        char sol[82];
        CHECK(Count(h, empty, 1, sol) == 1 && Valid(empty, sol), h.name << " empty solution");
        set<string> s = SolutionSet(h, empty, 500);
        CHECK(s.size() == 500, h.name << " empty enumerate");
        string pm_empty = ToPencilmark(empty, "", rng, 0.0);
        CHECK(Count(h, pm_empty, 50) == 50, h.name << " pencilmark empty");
        // contradictory: two identical digits in a row
        string bad = empty;
        bad[0] = '5';
        bad[8] = '5';
        CHECK(Count(h, bad, 10) == 0, h.name << " contradictory");
    }
    cout << "solver contract checks: " << checks << " puzzle x heuristic" << endl;
}

int main(int argc, char **argv) {
    string file = argc > 1 ? argv[1] : "test/test_puzzles";
    size_t max_puzzles = argc > 2 ? stoul(argv[2]) : 1000000;
    TestGeometry();
    TestCompat();
    vector<pair<string, long>> cases;
    ifstream in(file);
    string line;
    while (getline(in, line) && cases.size() < max_puzzles) {
        if (line.empty() || line[0] == '#' || line.size() < 81) continue;
        stringstream ss(line);
        string puzzle, expect;
        getline(ss, puzzle, ':');
        long e = -1;
        if (getline(ss, expect, ':') && !expect.empty()) e = stol(expect);
        puzzle = puzzle.substr(0, 81);
        for (char &c : puzzle) if (c < '1' || c > '9') c = '.';
        cases.emplace_back(puzzle, e);
    }
    TestSolverContract(cases);
    if (g_failures) {
        cout << "FAILURES: " << g_failures << endl;
        return 1;
    }
    cout << "ALL PASS" << endl;
    return 0;
}
