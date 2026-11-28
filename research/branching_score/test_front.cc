// Checks the SIMD front end against the scalar features on every decision node of exact-MRV
// searches. usage: test_front <puzzles> <max_puzzles>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "features.h"
#include "simd_front.h"

namespace {
size_t checked = 0, bad = 0;

void Check(const State &s) {
    sfront::Front f;
    f.Compute(s.bands, s.boxen);
    sfront::Front2 f2;
    f2.Compute(s.bands, s.boxen);
    for (int x = 0; x < 3; x++)
        for (int p = 0; p < 3; p++)
            for (int e = 0; e < 3; e++)
                for (int d = 0; d < 9; d++) {
                    // h_x: S_row_e of box (x, p); v_x: S_col_e of box (p, x)
                    int hs = sfront::Front::Byte(e == 0 ? f.SA[x * 3 + p] : e == 2 ? f.SA[x * 3 + p] : f.SB[x * 3 + p], (e == 2 ? 16 : 0) + d);
                    int vs = sfront::Front::Byte(e == 0 ? f.SC[p * 3 + x] : e == 2 ? f.SC[p * 3 + x] : f.SB[p * 3 + x], (e == 0 ? 0 : 16) + d);
                    int hk = sfront::Front::Byte(f2.E[x][p][e], d), vk = sfront::Front::Byte(f2.E[x][p][e], 16 + d);
                    int pc = sfront::Front::Byte(sfront::Pop6b(f2.E[x][p][e]), d);
                    if (hs != hk || vs != vk || pc != __builtin_popcount(hk)) {
                        if (bad < 5) fprintf(stderr, "front2 x %d p %d e %d d %d: %d/%d vs %d/%d\n", x, p, e, d, hs, vs, hk, vk);
                        bad++;
                    }
                }
    for (int b = 0; b < 6; b++) {
        Lanes x = BandLanes(s, b);
        for (int d = 0; d < 9; d++) {
            unsigned all = 0;
            for (int c = 0; c < 6; c++) all |= (unsigned) (x.l[c] >> d & 1) << c;
            if ((int) all != f.Cfg(b, d) || CountDigit(x, d) != f.N(b, d)) bad++;
            for (int p = 0; p < 3; p++)
                for (int c = 0; c < 6; c++) {
                    int k, n;
                    Crossing(s, b, d, 1u << c, p, k, n);
                    if (k != f.K(b, p, kElem[c][p], d)) {
                        if (bad < 5) fprintf(stderr, "b %d p %d e %d d %d: %d vs %d\n", b, p, kElem[c][p], d, k, f.K(b, p, kElem[c][p], d));
                        bad++;
                    }
                    checked++;
                }
        }
    }
}

size_t guesses;
int sols;
void Count(State &s) {
    if (guesses > 2000) return;
    int b, d;
    if (!ChooseMRV(s, b, d)) { sols++; return; }
    Check(s);
    guesses++;
    int c = LowConfig(BandLanes(s, b), d);
    State copy = s;
    AddElims(copy, b, d, c, true);
    if (Eliminate(copy, b)) Count(copy);
    if (sols >= 2) return;
    AddElims(s, b, d, c, false);
    if (Eliminate(s, b)) Count(s);
}
}  // namespace

int main(int argc, char **argv) {
    FILE *fp = fopen(argv[1], "r");
    size_t maxp = (size_t) atol(argv[2]), np = 0;
    static char line[8192];
    while (np < maxp && fgets(line, sizeof line, fp)) {
        size_t n = strcspn(line, " \t\r\n,:");
        if (n != 729 && n != 81) continue;
        std::vector<char> buf(736, 0);
        memcpy(buf.data(), line, n);
        State st;
        bool pm = buf[81] >= '.';
        if (pm ? Base::InitPencilmarkByBox(buf.data(), st) : Base::InitVanillaByBand(buf.data(), st)) {
            guesses = 0; sols = 0;
            Count(st);
        }
        np++;
    }
    printf("checked %zu bad %zu\n", checked, bad);
    return bad != 0;
}
