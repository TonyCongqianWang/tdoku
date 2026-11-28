#!/bin/bash
# Rebuild all variant objects and the benchmark drivers (run from anywhere).
set -e
cd "$(dirname "$0")"
mkdir -p obj
F="-O3 -march=native -std=c++14 -Isolver/include -Isolver/src -include cstdint -include cstring"
b() { g++ $F "$@"; }
b -c variant_wrap.cc -DVARIANT_ENTRY=v_B  -DTDOKU_DSEL=0 -o obj/v_B.o &
b -c variant_wrap.cc -DVARIANT_ENTRY=v_A  -DTDOKU_DSEL=1 -DTDOKU_DSEL_FN=dsel::SelA  -o obj/v_A.o &
b -c variant_wrap.cc -DVARIANT_ENTRY=v_C  -DTDOKU_DSEL=2 -DTDOKU_DSEL_FN=dsel::SelC  -o obj/v_C.o &
b -c variant_wrap.cc -DVARIANT_ENTRY=v_D1 -DTDOKU_DSEL=3 -DTDOKU_DSEL_FN=dsel::SelD1 -o obj/v_D1.o &
b -c variant_wrap.cc -DVARIANT_ENTRY=v_DB -DTDOKU_DSEL=4 -DTDOKU_DSEL_FN=dsel::SelDB -o obj/v_DB.o &
for v in base:0 lazyB:1 lazyDB:2 eager:3; do
  n=${v%%:*}; l=${v##*:}
  b -c solver/src/solver_dpll_triad_simd_ix.cc -DTdokuSolverIntersection=ix_$n -DIX_LAZY=$l -o obj/ix_$n.o &
done
wait
b bench_e2e.cc   obj/*.o solver/src/util.cc -o bench_e2e
b bench_count.cc obj/*.o solver/src/util.cc -o bench_count
b perpuzzle.cc obj/v_B.o obj/v_C.o obj/ix_base.o solver/src/util.cc -o perpuzzle
g++ -O3 -march=native -std=c++17 mbench.cc -o mbench
g++ -O2 -march=native -std=c++17 test_dsel.cc -o t && ./t | tail -1
