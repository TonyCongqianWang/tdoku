// Per-candidate branching oracle (see SPEC.md).
//
// For each puzzle: search with exact MRV (limit 2), sample up to K decision nodes evenly spaced
// in visiting order, and at every sampled node evaluate every candidate (band, digit, config):
//     cost = 1 + G(+) + G(-),  G = guesses of an exact-MRV rollout of the subtree (limit 2),
// censored at 4 * cost(MRV choice) + 64. Every candidate row carries the raw features of the
// general score, so any score in the family can be evaluated offline.
//
// usage: oracle <puzzles> <dataset> <start> <count> <K>     (CSV on stdout)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "features.h"

namespace {

// exact-MRV search with limit 2. Optionally captures the states at selected decision indices.
struct Search {
    size_t guesses = 0, budget = SIZE_MAX, sols = 0;
    bool aborted = false;
    const std::vector<size_t> *capture_idx = nullptr;
    size_t capture_pos = 0;
    std::vector<std::pair<State, int>> *captured = nullptr;

    void Count(State &s, int depth) {
        if (aborted) return;
        int b, d;
        if (!ChooseMRV(s, b, d)) { sols++; return; }
        if (captured && capture_pos < capture_idx->size() && (*capture_idx)[capture_pos] == guesses) {
            captured->push_back({s, depth});
            capture_pos++;
        }
        if (++guesses > budget) { aborted = true; return; }
        int c = LowConfig(BandLanes(s, b), d);
        State copy = s;
        AddElims(copy, b, d, c, true);
        if (Eliminate(copy, b)) Count(copy, depth + 1);
        if (sols >= 2 || aborted) return;
        AddElims(s, b, d, c, false);
        if (Eliminate(s, b)) Count(s, depth + 1);
    }
};

struct Eval {
    size_t gp, gm;  // guesses below the + and - branches
    bool rp, rm;    // branch refuted by propagation
    bool censored;
    size_t Cost() const { return 1 + gp + gm; }
};

Eval Evaluate(const State &s, int b, int d, int c, size_t budget) {
    Eval e{0, 0, false, false, false};
    State sp = s;
    AddElims(sp, b, d, c, true);
    if (Eliminate(sp, b)) {
        Search r;
        r.budget = budget;
        r.Count(sp, 0);
        e.gp = r.guesses;
        e.censored |= r.aborted;
    } else {
        e.rp = true;
    }
    State sm = s;
    AddElims(sm, b, d, c, false);
    if (Eliminate(sm, b)) {
        Search r;
        r.budget = budget > e.gp ? budget - e.gp : 0;
        r.Count(sm, 0);
        e.gm = r.guesses;
        e.censored |= r.aborted;
    } else {
        e.rm = true;
    }
    return e;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: oracle <puzzles> <dataset> <start> <count> <K>\n");
        return 1;
    }
    const char *dataset = argv[2];
    size_t start = (size_t) atol(argv[3]), count = (size_t) atol(argv[4]), K = (size_t) atol(argv[5]);
    std::vector<std::string> puzzles;
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }
    static char line[8192];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        size_t n = strcspn(line, " \t\r\n,:");
        if (n == 729 || n == 81) puzzles.emplace_back(line, n);
    }
    fclose(f);

    printf("dataset,puzzle,node,nodes,depth,b,d,c,n,R,m,has2,is_mrv,is_orig,is_ix,"
           "kp0,np0,kp1,np1,kp2,np2,km0,nm0,km1,nm1,km2,nm2,sp0,sp1,sp2,sm0,sm1,sm2,"
           "gp,gm,rp,rm,cost,cens\n");
    for (size_t pi = start; pi < puzzles.size() && pi < start + count; pi++) {
        std::vector<char> buf(736, 0);
        memcpy(buf.data(), puzzles[pi].data(), puzzles[pi].size());
        bool pencilmark = buf[81] >= '.';
        State init;
        if (!(pencilmark ? Base::InitPencilmarkByBox(buf.data(), init)
                         : Base::InitVanillaByBand(buf.data(), init))) continue;
        // pass 1: count decision nodes; pass 2: capture K evenly spaced ones
        Search s1;
        State st = init;
        s1.Count(st, 0);
        size_t N = s1.guesses;
        if (N == 0 || s1.sols != 1) continue;  // only unique puzzles that need a guess
        std::vector<size_t> idx;
        for (size_t i = 0; i < K && i < N; i++) {
            size_t v = N <= K ? i : i * N / K;
            if (idx.empty() || idx.back() != v) idx.push_back(v);
        }
        std::vector<std::pair<State, int>> nodes;
        Search s2;
        s2.capture_idx = &idx;
        s2.captured = &nodes;
        st = init;
        s2.Count(st, 0);

        for (size_t ni = 0; ni < nodes.size(); ni++) {
            const State &s = nodes[ni].first;
            int depth = nodes[ni].second;
            int mb, md;
            ChooseMRV(s, mb, md);
            auto orig = Base::ChooseBandAndValueToBranch(s);
            alignas(16) uint16_t ov[8];
            _mm_store_si128((__m128i *) ov, orig.second.vec);
            int ob = (int) orig.first, od = __builtin_ctz(ov[0]);
            auto ix = SolverIntersection<0>::ChooseIx(s);
            int ib = (int) ix.band, id = ix.digit;

            Lanes ml = BandLanes(s, mb);
            Eval ref = Evaluate(s, mb, md, LowConfig(ml, md), (size_t) 200000000);
            size_t budget = 4 * ref.Cost() + 64;

            for (int b = 0; b < 6; b++) {
                Lanes x = BandLanes(s, b);
                int R = Popcount(x);
                if (R < 10) continue;
                int m = 100, has2 = 0;
                for (int e = 0; e < 9; e++) {
                    int n = CountDigit(x, e);
                    if (n >= 2 && n < m) m = n;
                    has2 |= n == 2;
                }
                for (int d = 0; d < 9; d++) {
                    int n = CountDigit(x, d);
                    if (n < 2) continue;
                    unsigned all = 0;
                    for (int c = 0; c < 6; c++) all |= (unsigned) (x.l[c] >> d & 1) << c;
                    for (int c = 0; c < 6; c++) {
                        if (!(all >> c & 1)) continue;
                        if (n == 2 && c != LowConfig(x, d)) continue;  // same subtrees
                        bool is_mrv = b == mb && d == md && c == LowConfig(x, d);
                        Eval e = is_mrv ? ref : Evaluate(s, b, d, c, budget);
                        unsigned rest = all & ~(1u << c);
                        int kp[3], np[3], km[3], nm[3], sp[3], sm[3];
                        for (int j = 0; j < 3; j++) {
                            Crossing(s, b, d, 1u << c, j, kp[j], np[j]);
                            Crossing(s, b, d, rest, j, km[j], nm[j]);
                            int r = kElem[c][j];
                            int poss = Possible(x, d, j, r);
                            sp[j] = poss - 2;
                            bool still = false;
                            for (int c2 = 0; c2 < 6; c2++)
                                if ((rest >> c2 & 1) && kElem[c2][j] == r) still = true;
                            sm[j] = still ? -9 : poss - 3;
                        }
                        printf("%s,%zu,%zu,%zu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%zu,%zu,%d,%d,%zu,%d\n",
                               dataset, pi, idx[ni], N, depth, b, d, c, n, R, m, has2,
                               (int) is_mrv, (int) (b == ob && d == od), (int) (ix.band != UINT32_MAX && b == ib && d == id),
                               kp[0], np[0], kp[1], np[1], kp[2], np[2],
                               km[0], nm[0], km[1], nm[1], km[2], nm[2],
                               sp[0], sp[1], sp[2], sm[0], sm[1], sm[2],
                               e.gp, e.gm, (int) e.rp, (int) e.rm, e.Cost(), (int) e.censored);
                    }
                }
            }
        }
        fflush(stdout);
    }
    return 0;
}
