#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
size_t TdokuSolverIntersection(const char *, size_t, bool, char *, size_t *);
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "r"); static char line[8192]; size_t np = 0, max = atol(argv[2]); double s = 0, ls = 0; char sol[729];
    while (np < max && fgets(line, sizeof line, f)) { size_t n = strcspn(line, " \t\r\n,:"); if (n != 81 && n != 729) continue;
        std::vector<char> b(736, 0); memcpy(b.data(), line, n); size_t g; TdokuSolverIntersection(b.data(), 2, false, sol, &g); np++; s += g; ls += log(g + 1.0); }
    printf("n %zu mean %.3f geo1 %.3f\n", np, s / np, exp(ls / np));
}
