// End-to-end guess counts of a general-score policy (no gate, limit 2), with per-decision
// precomputed crossing counts and the cross-term extensions of the score family.
//
// The base score is identical to policy_eval.cc (same floating-point operation order, so the
// same parameters give the same guess counts); extra terms are only added when their weight is
// nonzero. Build with -ffp-contract=off.
//
// usage: policy2 <puzzles> <max_puzzles> [mrv] [cap=N] [check] [key=value ...]
//   base keys:  wR f3 f4 a bm wprod wsp wsm wnob            (f3/f4 = inf filters n = 3 / n >= 4)
//   cross keys: wpp wpm      extra weight on prod(+) / prod(-)
//               wbal         prod(+) * prod(-)
//               wRp          R * (prod(+) + prod(-))
//               wnp          (n - 2) * (prod(+) + prod(-))
//               wmax         max(prod(+), prod(-))
//               wown         prod(+) / n + prod(-) * (n - 1) / n
//               wv wlv       sum over c' in C(B,d) of prod(+ at c'), and log(1e-3 + that sum)
//               wnm          n - m(B)
//               wsq          sum_j rho_j^2 over both branches
//               a2 b2        weights of sum(+) / sum(-) for n = 2 (default 1, 1)
//   log-volume modes: mode=1 (lv) mode=2 (dv) with al be g3 g4 gR eps, cpick=0|1|2 (dv)
//   nb=k: score only the k unfixed bands with the fewest configurations
//   check: also run the reference Crossing() for every candidate and abort on a mismatch
// prints: n, mean guesses, geomean(guesses + 1), solution-count histogram, capped puzzles
#include <immintrin.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "features.h"

namespace {

struct Params {
    bool mrv = false, check = false;
    double wR = 0, f3 = INFINITY, f4 = INFINITY, a = 1, bm = 1, wprod = 0, wsp = 0, wsm = 0, wnob = 0;
    double wpp = 0, wpm = 0, wbal = 0, wRp = 0, wnp = 0, wmax = 0, wown = 0, wv = 0, wlv = 0, wnm = 0,
           wsq = 0, a2 = 1, b2 = 1;
    // log-volume families: mode 1 = lv (per candidate, + and - branch products),
    // mode 2 = dv (digit volume: sum over the digit's configurations of the + products)
    int mode = 0, cpick = 0, nb = 6;  // nb: score only the nb bands with the fewest configurations  // cpick (dv, n >= 3): 0 lowest config, 1 largest + product, 2 smallest
    double al = 1, be = 0, g3 = 0, g4 = 0, gR = 0, eps = 1e-3;
    // mode 2 (union free): wl log(eps + vs / n^sn) + wlin vs / n^sn + ws ss / n^sn + g(n) + gR R,
    // vs = sum_c prod_j rho_j(c)^al, ss = sum_c sum_j rho_j(c), over the digit's configurations
    double wl = 1, wlin = 0, ws = 0, sn = 0;
    // mode 1, n >= 3: - branch volume 0 exact union, 1 union over all of C(B,d) (per peer),
    // 2 sum of the other configurations' + volumes, 3 their maximum
    int lvm = 0;
};

const uint64_t kSpread = 0x111111111ull;  // one nibble per digit

// per-decision precomputation
struct Pre {
    Snap snap;
    int R[6];
    uint64_t ncnt[6];        // per-digit configuration counts (nibbles)
    uint64_t K[6][3][8];     // per-digit crossing counts for row sets 1..7 of peer j of band b
    uint16_t rowcfg[3][3];   // configs using element e of peer j (bitmask)
    uint16_t useelem[6][3][3];  // digits that can use element e of peer j of band b

