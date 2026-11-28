// End-to-end guess counts of a general-score policy applied at every decision (no gate),
// limit 2. The score mirrors make_score() in evaluate.py, including the order of floating-point
// operations, so offline and end-to-end choices agree. Build with -ffp-contract=off.
//
// usage: policy_eval <puzzles> <max_puzzles> [mrv] [cap=N] [key=value ...]
//   keys: wR f3 f4 a bm wprod wsp wsm wnob   (f3/f4 = inf filters n = 3 / n >= 4 out)
// prints: n, mean guesses, geomean(guesses + 1), solution-count histogram, puzzles stopped at
// the guess cap (their counts are lower bounds)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "features.h"

namespace {

struct Params {
    bool mrv = false;
    double wR = 0, f3 = INFINITY, f4 = INFINITY, a = 1, bm = 1, wprod = 0, wsp = 0, wsm = 0, wnob = 0;
};

double Score(const Snap &s, const Params &p, int b, const Lanes &x, int R, int has2, int d, int n,
             unsigned all, int c) {
    unsigned rest = all & ~(1u << c);
    double rp[3], rm[3];
    int sp_zero = 0, sm_zero = 0;
    for (int j = 0; j < 3; j++) {
        int k, m;
        Crossing(s, b, d, 1u << c, j, k, m);
        rp[j] = (double) k / (double) m;
        Crossing(s, b, d, rest, j, k, m);
        rm[j] = (double) k / (double) m;
        int r = kElem[c][j];
        int poss = Possible(x, d, j, r);
        sp_zero += poss - 2 == 0;
        bool still = false;
        for (int c2 = 0; c2 < 6; c2++)
            if ((rest >> c2 & 1) && kElem[c2][j] == r) still = true;
        if (!still) sm_zero += poss - 3 == 0;
    }
    double xp = 0.0 + rp[0] + rp[1] + rp[2], xm = 0.0 + rm[0] + rm[1] + rm[2];
    double xp_prod = rp[0] * rp[1] * rp[2], xm_prod = rm[0] * rm[1] * rm[2];
    double v;
    if (n == 2) v = xp + xm;
    else if (n == 3) {
        if (std::isinf(p.f3)) return INFINITY;
        v = p.f3 + p.a * xp + p.bm * xm;
    } else {
        if (std::isinf(p.f4)) return INFINITY;
        v = p.f4 + p.a * xp + p.bm * xm;
    }
    v += p.wR * R + p.wprod * (xp_prod + xm_prod);
    v -= p.wsp * sp_zero + p.wsm * sm_zero;
    v += p.wnob * (1 - has2) * R;
    return v;
}

// false if every band is fixed
bool Choose(const State &s, const Params &p, int &bb, int &bd, int &bc) {
    if (!ChooseMRV(s, bb, bd)) return false;
    bc = LowConfig(BandLanes(s, bb), bd);
    if (p.mrv) return true;
    double best = INFINITY;
    Snap snap(s);
    for (int b = 0; b < 6; b++) {
        const Lanes &x = snap.band[b];
        int R = Popcount(x);
        if (R < 10) continue;
        int has2 = 0;
        for (int e = 0; e < 9; e++) has2 |= CountDigit(x, e) == 2;
        for (int d = 0; d < 9; d++) {
            int n = CountDigit(x, d);
            if (n < 2) continue;
            if (n == 3 && std::isinf(p.f3)) continue;
            if (n >= 4 && std::isinf(p.f4)) continue;
            unsigned all = 0;
            for (int c = 0; c < 6; c++) all |= (unsigned) (x.l[c] >> d & 1) << c;
            for (int c = 0; c < 6; c++) {
                if (!(all >> c & 1)) continue;
                if (n == 2 && c != LowConfig(x, d)) continue;
                double v = Score(snap, p, b, x, R, has2, d, n, all, c);
                if (v < best) { best = v; bb = b; bd = d; bc = c; }
            }
        }
    }
    return true;  // no admissible candidate: the exact-MRV choice stands
}

struct Search {
    const Params *p;
    size_t guesses = 0, sols = 0, cap = SIZE_MAX;
    void Count(State &s) {
        if (guesses >= cap) return;
        int b, d, c;
        if (!Choose(s, *p, b, d, c)) { sols++; return; }
        guesses++;
        State copy = s;
        AddElims(copy, b, d, c, true);
        if (Eliminate(copy, b)) Count(copy);
        if (sols >= 2 || guesses >= cap) return;
        AddElims(s, b, d, c, false);
        if (Eliminate(s, b)) Count(s);
    }
};

}  // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: policy_eval <puzzles> <max_puzzles> [mrv] [key=value ...]\n");
        return 1;
    }
    Params p;
    size_t cap = SIZE_MAX, capped = 0;
    for (int i = 3; i < argc; i++) {
        std::string a = argv[i];
        if (a == "mrv") { p.mrv = true; continue; }
        if (a.rfind("cap=", 0) == 0) { cap = (size_t) atol(a.c_str() + 4); continue; }
        size_t eq = a.find('=');
        std::string k = a.substr(0, eq);
        double v = a.substr(eq + 1) == "inf" ? INFINITY : atof(a.substr(eq + 1).c_str());
        if (k == "wR") p.wR = v; else if (k == "f3") p.f3 = v; else if (k == "f4") p.f4 = v;
        else if (k == "a") p.a = v; else if (k == "bm") p.bm = v; else if (k == "wprod") p.wprod = v;
        else if (k == "wsp") p.wsp = v; else if (k == "wsm") p.wsm = v; else if (k == "wnob") p.wnob = v;
        else { fprintf(stderr, "unknown key %s\n", k.c_str()); return 1; }
    }
    size_t max_p = (size_t) atol(argv[2]);
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }
    static char line[8192];
    size_t np = 0, sol_hist[3] = {0, 0, 0};
    double sum = 0, logsum = 0;
    while (np < max_p && fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        size_t n = strcspn(line, " \t\r\n,:");
        if (n != 729 && n != 81) continue;
        std::vector<char> buf(736, 0);
        memcpy(buf.data(), line, n);
        bool pencilmark = buf[81] >= '.';
        State st;
        Search s{&p};
        s.cap = cap;
        if (pencilmark ? Base::InitPencilmarkByBox(buf.data(), st) : Base::InitVanillaByBand(buf.data(), st))
            s.Count(st);
        np++;
        sum += (double) s.guesses;
        logsum += log((double) s.guesses + 1);
        sol_hist[s.sols < 2 ? s.sols : 2]++;
        capped += s.guesses >= cap;
    }
    fclose(f);
    printf("n %zu mean %.3f geo1 %.3f sols %zu/%zu/%zu capped %zu\n", np, sum / (double) np,
           exp(logsum / (double) np), sol_hist[0], sol_hist[1], sol_hist[2], capped);
    return 0;
}
