// End-to-end guess counts of the SIMD candidate choosers (no gate, limit 2), with an optional
// exact check of every choice against a scalar mirror of the fixed-point score.
//
// usage: policy_simd <puzzles> <max_puzzles> <ds|dv|lv|dv2|lv2> [cap=N] [check] [key=value ...]
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "features.h"
#include "simd_ds.h"
#include "simd_dv.h"
#include "simd_lv.h"
#include "simd_lv2.h"
#include "simd_dv2.h"

namespace {

struct Policy {
    virtual bool Choose(const State &s, int &b, int &d, int &c) = 0;
    virtual void Check(const State &s, int b, int d, int c) = 0;
};

struct DS : Policy {
    sds::Params p;
    sds::Tables *t = nullptr;
    bool Choose(const State &s, int &b, int &d, int &c) override { return sds::Choose(s, *t, b, d, c); }
    void Check(const State &s, int cb, int cd, int cc) override {
        unsigned best = 0x10000;
        int bb = -1, bd = -1, bc = -1;
        for (int b = 0; b < 6; b++) {
            Lanes x = BandLanes(s, b);
            int R = Popcount(x);
            if (R < 10) continue;
            for (int d = 0; d < 9; d++) {
                int n = CountDigit(x, d);
                if (n < 2) continue;
                int pairs[3] = {0, 0, 0}, nj[3];
                for (int j = 0; j < 3; j++)
                    for (int c = 0; c < 6; c++) {
                        if (!(x.l[c] >> d & 1)) continue;
                        int k;
                        Crossing(s, b, d, 1u << c, j, k, nj[j]);
                        pairs[j] += k;
                    }
                for (int j = 0; j < 3; j++) if (!pairs[j]) { int k; Crossing(s, b, d, 1, j, k, nj[j]); }
                unsigned v = sds::KeyRef(*t, p, pairs, nj, n, R);
                if (v < best) { best = v; bb = b; bd = d; bc = LowConfig(x, d); }
            }
        }
        if (bb != cb || bd != cd || bc != cc) {
            fprintf(stderr, "DS mismatch: simd %d %d %d  ref %d %d %d\n", cb, cd, cc, bb, bd, bc);
            abort();
        }
    }
};


struct DV : Policy {
    sdv::Params p;
    sdv::Tables *t = nullptr;
    bool Choose(const State &s, int &b, int &d, int &c) override { return sdv::Choose(s, *t, b, d, c); }
    void Check(const State &s, int cb, int cd, int cc) override {
        unsigned best = 0x10000;
        int bb = -1, bd = -1, bc = -1;
        for (int b = 0; b < 6; b++) {
            Lanes x = BandLanes(s, b);
            int R = Popcount(x);
            if (R < 10) continue;
            for (int d = 0; d < 9; d++) {
                int n = CountDigit(x, d);
                if (n < 2) continue;
                unsigned cs = 0;
                int k[6][3] = {}, nj[3];
                for (int c = 0; c < 6; c++) {
                    if (!(x.l[c] >> d & 1)) continue;
                    cs |= 1u << c;
                    for (int j = 0; j < 3; j++) Crossing(s, b, d, 1u << c, j, k[c][j], nj[j]);
                }
                int cbest;
                unsigned v = sdv::KeyRef(*t, cs, k, nj, n, R, cbest);
                if (v < best) { best = v; bb = b; bd = d; bc = cbest; }
            }
        }
        if (bb != cb || bd != cd || bc != cc) {
            fprintf(stderr, "DV mismatch: simd %d %d %d  ref %d %d %d\n", cb, cd, cc, bb, bd, bc);
            abort();
        }
    }
};


struct LV : Policy {
    slv::Params p;
    slv::Tables *t = nullptr;
    bool Choose(const State &s, int &b, int &d, int &c) override { return slv::Choose(s, *t, b, d, c); }
    void Check(const State &s, int cb, int cd, int cc) override {
        unsigned best = 0x10000;
        int bb = -1, bd = -1, bc = -1;
        for (int b = 0; b < 6; b++) {
            Lanes x = BandLanes(s, b);
            int R = Popcount(x);
            if (R < 10) continue;
            for (int d = 0; d < 9; d++) {
                int n = CountDigit(x, d);
                if (n < 2) continue;
                unsigned cs = 0;
                for (int c = 0; c < 6; c++) cs |= (unsigned) (x.l[c] >> d & 1) << c;
                int kp[6][3] = {}, km[6][3] = {}, nj[3];
                for (int c = 0; c < 6; c++) {
                    if (!(cs >> c & 1)) continue;
                    for (int j = 0; j < 3; j++) {
                        Crossing(s, b, d, 1u << c, j, kp[c][j], nj[j]);
                        Crossing(s, b, d, cs & ~(1u << c), j, km[c][j], nj[j]);
                    }
                }
                int cbest = -1;
                unsigned v = slv::KeyRef(*t, cs, kp, km, nj, n, R, cbest);
                if (v < best) { best = v; bb = b; bd = d; bc = cbest; }
            }
        }
        if (bb != cb || bd != cd || bc != cc) {
            fprintf(stderr, "LV mismatch: simd %d %d %d  ref %d %d %d\n", cb, cd, cc, bb, bd, bc);
            abort();
        }
    }
};


struct LV2 : Policy {
    slv2::Params p;
    slv2::Tables *t = nullptr;
    bool Choose(const State &s, int &b, int &d, int &c) override { return slv2::Choose(s, *t, b, d, c); }
    void Check(const State &s, int cb, int cd, int cc) override {
        int best = 0, bb = -1, bd = -1, bc = -1;
        for (int b = 0; b < 6; b++) {
            Lanes x = BandLanes(s, b);
            int R = Popcount(x);
            for (int d = 0; d < 9; d++) {
                int n = CountDigit(x, d);
                if (n < 2) continue;
                unsigned cs = 0;
                for (int c = 0; c < 6; c++) cs |= (unsigned) (x.l[c] >> d & 1) << c;
                int kp[6][3] = {}, km[6][3] = {}, nj[3];
                for (int c = 0; c < 6; c++) {
                    if (!(cs >> c & 1)) continue;
                    for (int j = 0; j < 3; j++) {
                        Crossing(s, b, d, 1u << c, j, kp[c][j], nj[j]);
                        Crossing(s, b, d, cs & ~(1u << c), j, km[c][j], nj[j]);
                    }
                }
                int cbest = -1;
                int y = slv2::KeyRef(*t, cs, kp, km, nj, n, R, cbest);
                if (y > best) { best = y; bb = b; bd = d; bc = cbest; }
            }
        }
        if (bb != cb || bd != cd || bc != cc) {
            fprintf(stderr, "LV2 mismatch: simd %d %d %d  ref %d %d %d\n", cb, cd, cc, bb, bd, bc);
            abort();
        }
    }
};


struct DV2 : Policy {
    sdv2::Params p;
    sdv2::Tables *t = nullptr;
    bool Choose(const State &s, int &b, int &d, int &c) override { return sdv2::Choose(s, *t, b, d, c); }
    void Check(const State &s, int cb, int cd, int cc) override {
        int best = 0, bb = -1, bd = -1, bc = -1;
        for (int b = 0; b < 6; b++) {
            Lanes x = BandLanes(s, b);
            int R = Popcount(x);
            for (int d = 0; d < 9; d++) {
                int n = CountDigit(x, d);
                if (n < 2) continue;
                unsigned cs = 0;
                int k[6][3] = {}, nj[3];
                for (int c = 0; c < 6; c++) {
                    if (!(x.l[c] >> d & 1)) continue;
                    cs |= 1u << c;
                    for (int j = 0; j < 3; j++) Crossing(s, b, d, 1u << c, j, k[c][j], nj[j]);
                }
                int cbest;
                int y = sdv2::KeyRef(*t, cs, k, nj, n, R, cbest);
                if (y > best) { best = y; bb = b; bd = d; bc = cbest; }
            }
        }
        if (bb != cb || bd != cd || bc != cc) {
            fprintf(stderr, "DV2 mismatch: simd %d %d %d  ref %d %d %d\n", cb, cd, cc, bb, bd, bc);
            abort();
        }
    }
};

struct Search {
    Policy *pol;
    bool check = false;
    size_t guesses = 0, sols = 0, cap = SIZE_MAX, checks = 0;
    void Count(State &s) {
        if (guesses >= cap) return;
        int b, d, c;
        if (!pol->Choose(s, b, d, c)) { sols++; return; }
        if (check) { pol->Check(s, b, d, c); checks++; }
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
    if (argc < 4) { fprintf(stderr, "usage: policy_simd <puzzles> <max> <ds|dv|lv|dv2|lv2> [cap=N] [check] [k=v]\n"); return 1; }
    std::string which = argv[3];
    DS ds;
    DV dv;
    LV lv;
    LV2 lv2;
    DV2 dv2;
    Policy *pol = nullptr;
    bool check = false;
    size_t cap = SIZE_MAX, capped = 0, checks = 0;
    for (int i = 4; i < argc; i++) {
        std::string a = argv[i];
        if (a == "check") { check = true; continue; }
        if (a.rfind("cap=", 0) == 0) { cap = (size_t) atol(a.c_str() + 4); continue; }
        size_t eq = a.find('=');
        std::string k = a.substr(0, eq);
        double v = atof(a.substr(eq + 1).c_str());
        if (k == "sn") ds.p.sn = v;
        else if (k == "g3") ds.p.g3 = dv.p.g3 = lv.p.g3 = lv2.p.g3 = dv2.p.g3 = v;
        else if (k == "g4") ds.p.g4 = dv.p.g4 = lv.p.g4 = lv2.p.g4 = dv2.p.g4 = v;
        else if (k == "gR") ds.p.gR = dv.p.gR = lv.p.gR = lv2.p.gR = dv2.p.gR = v;
        else if (k == "al") dv.p.al = lv.p.al = lv2.p.al = dv2.p.al = v;
        else if (k == "eps") dv.p.eps = lv.p.eps = v;
        else if (k == "be") lv.p.be = lv2.p.be = v;
        else if (k == "corr") dv.p.corr = lv.p.corr = (int) v;
        else if (k == "cpick") dv.p.cpick = dv2.p.cpick = (int) v;
        else { fprintf(stderr, "unknown key %s\n", k.c_str()); return 1; }
    }
    if (which == "ds") { ds.t = new sds::Tables(ds.p); pol = &ds; }
    else if (which == "dv2") { dv2.t = new sdv2::Tables(dv2.p); pol = &dv2; }
    else if (which == "lv2") { lv2.t = new slv2::Tables(lv2.p); pol = &lv2; }
    else if (which == "lv") { lv.t = new slv::Tables(lv.p); pol = &lv; }
    else if (which == "dv") { dv.t = new sdv::Tables(dv.p); pol = &dv; }
    else { fprintf(stderr, "unknown policy\n"); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 1; }
    size_t max_p = (size_t) atol(argv[2]), np = 0, sol_hist[3] = {0, 0, 0};
    static char line[8192];
    double sum = 0, logsum = 0;
    while (np < max_p && fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        size_t n = strcspn(line, " \t\r\n,:");
        if (n != 729 && n != 81) continue;
        std::vector<char> buf(736, 0);
        memcpy(buf.data(), line, n);
        bool pencilmark = buf[81] >= '.';
        State st;
        Search s{pol, check};
        s.cap = cap;
        if (pencilmark ? Base::InitPencilmarkByBox(buf.data(), st) : Base::InitVanillaByBand(buf.data(), st))
            s.Count(st);
        np++;
        checks += s.checks;
        sum += (double) s.guesses;
        logsum += log((double) s.guesses + 1);
        sol_hist[s.sols < 2 ? s.sols : 2]++;
        capped += s.guesses >= cap;
    }
    fclose(f);
    printf("n %zu mean %.3f geo1 %.3f sols %zu/%zu/%zu capped %zu", np, sum / (double) np,
           exp(logsum / (double) np), sol_hist[0], sol_hist[1], sol_hist[2], capped);
    if (check) printf(" checked %zu", checks);
    printf("\n");
    return 0;
}