    explicit Pre(const State &s) : snap(s) {
        for (int j = 0; j < 3; j++)
            for (int e = 0; e < 3; e++) {
                rowcfg[j][e] = 0;
                for (int c = 0; c < 6; c++) if (kElem[c][j] == e) rowcfg[j][e] |= 1u << c;
            }
        for (int b = 0; b < 6; b++) {
            R[b] = Popcount(snap.band[b]);
            uint64_t n = 0;
            for (int c = 0; c < 6; c++) n += _pdep_u64(snap.band[b].l[c], kSpread);
            ncnt[b] = n;
        }
        for (int b = 0; b < 6; b++) {
            if (R[b] < 10) continue;
            const Lanes &x = snap.band[b];
            for (int j = 0; j < 3; j++) {
                for (int e = 0; e < 3; e++) {
                    uint16_t u = 0;
                    for (int c = 0; c < 6; c++) if (kElem[c][j] == e) u |= x.l[c];
                    useelem[b][j][e] = u;
                }
                const Lanes &yl = snap.band[b < 3 ? 3 + j : j];
                const uint16_t *cells = snap.cell[BoxOf(b, j)];
                int p = b % 3;
                uint16_t sv[3][6];
                for (int e = 0; e < 3; e++)
                    for (int cy = 0; cy < 6; cy++) {
                        int ey = kElem[cy][p];
                        int row = b < 3 ? e : ey, col = b < 3 ? ey : e;
                        sv[e][cy] = yl.l[cy] & cells[row * 4 + col];
                    }
                K[b][j][0] = 0;
                for (int set = 1; set < 8; set++) {
                    uint64_t k = 0;
                    for (int cy = 0; cy < 6; cy++) {
                        uint16_t v = 0;
                        for (int e = 0; e < 3; e++) if (set >> e & 1) v |= sv[e][cy];
                        k += _pdep_u64(v, kSpread);
                    }
                    K[b][j][set] = k;
                }
            }
        }
    }
    static int Nib(uint64_t v, int d) { return (int) (v >> (4 * d) & 15); }
    int Ncross(int b, int j, int d) const { return Nib(ncnt[b < 3 ? 3 + j : j], d); }
    // row set of peer j used by the configurations in S
    int Rows(unsigned S, int j) const {
        int r = 0;
        for (int e = 0; e < 3; e++) if (S & rowcfg[j][e]) r |= 1 << e;
        return r;
    }
};

double Score(const Pre &pre, const Params &p, int b, int R, int m, int has2, int d, int n,
             unsigned all, int c) {
    const Lanes &x = pre.snap.band[b];
    unsigned rest = all & ~(1u << c);
    double rp[3], rm[3];
    int sp_zero = 0, sm_zero = 0;
    for (int j = 0; j < 3; j++) {
        int nj = pre.Ncross(b, j, d);
        int kp = Pre::Nib(pre.K[b][j][1 << kElem[c][j]], d);
        int km = Pre::Nib(pre.K[b][j][pre.Rows(rest, j)], d);
        if (p.check) {
            int k, mm;
            Crossing(pre.snap, b, d, 1u << c, j, k, mm);
            if (k != kp || mm != nj) { fprintf(stderr, "mismatch +\n"); abort(); }
            Crossing(pre.snap, b, d, rest, j, k, mm);
            if (k != km || mm != nj) { fprintf(stderr, "mismatch -\n"); abort(); }
        }
        rp[j] = (double) kp / (double) nj;
        rm[j] = (double) km / (double) nj;
        if (p.wsp != 0 || p.wsm != 0) {
            int r = kElem[c][j];
            int poss = __builtin_popcount(pre.useelem[b][j][r] & ~(1u << d) & 0x1ff);
            if (p.check && poss != Possible(x, d, j, r)) { fprintf(stderr, "mismatch poss\n"); abort(); }
            sp_zero += poss - 2 == 0;
            bool still = (rest & pre.rowcfg[j][r]) != 0;
            if (!still) sm_zero += poss - 3 == 0;
        }
    }
    double xp = 0.0 + rp[0] + rp[1] + rp[2], xm = 0.0 + rm[0] + rm[1] + rm[2];
    double xp_prod = rp[0] * rp[1] * rp[2], xm_prod = rm[0] * rm[1] * rm[2];
    double v;
    if (n == 2) {
        v = (p.a2 == 1 && p.b2 == 1) ? xp + xm : p.a2 * xp + p.b2 * xm;
    } else if (n == 3) {
        if (std::isinf(p.f3)) return INFINITY;
        v = p.f3 + p.a * xp + p.bm * xm;
    } else {
        if (std::isinf(p.f4)) return INFINITY;
        v = p.f4 + p.a * xp + p.bm * xm;
    }
    v += p.wR * R + p.wprod * (xp_prod + xm_prod);
    v -= p.wsp * sp_zero + p.wsm * sm_zero;
    v += p.wnob * (1 - has2) * R;
    // cross terms
    if (p.wpp != 0) v += p.wpp * xp_prod;
    if (p.wpm != 0) v += p.wpm * xm_prod;
    if (p.wbal != 0) v += p.wbal * xp_prod * xm_prod;
    if (p.wRp != 0) v += p.wRp * R * (xp_prod + xm_prod);
    if (p.wnp != 0) v += p.wnp * (n - 2) * (xp_prod + xm_prod);
    if (p.wmax != 0) v += p.wmax * (xp_prod > xm_prod ? xp_prod : xm_prod);
    if (p.wown != 0) v += p.wown * (xp_prod / n + xm_prod * (n - 1) / n);
    if (p.wnm != 0) v += p.wnm * (n - m);
    if (p.wsq != 0) {
        double q = 0;
        for (int j = 0; j < 3; j++) q += rp[j] * rp[j] + rm[j] * rm[j];
        v += p.wsq * q;
    }
    if (p.wv != 0 || p.wlv != 0) {
        double vs = 0;
        for (int c2 = 0; c2 < 6; c2++) {
            if (!(all >> c2 & 1)) continue;
            double pr = 1;
            for (int j = 0; j < 3; j++)
                pr *= (double) Pre::Nib(pre.K[b][j][1 << kElem[c2][j]], d) / (double) pre.Ncross(b, j, d);
            vs += pr;
        }
        v += p.wv * vs + p.wlv * log(1e-3 + vs);
    }
    return v;
}


double LogTerm(const Pre &pre, int b, int d, int c, double al) {
    double pr = 1;
    for (int j = 0; j < 3; j++)
        pr *= (double) Pre::Nib(pre.K[b][j][1 << kElem[c][j]], d) / (double) pre.Ncross(b, j, d);
    return al == 1 ? pr : pow(pr, al);
}

double Penalty(const Params &p, int n, int R) {
    double g = n == 2 ? 0 : n == 3 ? p.g3 : p.g4;
    return g + p.gR * R;
}

// mode lv: log(eps + f+^be P+^al + f-^be P-^al) + g(n) + gR R
double ScoreLv(const Pre &pre, const Params &p, int b, int R, int d, int n, unsigned all, int c) {
    unsigned rest = all & ~(1u << c);
    double pp = 1, pm = 1;
    unsigned mset = p.lvm == 1 && n >= 3 ? all : rest;
    for (int j = 0; j < 3; j++) {
        double nj = (double) pre.Ncross(b, j, d);
        pp *= (double) Pre::Nib(pre.K[b][j][1 << kElem[c][j]], d) / nj;
        pm *= (double) Pre::Nib(pre.K[b][j][pre.Rows(mset, j)], d) / nj;
    }
    if (n >= 3 && p.lvm >= 2) {
        pm = 0;
        for (int c2 = 0; c2 < 6; c2++) {
            if (!(rest >> c2 & 1)) continue;
            double t = LogTerm(pre, b, d, c2, 1.0);
            pm = p.lvm == 2 ? pm + t : (t > pm ? t : pm);
        }
    }
    if (p.al != 1) { pp = pow(pp, p.al); pm = pow(pm, p.al); }
    if (p.be != 0) { pp *= pow(1.0 / n, p.be); pm *= pow((n - 1.0) / n, p.be); }
    return log(p.eps + pp + pm) + Penalty(p, n, R);
}

bool Choose(const State &s, const Params &p, int &bb, int &bd, int &bc) {
    if (!ChooseMRV(s, bb, bd)) return false;
    bc = LowConfig(BandLanes(s, bb), bd);
    if (p.mrv) return true;
    double best = INFINITY;
    Pre pre(s);
    unsigned allowed = 0x3f;
    if (p.nb < 6) {  // the nb unfixed bands with the fewest configurations (lowest index on ties)
        allowed = 0;
        for (int k = 0; k < p.nb; k++) {
            int bk = -1;
            for (int b = 0; b < 6; b++)
                if (!(allowed >> b & 1) && pre.R[b] >= 10 && (bk < 0 || pre.R[b] < pre.R[bk])) bk = b;
            if (bk >= 0) allowed |= 1u << bk;
        }
    }
    for (int b = 0; b < 6; b++) {
        const Lanes &x = pre.snap.band[b];
        int R = pre.R[b];
        if (R < 10 || !(allowed >> b & 1)) continue;
        int has2 = 0, m = 100;
        for (int e = 0; e < 9; e++) {
            int n = Pre::Nib(pre.ncnt[b], e);
            has2 |= n == 2;
            if (n >= 2 && n < m) m = n;
        }
        for (int d = 0; d < 9; d++) {
            int n = Pre::Nib(pre.ncnt[b], d);
            if (n < 2) continue;
            if (n == 3 && std::isinf(p.mode ? p.g3 : p.f3)) continue;
            if (n >= 4 && std::isinf(p.mode ? p.g4 : p.f4)) continue;
            unsigned all = 0;
            for (int c = 0; c < 6; c++) all |= (unsigned) (x.l[c] >> d & 1) << c;
            if (p.mode == 2) {
                double vs = 0, ss = 0, ext = 0;
                int pick = -1;
                for (int c = 0; c < 6; c++) {
                    if (!(all >> c & 1)) continue;
                    double t = LogTerm(pre, b, d, c, p.al);
                    vs += t;
                    if (p.ws != 0)
                        for (int j = 0; j < 3; j++)
                            ss += (double) Pre::Nib(pre.K[b][j][1 << kElem[c][j]], d) / (double) pre.Ncross(b, j, d);
                    if (pick < 0 || (p.cpick == 1 && t > ext) || (p.cpick == 2 && t < ext)) { pick = c; ext = t; }
                }
                if (p.sn != 0) { double q = pow((double) n, p.sn); vs /= q; ss /= q; }
                double v = Penalty(p, n, R);
                if (p.wl != 0) v += p.wl * log(p.eps + vs);
                v += p.wlin * vs + p.ws * ss;
                if (v < best) { best = v; bb = b; bd = d; bc = n == 2 ? LowConfig(x, d) : pick; }
                continue;
            }
            for (int c = 0; c < 6; c++) {
                if (!(all >> c & 1)) continue;
                if (n == 2 && c != LowConfig(x, d)) continue;
                double v = p.mode == 1 ? ScoreLv(pre, p, b, R, d, n, all, c)
                                       : Score(pre, p, b, R, m, has2, d, n, all, c);
                if (v < best) { best = v; bb = b; bd = d; bc = c; }
            }
        }
    }
    return true;
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
        fprintf(stderr, "usage: policy2 <puzzles> <max_puzzles> [mrv] [cap=N] [check] [key=value ...]\n");
        return 1;
    }
    Params p;
    std::map<std::string, double *> keys = {
            {"wR", &p.wR}, {"f3", &p.f3}, {"f4", &p.f4}, {"a", &p.a}, {"bm", &p.bm},
            {"wprod", &p.wprod}, {"wsp", &p.wsp}, {"wsm", &p.wsm}, {"wnob", &p.wnob},
            {"wpp", &p.wpp}, {"wpm", &p.wpm}, {"wbal", &p.wbal}, {"wRp", &p.wRp}, {"wnp", &p.wnp},
            {"wmax", &p.wmax}, {"wown", &p.wown}, {"wv", &p.wv}, {"wlv", &p.wlv}, {"wnm", &p.wnm},
            {"wsq", &p.wsq}, {"a2", &p.a2}, {"b2", &p.b2}, {"al", &p.al}, {"be", &p.be},
            {"g3", &p.g3}, {"g4", &p.g4}, {"gR", &p.gR}, {"eps", &p.eps}, {"wl", &p.wl},
            {"wlin", &p.wlin}, {"ws", &p.ws}, {"sn", &p.sn}};
    double mode = 0, cpick = 0, nb = 6, lvm = 0;
    keys["lvm"] = &lvm;
    keys["nb"] = &nb;
    keys["mode"] = &mode;
    keys["cpick"] = &cpick;
    size_t cap = SIZE_MAX, capped = 0;
    for (int i = 3; i < argc; i++) {
        std::string a = argv[i];
        if (a == "mrv") { p.mrv = true; continue; }
        if (a == "check") { p.check = true; continue; }
        if (a.rfind("cap=", 0) == 0) { cap = (size_t) atol(a.c_str() + 4); continue; }
        size_t eq = a.find('=');
        std::string k = a.substr(0, eq);
        double v = a.substr(eq + 1) == "inf" ? INFINITY : atof(a.substr(eq + 1).c_str());
        auto it = keys.find(k);
        if (it == keys.end()) { fprintf(stderr, "unknown key %s\n", k.c_str()); return 1; }
        *it->second = v;
    }
    p.mode = (int) mode;
    p.cpick = (int) cpick;
    p.nb = (int) nb;
    p.lvm = (int) lvm;
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
