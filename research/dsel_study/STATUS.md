# Digit-selection counting study: status (2026-11-26)

Durable copy of the scratch work (the original lived in /tmp and is lost on reboot).
`dsel/` = kernels, tests, benchmarks, data, results. `rb/` = research-branch archive with
`research_bench` (patched to accept 729-char pencilmark lines).

## Done
- Kernels A, B, C, D1, D2, E1, E2, F, DB in `dsel/dsel.h`; `dsel/t` (test_dsel.cc) passes:
  exhaustive 64-pattern counts, pair patterns, 2M random vectors (gcc native / avx2 / sse4.2, clang).
- Static analysis: `dsel/mca_*`, `dsel/k_*.s`, `dsel/asm/` (llvm-mca alderlake / icelake-server).
- Microbenchmarks: `dsel/mb_pcore.txt` (cpu 2, Golden Cove), `dsel/mb_ecore.txt` (cpu 9, Gracemont).
- Branching-domain statistics: `dsel/stats_l2.txt`; captured states in `dsel/states/`.
- Pencilmark guess counts per component of the ix heuristic: `dsel/rb_pm_hard.txt`, `dsel/rb_pm_easy.txt`.

## Not done
- End-to-end timing never ran: the wait loop `pgrep -f "generate ..."` matched its own shell
  and spun for 2 h. Fixed in `dsel/resume_e2e.sh` (no wait loop).
- Report (answers to the 6 research questions, ix improvements, pencilmark regression).

## Resume
```
cd research/dsel_study/dsel
./build.sh          # rebuild objects + drivers, runs the correctness test
uptime              # only time when the machine is idle
./resume_e2e.sh     # ~40 min; results in e2e_p{1,2,3}.txt (P-core) and e2e_e1.txt (E-core)
```

## Pencilmark diagnosis (limit 2, guesses/puzzle, geomean(g+1))
| variant | pm hard (825) | pm high-clue (662) |
|---|---|---|
| orig (B) | 15838 / 2882 | 5012 / 2060 |
| exactMRV | 15129 / 2787 | 4976 / 2060 |
| flatlex (global n=2, no score) | 38583 / 6865 | 13588 / 6472 |
| score_in_orig_band | 13810 / 2401 | 4025 / 1730 |
| flat_score_ungated | 27950 / 5656 | 11281 / 5498 |
| final (tdoku_ix) | 15768 / 3248 | 6119 / 2827 |

The pencilmark regression comes from leaving the band-first choice (taking an n=2 candidate in
another band when the selected band has none), not from the score: scoring restricted to the
original band is 17% / 16% better than orig by geomean. Still to check: the vanilla datasets
for score_in_orig_band versus final.
