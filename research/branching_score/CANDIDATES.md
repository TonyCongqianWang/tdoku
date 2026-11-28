# Three branching-score candidates: guesses vs. static cycle cost

There is no end-to-end timing yet. Guess counts are deterministic, and costs are llvm-mca estimates of one cold call of the chooser.

## Update: optimized LV2 / DV2 and end-to-end timing

**Optimized versions** (`simd_lv2.h`, `simd_dv2.h`). Both were already 8-bit integer log arithmetic with `pshufb` tables. On top of that:
- the 6 bands × 9 digits are packed into 2 vectors instead of 3;
- the − branch union selects with `or` + `min` instead of `vpblendvb` (3 uops);
- the own-band terms are folded into the first peer's tables;
- the "volume 0" guard is dropped in LV (identical guesses);
- the band term is computed with SAD;
- the keys are 8-bit (`avg(L, 255 − B)`, no eps clamp).

Both are again verified exactly against scalar mirrors, and refitting confirmed that their guess counts match LV/DV.

| chooser | instructions | cycles (mca, ADL-P) | val guesses vs exact MRV |
|---|---|---|---|
| DS | 771 | 241 | .708 |
| DV2 | 807 | 274 (DV: 335) | .650 |
| LV2 | 1271 | 394 (LV: 523 → 506) | .592 |

**End-to-end timing** (`time_e2e.cc`, `run_time.sh`): the validation split, inside the real Tdoku search, policies timed round robin, median of 7 passes. Values are time relative to the original solver, geomean over the 8 sets (full tables in `time_pcore_table.txt`, `time_ecore_table.txt`).

| | orig | ix gated (PR) | DS | DV2 | LV2 |
|---|---|---|---|---|---|
| P-core, limit 2 | 1 | .935 | .731 | .691 | **.647** |
| P-core, limit 1 | 1 | .945 | .739 | .692 | **.649** |
| E-core, limit 2 | 1 | .938 | .739 | .697 | **.662** |
| E-core, limit 1 | 1 | .946 | .751 | .707 | **.669** |

- **Hard standard sets:** LV2 is 0.67–0.75× the original on a P-core, vs 0.72–0.81× for the PR's gated ix. On the E-core, DV2 ties LV2 there.
- **Pencilmark:** LV2 is 0.28–0.36× the original, vs 1.1–1.36× for gated ix.
- **Easy sets** (17clue, unbiased): everything is within ±3% of the original; LV2 is up to 3.3% slower on unbiased on the E-core.
- **Cost per guess node:** a guess node costs on the order of 400–600 ns in the original solver, which is far above the chooser costs. That explains why the most expensive chooser wins.

The sections below describe the first (unpacked) versions.

## Candidates

The notation follows SPEC.md: band B, digit d, configuration c, n = |C(B,d)|, crossing survival ρ_j = k_j / n_j, R = configurations of B. Each score is minimised over all unfixed bands and all digits with n ≥ 2.

| | score | union of the − branch | parameters (fit3_*_wide) |
|---|---|---|---|
| **DS** (digit sum) | Σ_j (1/n_j) Σ_{c∈C} k_j(c) / n^sn + g(n) + gR·R | no | sn .5, g3 −.25, g4 −.5, gR .025 |
| **DV** (digit volume) | ln(ε + Σ_{c∈C} Π_j ρ_j(c)^α) + g(n) + gR·R | no | α 1.1, ε 1e-4, g3 −.15, g4 .1, gR .015, c = largest + volume |
| **LV** (branch volumes) | ln(ε + n^−β P₊ + ((n−1)/n)^β P₋) + g(n) + gR·R, P = Π_j ρ_j^α | yes (exact) | α 1.1, β .6, ε 1e-3, g3 −.25, g4 −.1, gR .015 |

- **DS** is ChooseIx extended to digits with n ≥ 3. The sum Σ_{c∈C} k_j(c) counts compatible configuration pairs, and that count is symmetric, so only the row transposes are needed (as in ix).
- **DV** is the union-free volume. For n = 2 it equals LV with β = 0.
- **LV** matches double-precision L5 on guess counts with 6 parameters and no linear terms.

## Guesses (geomean(guesses+1) relative to exact MRV, limit 2, no gate)

`test` is the odd dev puzzles. `val` is unseen puzzles: research/data/val plus the unused pencilmark puzzles (see sets/val).

| policy | hard | hard11 | hard1106 | magictour | 17clue | unbiased | pm | pm_easy | **test** | **val** |
|---|---|---|---|---|---|---|---|---|---|---|
| exact MRV | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| ix gated (the PR) | .598 | .676 | .628 | .777 | .967 | .952 | 1.218 | 1.463 | .874 | .869 |
| **DS** | .600 | .650 | .622 | .792 | .975 | .974 | .549 | .628 | .674 | .708 |
| **DV** | .576 | .633 | .579 | .744 | .966 | .954 | .434 | .493 | .624 | .648 |
| **LV** | .538 | .590 | .553 | .736 | .962 | .947 | .328 | .389 | .561 | .592 |
| L5 (double, reference) | .537 | .588 | .560 | .751 | .966 | .942 | .337 | .402 | .564 | .598 |

The per-set columns are on the val split. The guess counts come from the fixed-point SIMD implementations themselves (policy_simd). Refitting on them recovered the small quantization loss; see fit3_*_wide.txt.

