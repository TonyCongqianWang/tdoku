// per-puzzle guess counts: original (B), exact MRV (C) and the intersection heuristic
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
extern "C" size_t v_B(const char *, size_t, char *, size_t *);
extern "C" size_t v_C(const char *, size_t, char *, size_t *);
size_t ix_base(const char *, size_t, bool, char *, size_t *);
int main(int argc, char **argv) {
    size_t limit = (size_t) atoi(argv[2]);
    FILE *f = fopen(argv[1], "r"); static char line[8192]; char sol[729];
    std::vector<double> lr_ix, lr_c; double sb = 0, si = 0, sc = 0; size_t n = 0, ix_better = 0, ix_worse = 0;
    while (fgets(line, sizeof line, f)) {
        size_t len = strcspn(line, " \t\r\n"); if (len != 729 && len != 81) continue;
        std::vector<char> b(736, 0); memcpy(b.data(), line, len);
        size_t gb, gi, gc; v_B(b.data(), limit, sol, &gb); ix_base(b.data(), limit, limit == 1, sol, &gi); v_C(b.data(), limit, sol, &gc);
        sb += gb; si += gi; sc += gc; n++;
        lr_ix.push_back(log((gi + 1.0) / (gb + 1.0))); lr_c.push_back(log((gc + 1.0) / (gb + 1.0)));
        ix_better += gi < gb; ix_worse += gi > gb;
    }
    auto gm = [](std::vector<double> v) { double s = 0; for (double x : v) s += x; return exp(s / v.size()); };
    auto med = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return exp(v[v.size() / 2]); };
    printf("%-10s limit %zu n %zu | mean guesses B %.1f C %.1f ix %.1f | ix/B: ratio of means %.3f geomean %.3f median %.3f better %zu worse %zu | C/B geomean %.3f\n",
           argv[3], limit, n, sb / n, sc / n, si / n, si / sb, gm(lr_ix), med(lr_ix), ix_better, ix_worse, gm(lr_c));
}
