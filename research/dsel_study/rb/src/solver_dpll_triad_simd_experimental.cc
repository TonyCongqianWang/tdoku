// Experimental branching heuristics, compiled as a separate translation unit so that their
// template instantiations do not change the compiler's optimization of the original solver.
// See docs/BRANCHING_RESEARCH.md.
#define TDOKU_EXPERIMENTAL_TU
#include "solver_dpll_triad_simd.cc"
