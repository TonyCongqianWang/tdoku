// Captures the selected band's configurations at every branching decision of the unmodified
// Tdoku search (limit given on the command line) and reports cardinality statistics.
#include "dsel.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
extern "C" size_t TdokuSolverDpllTriadSimd(const char *, size_t, uint32_t, char *, size_t *);
void (*g_dsel_hook)(const void *state, unsigned band) = nullptr;

static long n_dec = 0, sel_min[8] = {0}, glob_min[8] = {0}, b_differs = 0, sel_gt_glob = 0;
static std::vector<__m128i> *g_out = nullptr;

static void Hook(const void *state, unsigned band) {
    const uint16_t *base = (const uint16_t *) state;
    auto lanes = [&](int b) { return base + b * 16; };  // Band = 2 x Cells08 = 32 bytes = 16 u16
    n_dec++;
    const uint16_t *sel = lanes((int) band);
    int ex = dsel::RefExact(sel), bb = dsel::RefB(sel);
    int smin = dsel::RefCount(sel, ex);
    sel_min[smin]++;
    if (ex != bb) b_differs++;
    int gmin = 9;
    for (int b = 0; b < 6; b++) {
        const uint16_t *l = lanes(b);
        int tot = 0;
        for (int i = 0; i < 6; i++) tot += __builtin_popcount(l[i]);
        if (tot < 10) continue;
        int e = dsel::RefExact(l);
        if (e >= 0) { int n = dsel::RefCount(l, e); if (n < gmin) gmin = n; }
    }
    glob_min[gmin]++;
    if (smin > gmin) sel_gt_glob++;
    if (g_out && g_out->size() < 4000000) g_out->push_back(_mm_loadu_si128((const __m128i *) sel));
}

int main(int argc, char **argv) {
    // capture <puzzles> <limit> <name> [out.bin]
    size_t limit = (size_t) atoi(argv[2]);
    std::vector<__m128i> out;
    if (argc > 4) g_out = &out;
    g_dsel_hook = Hook;
    FILE *f = fopen(argv[1], "r");
    static char line[4096];
    char sol[729];
    size_t g, np = 0, guesses = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || strlen(line) < 81) continue;
        TdokuSolverDpllTriadSimd(line, limit, 0, sol, &g);
        np++; guesses += g;
    }
    printf("%-14s limit %-4zu puzzles %6zu guesses %9zu | selected-band min n: 2 %.3f%% 3 %.3f%% 4 %.4f%% 5 %.4f%% 6 %.4f%% | "
           "global min n: 2 %.3f%% 3 %.3f%% >=4 %.4f%% | sel>glob %.3f%% | B != exact %ld (%.4f%%)\n",
           argv[3], limit, np, guesses,
           100.0 * sel_min[2] / n_dec, 100.0 * sel_min[3] / n_dec, 100.0 * sel_min[4] / n_dec,
           100.0 * sel_min[5] / n_dec, 100.0 * sel_min[6] / n_dec,
           100.0 * glob_min[2] / n_dec, 100.0 * glob_min[3] / n_dec,
           100.0 * (glob_min[4] + glob_min[5] + glob_min[6]) / n_dec,
           100.0 * sel_gt_glob / n_dec, b_differs, 100.0 * b_differs / n_dec);
    if (argc > 4) {
        FILE *o = fopen(argv[4], "wb");
        fwrite(out.data(), 16, out.size(), o);
        fclose(o);
    }
}