## Static cost (llvm-mca 18, one cold call, region markers around the chooser)

| chooser | instructions | Alder Lake P | Ice Lake server | Zen 4 |
|---|---|---|---|---|
| MRV (Tdoku original) | 101 | 74 | 67 | 55 |
| ix (ChooseIx, the PR) | 661 | 232 | 231 | 214 |
| **DS** | 771 | 241 | 238 | 238 |
| **DV** | 1061 | 335 | 332 | 352 |
| **LV** | 1749 | 523 | 490 | 497 |

- The chooser is throughput-bound. Its critical path is about 100 cycles, so the vector uops spread over p0/p1/p5 set the cost, and instruction count is the metric to optimise.
- In the real gated solver, ix costs less on average than the figure above, because the gate falls back to the cheap original choice.

## Break-even propagation cost per guess node (cycles, val mean guesses)

A candidate is faster than the baseline when the solver's own work per guess node exceeds this value: T* = (G_x·t_x − G_b·t_b) / (G_b − G_x).

| vs | | hard | hard11 | hard1106 | magictour | 17clue | unbiased | pm | pm_easy |
|---|---|---|---|---|---|---|---|---|---|
| MRV | DS | 141 | 202 | 185 | 260 | 353 | 1474 | 34 | 81 |
| | DV | 231 | 316 | 254 | 321 | 378 | 1270 | 49 | 73 |
| | LV | 372 | 501 | 437 | 552 | 716 | 1901 | 51 | 94 |
| ix gated | DS | 625 | always | always | never | never | never | always | always |
| | DV | 1603 | 875 | 527 | 1219 | 986 | never | always | always |
| | LV | 1808 | 1518 | 1313 | 2494 | 3486 | 13704 | always | always |

The per-node propagation cost of Tdoku decides which candidate wins. Measuring it is the first end-to-end measurement to make.

## Implementation notes

All three are AVX2 + GFNI, branch-free apart from the final argmin. Each is checked for exact equality against a scalar mirror of its fixed-point key on every decision (`policy_simd ... check`).

- **Layout.** "Digit per byte" as in ChooseIx. DV and LV use a band-pair layout `[h_x | v_x]`, so every per-(band, peer) quantity fills a full vector (simd_front.h, Front2).
- **Column sets.** One `vpermd` gathers the three rows of a column into a lane, then one GF 8×8 transposition. That's 3 ops instead of about 10 for the shift/affine construction.
- **Popcount of ≤ 6-bit bytes.** Nibble lookup plus a GF bit permutation for bits 4–5: 5 ops.
- **Log arithmetic.** 8-bit, 1/24 bit per unit. Products become saturating sums of Q(k) = 24α·log2(6/k), with k = 0 mapped to 255 ("volume 0"). Sums over configurations use LSE(a, b) = min − corr(|a − b|) with a 16-entry table indexed by a GF bit permutation of the difference (8 ops). The crossing normalisation Σ_j log n_j is the same for all configurations of a digit and is subtracted once per band pair.
- **Exact union (LV).** Per band pair and peer, M_e = S_e if C uses element e; T = ∪M_e, X_e = ∪_{e'≠e} M_e'. Then k₋(c) = |T| if c's partner at that peer (the other configuration with the same element) is in C, else |X_e(c)|. So each − count is one `vpblendvb` between two precomputed table values, and the membership masks double as partner tests.

## Cross terms

- **Inside the score, the crossing product matters most.** It is the cross term over peers j: L3 → L4 went from .688 to .570 end to end, although the oracle predicted only .892 → .875.
- **On top of L5, extra cross terms add ≤ 1.6%** (test, fit2_screen.txt): ln Σ_c P₊(c) (wlv) .555, own-band fraction (wown) .559, digit volume (wv) .559. R × product, (n−2) × product, P₊ × P₋, max(P₊, P₋) and per-branch product weights add nothing.
- **These findings motivated LV:** pure products plus the own-band fraction (the wown/β term).
- **Approximating the − branch union for n ≥ 3 is not viable.** The union over all of C, the sum of the other configurations' volumes, and their maximum cost 6–116% more guesses (policy2 `lvm`).
- **Band restriction is not viable either.** Scoring only the k least-constrained bands costs too much (hard: 39.8 with k = 1, 26.6 with k = 3, vs 24.9 with all six).

## Files

- `simd_front.h`: shared front end (Front: per-box layout; Front2: band-pair layout).
- `simd_ds.h`, `simd_dv.h`, `simd_lv.h`: the choosers and their scalar mirrors (KeyRef).
- `policy_simd.cc`: end-to-end guess counts of the SIMD choosers with the `check` mode. `test_front.cc` tests the front ends.
- `policy2.cc`: double-precision evaluator with per-decision precomputation (13× faster than policy_eval) and all score families.
- `fit2.py`, `fit3.py`, `baselines.py`: fits and reference tables (`fit2_*.txt`, `fit3_*_wide.txt`, `baselines.txt`).
- `mca.sh`, `mca_choosers.cc`, `mca/summary_*.txt`: static analysis.

Build with `-O3 -march=native -std=c++17 -ffp-contract=off -I../../src -I../../include -include cstdint -include cstring ... ../../src/util.cc`.
