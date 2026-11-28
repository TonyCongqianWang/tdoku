// one digit-selection variant of the original solver, in its own translation unit
#define TDOKU_IX_TU
#include "solver_dpll_triad_simd.cc"
namespace {
SolverDpllTriadSimd<0> v_none;
SolverDpllTriadSimd<1> v_last;
}
extern "C" size_t VARIANT_ENTRY(const char *p, size_t limit, char *sol, size_t *g) {
    return limit == 1 ? v_last.SolveSudoku(p, limit, sol, g) : v_none.SolveSudoku(p, limit, sol, g);
}
