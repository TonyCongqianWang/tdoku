// End-to-end paired benchmark of digit-selection variants. Every variant is its own translation
// unit built the same way (baseline included), so code-generation effects are comparable.
// usage: bench_e2e <group: tdoku|ix> <puzzles> <limit> <max_puzzles> <rounds> <name>
// Each round times one full pass over the puzzles for every variant, in rotated order; the
// reported ratio is the median over rounds of t(variant) / t(baseline) within the round.
#include <x86intrin.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
size_t v_B(const char *, size_t, char *, size_t *);
size_t v_A(const char *, size_t, char *, size_t *);
size_t v_C(const char *, size_t, char *, size_t *);
size_t v_D1(const char *, size_t, char *, size_t *);
size_t v_DB(const char *, size_t, char *, size_t *);
}
size_t ix_base(const char *, size_t, bool, char *, size_t *);
size_t ix_lazyB(const char *, size_t, bool, char *, size_t *);
size_t ix_lazyDB(const char *, size_t, bool, char *, size_t *);
size_t ix_eager(const char *, size_t, bool, char *, size_t *);
static size_t w_ix_base(const char *p, size_t l, char *s, size_t *g) { return ix_base(p, l, l == 1, s, g); }
static size_t w_ix_lazyB(const char *p, size_t l, char *s, size_t *g) { return ix_lazyB(p, l, l == 1, s, g); }
static size_t w_ix_eager(const char *p, size_t l, char *s, size_t *g) { return ix_eager(p, l, l == 1, s, g); }
static size_t w_ix_lazyDB(const char *p, size_t l, char *s, size_t *g) { return ix_lazyDB(p, l, l == 1, s, g); }

typedef size_t Fn(const char *, size_t, char *, size_t *);
struct V { const char *name; Fn *fn; int policy; };  // same policy => identical search trees

int main(int argc, char **argv) {
    std::string group = argv[1];
    size_t limit = (size_t) atoi(argv[3]), max_p = (size_t) atol(argv[4]);
    int rounds = atoi(argv[5]);
    std::vector<V> vs;
    if (group == "tdoku")
        vs = {{"B", v_B, 0}, {"A", v_A, 1}, {"C", v_C, 1}, {"D1", v_D1, 1}, {"DB", v_DB, 0}};
    else
        vs = {{"ix", w_ix_base, 0}, {"ix_eager", w_ix_eager, 0}, {"ix_lazyB", w_ix_lazyB, 0}, {"ix_lazyDB", w_ix_lazyDB, 0}};

    std::vector<std::string> puzzles;
    FILE *f = fopen(argv[2], "r");
    static char line[8192];
    while (fgets(line, sizeof line, f) && puzzles.size() < max_p) {
        if (line[0] == '#') continue;
        size_t n = strcspn(line, " \t\r\n");
        if (n == 729 || n == 81) puzzles.emplace_back(line, n);
    }
    fclose(f);
    // zero-padded buffers (the solver reads input[81] to detect pencilmark input)
    std::vector<std::vector<char>> buf;
    for (auto &p : puzzles) { std::vector<char> b(736, 0); memcpy(b.data(), p.data(), p.size()); buf.push_back(b); }

    // correctness: solution counts and guesses per puzzle
    size_t nv = vs.size(), np = buf.size();
    std::vector<std::vector<size_t>> sols(nv, std::vector<size_t>(np)), gs(nv, std::vector<size_t>(np));
    char sol[729];
    for (size_t v = 0; v < nv; v++)
        for (size_t i = 0; i < np; i++) sols[v][i] = vs[v].fn(buf[i].data(), limit, sol, &gs[v][i]);
    bool ok = true;
    for (size_t v = 1; v < nv; v++)
        for (size_t i = 0; i < np; i++) {
            if (sols[v][i] != sols[0][i]) { ok = false; fprintf(stderr, "SOLUTION COUNT MISMATCH %s #%zu\n", vs[v].name, i); break; }
            for (size_t u = 0; u < v; u++)
                if (vs[u].policy == vs[v].policy && gs[u][i] != gs[v][i]) {
                    ok = false; fprintf(stderr, "TREE MISMATCH %s vs %s #%zu\n", vs[u].name, vs[v].name, i); break;
                }
        }

    if (rounds < 2) { for (size_t v = 0; v < nv; v++) { size_t g = 0; for (size_t i = 0; i < np; i++) g += gs[v][i]; printf("%s %s limit %zu n %zu %s guesses/puzzle %.2f\n", argv[6], vs[v].name, limit, np, ok ? "OK" : "MISMATCH", (double) g / (double) np); } return ok ? 0 : 1; }
    std::vector<std::vector<double>> ratio(nv);
    std::vector<double> base_t;
    uint64_t sink = 0;
    for (int r = 0; r < rounds; r++) {
        std::vector<double> t(nv);
        for (size_t k = 0; k < nv; k++) {
            size_t v = (k + (size_t) r) % nv;
            uint64_t t0 = __rdtsc();
            for (size_t i = 0; i < np; i++) { size_t g; sink += vs[v].fn(buf[i].data(), limit, sol, &g); }
            t[v] = (double) (__rdtsc() - t0);
        }
        if (r == 0) continue;  // warm-up round
        base_t.push_back(t[0]);
        for (size_t v = 0; v < nv; v++) ratio[v].push_back(t[v] / t[0]);
    }
    auto median = [](std::vector<double> x) { std::sort(x.begin(), x.end()); return x[x.size() / 2]; };
    printf("%-12s limit %-4zu n %6zu %s | base %.0f TSC/puzzle", argv[6], limit, np, ok ? "OK" : "MISMATCH",
           median(base_t) / (double) np);
    for (size_t v = 0; v < nv; v++) {
        size_t g = 0;
        for (size_t i = 0; i < np; i++) g += gs[v][i];
        printf(" | %s %.4f g=%.3f", vs[v].name, median(ratio[v]), (double) g / (double) np);
    }
    printf("%s\n", sink == 42 ? " " : "");
    return ok ? 0 : 1;
}
