# Intersection-aware branching for Tdoku: a research report

Branch: `claude/tdoku-intersection-branching-9c3181`. Baseline revision: tag `baseline-original`
(= upstream `af42618`). Hardware: Intel Core i5-1245U (Alder Lake: Golden Cove P-cores,
Gracemont E-cores), GCC 13.3 and Clang 18.1, `-O3 -march=native`, Ubuntu 24.04.

## 1. Summary and honest assessment

**Question.** Does scoring band–digit–configuration tuples with information from the three
intersecting bands give better branching decisions than Tdoku's band-then-digit hierarchy,
and does any benefit survive the cost of computing it?

**Answer: yes for the search tree, and yes for runtime on hard puzzles. On easy and
non-proper inputs it is roughly neutral or a net loss.**

* **Search tree.** The best intersection-aware rule (*two-sided residual fractions over the
  globally minimal n=2 pairs*, §4.4) cuts decisions by about **46%** on held-out hard puzzles.
  Decisions per puzzle drop to 0.54× on forum_hardest, 0.60× on its 11+ subset, 0.54× on the
  1106 list, 0.69× on magictour, 0.62× on 17-clue, 0.80× on unbiased, and 0.76× on a freshly
  generated corpus. The rule is cheap: it reads only current band masks and box candidates, with
  no speculative propagation. Even so, it nearly matches a full one-step lookahead that does real
  propagation (0.53× vs 0.50× on hard dev puzzles).
* **Runtime.** A branch-free AVX2/GFNI implementation costs about 160 TSC cycles per selection,
  against about 30 for the original. The finally selected variant (*FINAL*, heuristic id 5)
  gates the scoring: it scores the first 4 decisions of a solve and every decision after the
  first refuted branch.
  * On the held-out set (P-core, median of 5 independent processes) it needs **0.67×** the time
    on hard puzzles, 0.73× on the 11+ subset, 0.71× on the 1106 list, 0.86× on magictour,
    0.95× on 17-clue, and 0.97× on unbiased and on the fresh corpus.
  * The Gracemont E-core and Clang 18 builds give the same picture within about 2%.
  * Against the *pristine upstream binary* in the standard `run_benchmark` harness,
    forum_hardest_1905 takes **0.673×** the time (35.5 → 23.9 µs/puzzle).
* **Regressions, not hidden.**
  * On multi-solution puzzles (serg): **+14–17%** time. The tree does not shrink, and the extra
    selection cost on its first decisions remains.
  * On mostly-invalid generator output (gen): +2–5%.
  * On kaggle (no search at all): ±1%.
  * The geometric mean over the 10 held-out categories is 0.89 on the P-core and 0.90 on the
    E-core.
* **The original sub-hypotheses fared differently** (§8):
  * Flat band–digit selection is much *worse* than the hierarchy (+49% decisions).
  * Soft hierarchical scoring never beats the hierarchy.
  * Choosing the configuration within Tdoku's chosen pair is irrelevant for proving uniqueness:
    99% of decisions are n=2 splits, where the choice only reorders the two children.
  * The geometric mean is *not* better than the arithmetic mean.
  * Rewarding one severely restricted band (small or negative p, or min) is consistently *worse*.
  * The gain comes from using intersection information to *choose among the many n=2 pairs*.
    Two-sided scoring roughly doubles the gain of one-sided scoring.

**Revision 2** (§12), prompted by review: the refutation count now restarts at every solution.
This turns revision 1's +7–19% enumeration overhead into a 2–6% gain (limit 1000, second
held-out sample, both cores), with unchanged results on proper puzzles. Limit-2 searches on
underconstrained puzzles still cost +11–15%. Related scoring rules (§12.4) did not beat AM2.

**Revision 3** (§14–15): a looseness gate closes the scoring for a node's subtree when the mean
candidate score shows that splits barely constrain the intersecting bands. On a third, never-used
held-out sample, the overhead on multi-solution puzzles (serg) drops from +15% to +4%. The
geometric mean over 18 workloads improves from 0.979 to 0.973 (P-core) and from 0.981 to 0.972
(E-core), with hard puzzles unchanged (0.70×).

What remains uncertain:
* Results come from one machine (two microarchitectures) and two compilers.
* The corpora are historical Tdoku benchmark sources plus one generated corpus. The generated
  corpus comes from Tdoku's own generator and is easy.
* The gate was chosen on the development data and confirmed on validation. Alternatives
  differed by under 1% in geometric mean, so another gate could win elsewhere.

## 2. What the implementation actually does (verified against the source)

1. **Bands.** `State::bands[2][3]`: `bands[0][i]` is horizontal band *i* (box row *i*) and
   `bands[1][j]` is vertical band *j* (box column *j*). In horizontal band *i*, box (*i*,*j*) is
   *peer j*. In vertical band *j*, box (*i*,*j*) is *peer i*.
2. **Configurations.** `Band::configurations` is a `Bitvec08x16`. Lane *c* (0..5) holds the
   9-bit set of digits for which configuration *c* is still possible, and lanes 6–7 are zero.
   Configuration *c* places the digit in element `E[c][p]` of peer *p*. The element is the row
   within the box for horizontal bands and the column for vertical bands.

   | c | 0 | 1 | 2 | 3 | 4 | 5 |
   |---|---|---|---|---|---|---|
   | E[c][0..2] | 0,1,2 | 1,2,0 | 2,0,1 | 2,1,0 | 0,2,1 | 1,0,2 |

   This table matches both `Tables::peer_x_elem_to_config_mask` and
   `Tables::shuffle_configs_to_triads`, and `test/ix_tests.cc` checks it.
3. **`ChooseBandAndValueToBranch`.**
   * It takes the band with the smallest `Popcount` of all configuration bits, provided the
     popcount is ≥ 10 (unfixed). Ties go to the lowest index, with bands ordered h0,h1,h2,v0,v1,v2.
   * Within that band, it uses rotating bit-sliced counters (≥2, ≥3, ≥4) to pick the lowest
     digit with exactly 2 configurations. Failing that, the lowest digit with exactly 3. Failing
     that, the lowest digit with ≥4 (not necessarily the minimum).
   * Band ties are resolved *before* digit classes are considered. I first got this wrong in the
     research re-implementation, and 103 of 9000 puzzles differed until the key was corrected.
4. **`BranchOnBandAndValue`.** `ClearLowBit` keeps the lowest configuration lane. Its positive
   branch asserts that configuration on a copy of the state. The negative branch eliminates it
   in place. The positive branch is explored first, and the search stops once `limit` solutions
   are found.
5. **Boxes.** `Box::cells` is a 4×4 matrix of 9-bit sets. Lane `r*4+c` holds the cell at row
   *r*, column *c* of the box. Lanes 3/7/11 hold negative horizontal triads, and lanes 12–14 hold
   negative vertical triads. Bands restrict boxes through positive triads, and boxes send triad
   eliminations back to bands.
6. **Counting.** `num_guesses_` counts branching decisions. For a uniquely solvable puzzle
   solved with limit 2, decisions equal refuted leaves, and visited nodes equal decisions + 1.
7. **Existing facilities.** `run_tests` covers 43 puzzles with expected counts. `run_benchmark`
   is a throughput harness that randomly permutes puzzles, uses limit 2 by default, and
   validates during warmup. The `data.zip` corpora and `BENCH.sh` complete the set.

## 3. The compatibility primitive

### 3.1 Definition

Let *B* be a band of orientation *o* and index *x*, let *d* be a digit, and let π be a remaining
configuration of (*B*, *d*). For the opposite band *V_y* (*y* = 0..2), the shared box is peer *y*
of *B* and peer *x* of *V_y*.

* π places *d* at element `e = E[π][y]` of the shared box: row *e* if *B* is horizontal, column
  *e* otherwise.
* A configuration σ of (*V_y*, *d*) places *d* at element `E[σ][x]` (the other coordinate).
* σ is **locally compatible** with π if the cell where both would put *d* still has *d* as a
  candidate.

**k_y(π)** is the number of such σ among the current configurations of (*V_y*, *d*).

### 3.2 3×3 compatibility matrix and tables

`K[y][e] = popcount( C(V_y,d) & T[x][A(y,e,d)] )`, where:

* `A(y,e,d)` is the 3-bit set of opposite-coordinate positions in element *e* of the shared box
  that contain *d*.
* `T[p][A]` is the 6-bit set of configurations whose element in peer *p* lies in *A*. It is the
  OR of the two configurations using each element.

Then `k_y(π) = K[y][E[π][y]]`. The general-purpose table (24 bytes):

```
T[p][A]  (A = bits c2 c1 c0; result bits = configs 5..0)
p=0: 000->000000 001->010001 010->100010 011->110011 100->001100 101->011101 110->101110 111->111111
p=1: 000->000000 001->100100 010->001001 011->101101 100->010010 101->110110 110->011011 111->111111
p=2: 000->000000 001->001010 010->010100 011->011110 100->100001 101->101011 110->110101 111->111111
configs using element e of peer p:  p0: {0,4} {1,5} {2,3}   p1: {2,5} {0,3} {1,4}   p2: {1,3} {2,4} {0,5}
```

Implementation: `src/branch_features.h`. It provides `CompatMasks` (table-driven),
`CompatCountReference` (explicit enumeration of σ and direct cell tests), and
`NegativeCompatMask` (negative-branch support).

### 3.3 Validation

* **Table-driven vs reference.** 12.4M checks: random states at six densities, all 6 bands,
  digits, configurations, and opposite bands. Also structured single-cell edge cases, empty and
  full masks, and junk in the negative-triad lanes, which must be ignored. Negative support masks
  were checked against an explicit ρ×σ enumeration.
* **Geometry.** The configuration table was checked against the solver's own elimination table,
  and every configuration must be one of the 6 distinct permutations.
* **Semantic soundness against the real solver.** In a stats build every decision is traced.
  After propagating the positive branch, the opposite band's count never exceeds `k_y(π)`:
  **0 violations in 297k checks**. This is expected. Asserting π confines *d* to one row of the
  shared box. Any σ whose column lacks *d* there leaves that column without *d*. Tdoku then
  asserts the negative triad and eliminates σ.
* **Informativeness** (hard puzzles, the original's own decisions):
  * k_y < n_y for some y in 82% of decisions.
  * k_y = 0 never occurs, because propagation already removes such configurations.
  * Spearman correlation between estimated and actual opposite-band eliminations is 0.95, but
    only 0.41 against total eliminations.
  * AUC for predicting failure of the positive branch is 0.63 for 1 − ∏q, against 0.54 for
    min k.

### 3.4 Algebra that makes the two-sided score cheap

For a pair with configuration set *P*, Σ_{π∈P} k_y(π) is a bilinear form over the shared box
(*i*,*j*):

```
B(i,j,d) = Σ_r Σ_c  hm_r · [cell (r,c) has d] · vm_c
hm_r = #configs of (H_i,d) using row r of peer j      vm_c = #configs of (V_j,d) using column c of peer i
```

**B is symmetric in the horizontal and vertical roles.** One value per (box, digit) serves both
the horizontal pair (*H_i*, *d*) and the vertical pair (*V_j*, *d*).

For an n=2 pair, the negative branch is the positive branch of the other configuration. The
two-sided score with residual fractions q = k/n_y is therefore exactly:

```
S(b,d) = Σ_y  W[n(opp_y, d)] · B(box(b,y), d),     W[n] = 60/n   (exact integers, lcm(1..6)=60)
```

and we choose the minimal S. Ties go to the lowest band, then the lowest digit, then the lowest
configuration, the same order as the research selector.

### 3.5 Complexity

* **Research selector (scalar, all rules).** O(54 pairs × 3 bands × 3 elements) table lookups
  per node, plus double arithmetic: 1–3k TSC cycles.
* **Id 2 (scalar, lazy B).** About 48 B evaluations per selection on hard puzzles (19 candidate
  pairs): about 1100 TSC cycles, dominated by branch mispredictions.
* **Id 3 (AVX2).** Computes all 81 B values branch-free in a digit-per-byte layout:
  * transposes (pshufb + gf2p8affineqb, i.e. 8×8 bit-matrix transposes);
  * pshufb table lookups for T;
  * nibble popcounts;
  * 16-bit weighted sums;
  * a min-reduction argmin.

  Cost went 288 → 231 (constant tables) → 176 (GFNI) → 162 TSC cycles per selection (TSC is
  2.5 GHz, so about 280 core cycles). The original costs about 30. There is a portable fallback
  without GFNI (bit expansion) and without AVX2 (id 2). All three implementations produce
  identical decisions in AVX2-only and SSE4.2 builds.

## 4. Implemented variants

All variants change *only* which (band, digit, configuration) is branched on. Propagation and
binary branch semantics are unchanged. Selection is runtime-configurable through bits 8..15 of
the existing `configuration` argument of `TdokuSolverDpllTriadSimd`. Values below 256 behave
exactly as before.

| id | name | description |
|----|------|-------------|
| 0 | original | unchanged baseline (default) |
| 1 | research | scalar, runtime-parameterized (`TdokuSetResearchParams`): pair rule × tuple score × scope |
| 2 | ix two-sided (scalar) | flat n=2 class, two-sided summed residual fractions |
| 3 | ix two-sided (AVX2/GFNI) | identical decisions to 2 |
| 4 | gated (tunable) | id 3 while the gate is open (`TdokuSetIxGate`), else the original choice |
| 5 | **FINAL** | rev. 3 (§15): rev. 2, plus a node whose mean candidate score is ≥ 340 closes the gate for its subtree, plus no scoring at depth ≥ 8. Rev. 2 (§12): first 4 decisions, or after ≥1 refuted branch since the last solution; own TU, shared initialization. Rev. 1 = rev. 2 without the reset |

Research-selector families (`research/scripts/variants.py`; ix scores are maximized, pair keys
minimized):

* **A** `A_orig`: the original. `A_exact` uses the exact lexicographic key (band total, n).
* **B** `B_flat`: argmin n over all pairs, index tie-break. `B_flatlex` breaks ties on n by band
  total.
* **C** soft hierarchy: S = M_p^w(n, N_B/9) for w ∈ {.1,.25,.5,.75,.9} and p ∈ {−1,0,1}.
* **D1/D2/D3** tuple scores:
  * D1: −Σk.
  * D2: −M_p(q) for p ∈ {1, 0, −1, −4, −∞}.
  * D3: G = (1 − ∏q)/n.

  Each is applied with three scopes:
  * `Dcfg`: the configuration within the original's chosen pair only.
  * `Dtie_orig`: all pairs in the original's best class.
  * `Dtie_flat`: all pairs with globally minimal n.

  `Djoint_G` scores every tuple with G.
* **E** two-sided:
  * Count based: product/sum of estimated eliminations (`E_*_prod`/`E_*_sum`, the look-ahead
    "product rule").
  * Fraction based: `E_*_am2` uses Σ AM(q⁺)+AM(q⁻), and `E_*_ammax` uses max.
* **Look** (analysis ceiling, not a candidate): actual propagation of both branches for every
  candidate tuple, product of eliminations.

Other API additions: `TdokuEnumerateWithHeuristic` and the `tdoku_ix` entry in `run_benchmark`.
The source stays in one file, compiled as three translation units (§6.4):

* `solver_dpll_triad_simd.cc`: the original, unchanged code path.
* `solver_dpll_triad_simd_final.cc`: id 5 alone.
* `solver_dpll_triad_simd_experimental.cc`: ids 1–4.

## 5. Methodology

### 5.1 Data and splits

`research/scripts/make_splits.py` builds the splits. Each puzzle in every corpus gets a
**symmetry-invariant hash** (`research/tools/iso_hash.cc`):

* Clues form a graph with relation labels (same row/col/box/band/stack/digit).
* Colors are refined with 3 rounds of 1-WL.
* The hash is the minimum over the puzzle and its transpose.

Hash checks:
* It is invariant on 2000 randomly transformed puzzles.
* It separates all 49,158 non-isomorphic 17-clue puzzles.
* It collides 35 times in 2.1M forum_hardest puzzles, where some of these may be true isomorphs.

The split is a function of the hash (mod 10: 0–3 dev, 4–6 val, 7–9 test), so isomorphic copies
across files (1905 / 11+ / 1106 / magictour) always share a split. Cross-split leakage is
asserted absent. Categories are disjoint (hard excludes 11+ and 1106 members). Samples are drawn
with seeds derived from 20261123. The MANIFEST.json written next to the splits records SHA-256
of every source and split file. The splits themselves are regenerated rather than committed.

| category | source | dev | val | test |
|---|---|---|---|---|
| kaggle | puzzles0 | 2000 | 2000 | 10000 |
| unbiased | puzzles1 | 10000 | 10000 | 20000 |
| 17clue | puzzles2 | 5000 | 5000 | 10000 |
| magictour | puzzles3 | 571 | 447 | 445 |
| hard | puzzles4 (1905, excl. 11+/1106) | 10000 | 10000 | 20000 |
| hard11 | puzzles5 | 3000 | 3000 | 5000 |
| hard1106 | puzzles6 | 157 | 101 | 117 |
| serg (2+ solutions) | puzzles7 | 2000 | 2000 | 2909 |
| gen (mostly 0 solutions) | puzzles8 | 10000 | 10000 | 20000 |
| fresh (test only) | `generate -p0 -d81 -e0 -g0 -a1 -m1` | – | – | 17972 |

The *fresh* corpus consists of independently generated random minimal puzzles. Its raw file has
SHA-256 `19c19f3f…faaf9c7` and is kept compressed in `research/generated/`. It was selected
without reference to any heuristic's difficulty. Caveat: the historical corpora were used to
develop Tdoku itself, so they are not independent of the baseline.

Every puzzle is additionally transformed at load time by a seeded random symmetry (digits,
bands, rows within bands, stacks, columns, transposition). The seeds are 1 for dev, 2 for val,
and 3 for test, and seed 4 was used for the extra count checks.

### 5.2 Measurement

* `research_bench` runs in two modes:
  * **count mode** (deterministic): decisions, solution counts, and a cross-variant
    solution-count equality check. The `_stats` build adds nodes, refuted branches, rdtsc
    selection cycles, n histogram, and per-decision traces.
  * **time mode**: the variant order is shuffled each repetition, a warmup of ≥1 s per variant
    precedes measurement, and every puzzle is timed with rdtsc. Reported values are the median
    of 5–7 repetitions, MAD, and the **paired per-repetition ratio** to the baseline.
* Timing runs are pinned with `taskset` to CPU 2 (P-core, Golden Cove) or CPU 7 (E-core,
  Gracemont). Release builds have no instrumentation.
* Stats builds are separate binaries. Selection cycles there include rdtsc overhead, about 25
  cycles, which is included in the original's ~30.
* Hardware performance counters were unavailable: `perf_event_paranoid=4`, and changing that
  system security setting was out of scope. Selection cost is therefore attributed with rdtsc
  instrumentation in the stats build, not with perf.
* Selection rule fixed before validation: best geometric-mean time ratio across all dev
  categories. A difference under 0.005 counts as a tie, broken by the smaller worst-case
  regression. The test set was used once, after freezing id 5.

## 6. Results: development and validation

### 6.1 Ablation: decisions relative to the original (limit 2, validation set)

| variant | 17clue | gen | hard | hard11 | hard1106 | magictour | serg | unbiased |
|---|---|---|---|---|---|---|---|---|
| A_exact | 0.995 | 1.000 | 1.000 | 1.000 | 1.002 | 1.001 | 1.000 | 1.000 |
| B_flat | 1.303 | 1.012 | **1.488** | 1.480 | 1.399 | 1.318 | 0.980 | 1.278 |
| B_flatlex | 0.962 | 1.006 | 1.020 | 1.020 | 1.071 | 1.000 | 1.001 | 0.999 |
| C w=.1 p=0 | 0.995 | 0.999 | 1.001 | 1.001 | 1.003 | 1.001 | 1.000 | 1.000 |
| C w=.5 p=0 | 0.965 | 1.002 | 1.018 | 1.019 | 1.058 | 1.000 | 1.000 | 0.999 |
| Dcfg (AM) | 0.999 | 0.888 | 0.999 | 0.999 | 0.999 | 1.000 | 0.924 | 1.000 |
| Dtie_orig (AM) | 0.960 | 0.792 | 0.886 | 0.894 | 0.858 | 0.899 | 0.882 | 0.977 |
| Dtie_orig (min) | 0.966 | 0.828 | 0.942 | 0.939 | 0.958 | 0.913 | 0.886 | 0.983 |
| Dtie_flat D1 Σk | 1.030 | 0.806 | **2.993** | 2.732 | 3.902 | 1.412 | 0.866 | 1.264 |
| Dtie_flat AM (p=1) | 0.805 | 0.724 | 0.690 | 0.787 | 0.703 | 0.789 | 0.837 | 0.993 |
| Dtie_flat GM (p=0) | 0.817 | 0.727 | 0.699 | 0.784 | 0.719 | 0.761 | 0.838 | 0.996 |
| Dtie_flat HM (p=−1) | 0.842 | 0.735 | 0.729 | 0.801 | 0.754 | 0.780 | 0.842 | 1.010 |
| Dtie_flat min (p=−∞) | 0.998 | 0.761 | 0.938 | 1.008 | 1.020 | 0.892 | 0.868 | 1.067 |
| Dtie_flat D3 G | 0.817 | 0.727 | 0.699 | 0.784 | 0.719 | 0.761 | 0.838 | 0.996 |
| Djoint G | 0.823 | 0.697 | 0.696 | 0.781 | 0.715 | 0.763 | 0.810 | 0.996 |
| E_orig prod (counts) | 0.985 | 0.988 | 0.901 | 0.918 | 0.885 | 0.897 | 1.004 | 0.974 |
| E_flat prod (counts) | 0.831 | 0.972 | 0.722 | 0.848 | 0.817 | 0.746 | 0.997 | 0.911 |
| E_joint sum (counts) | 6.960 | 0.541 | 1.527 | 1.972 | 2.438 | 2.502 | 0.716 | 1.733 |
| E_orig AM2 | 0.937 | 0.987 | 0.859 | 0.866 | 0.829 | 0.865 | 1.001 | 0.953 |
| **E_flat AM2 (= ids 2, 3)** | 0.595 | 0.952 | **0.523** | 0.598 | 0.553 | 0.591 | 0.986 | 0.800 |
| E_flat AMmax | 0.594 | 0.974 | 0.611 | 0.674 | 0.563 | 0.598 | 0.983 | 0.799 |
| **FINAL (id 5)** | 0.600 | 0.966 | 0.529 | 0.603 | 0.559 | 0.589 | 0.997 | 0.800 |

* Dev results are practically identical: `research/results/tables/dev_count_l2_pivot.txt`.
* Per-row 95% CIs are about ±0.003–0.01 on hard, wider on the small sets (see the
  `*_detail.txt` tables).
* kaggle needs no decisions.
* First-solution mode (limit 1, dev) ranks the variants the same way: E_flat AM2 is 0.57× on
  hard (`dev_count_l1_key_pivot.txt`).

**Lookahead ceiling** (dev, 1500 puzzles/category). Real propagation of both branches for every
candidate tuple gives 0.497× on hard, against 0.531× for the cheap E_flat AM2. On 17-clue and
unbiased the cheap estimate is *better* than this product-of-eliminations lookahead (0.66 vs 0.74
and 0.79 vs 1.00).

### 6.2 Implementation cost (dev hard, P-core)

| implementation | selection TSC cycles/node | time ratio vs original (hard) |
|---|---|---|
| research selector (doubles) | ~2500 | 1.43 |
| id 2 scalar | ~1100 | 1.01 |
| id 3 AVX2 first version | 288 | 0.71 |
| id 3 + constant tables | 231 | – |
| id 3 + GFNI transposes | 176 | 0.675 |
| id 3 + compare-based row counts | 162 | (validation/test below) |

### 6.3 Gating and selection (time ratios vs in-build original, geometric mean over 9 categories)

| variant | dev geomean | dev worst | val geomean | val worst |
|---|---|---|---|---|
| IX3 (ungated) | 0.899 | 1.191 (serg) | 0.886 | 1.185 (serg) |
| gate: after 1st fail, first 1 decision | 0.899 | 1.027 | 0.891 | 1.049 |
| gate: after 1st fail, first 2 decisions | 0.890 | 1.056 | 0.885 | 1.082 |
| **gate: after 1st fail, first 4 decisions (FINAL)** | **0.889** | 1.127 | **0.877** | 1.134 |

Gates considered and rejected on dev:
* *After the first refutation only* is 0.77–0.81× on hard (depending on the selector build). It loses the important root decisions.
* *Total configurations ≥ T*: T=100 is 0.73× on hard but 1.15× on serg.

FINAL won on validation by 0.008. Note that these dev/val timings used the single-TU build,
whose baseline was 1.5–4% slower than pristine (§6.4). The *relative* ranking of the gated
variants is unaffected, but the absolute ratios above are slightly flattering.

### 6.4 A measurement pitfall found and fixed

Running the original heuristic's code path, my modified build was 1.5–4% slower than the
pristine upstream binary in `run_benchmark`, even though its decisions were identical. The
cause: the extra template instantiations in the same translation unit made GCC stop inlining
`CountSolutions…` into `BranchOnBandAndValue`. This was confirmed from the assembly, and
removing the instantiations restores the inlining.

After moving the experiments into their own TU, the build's original path is within ±1% of
pristine on all five harness corpora (`research/results/harness_run_benchmark_v2.txt`).

The same effect also hit the experimental side. With ids 1–5 instantiated together, FINAL was
8% slower than the original on kaggle on the P-core with GCC. Kaggle puzzles need *zero*
decisions, so that time was pure initialization and propagation code compiled differently.

Giving FINAL its own TU closed this gap: kaggle went to 0.99×. IX3, left in the crowded TU,
stays at 1.09× and serves as a control. This is a real hazard for heuristic studies: code-layout
effects of a few percent are as large as the easy-set effects being measured. The earlier
test-set timings from both superseded builds are kept in `research/results/test/v1_superseded`
and `v2_superseded`. All test-set timings below use the final three-TU build.

## 7. Results: held-out test set (evaluated once, after freezing FINAL)

### 7.1 Search tree (decisions per puzzle; limit 2, transform seed 3)

| category | n | original | FINAL | ratio (95% CI) | puzzles better / worse |
|---|---|---|---|---|---|
| hard | 20000 | 54.27 | 29.14 | 0.537 ± 0.004 | 89.9% / 8.6% |
| hard11 | 5000 | 64.38 | 38.57 | 0.599 ± 0.008 | 87.2% / 11.4% |
| hard1106 | 117 | 115.4 | 61.9 | 0.536 | – |
| magictour | 445 | 8.99 | 6.17 | 0.686 | – |
| 17clue | 10000 | 0.564 | 0.351 | 0.623 ± 0.043 | 6.9% / 1.8% |
| fresh | 17972 | 0.727 | 0.555 | 0.763 ± 0.015 | 11.2% / 3.6% |
| unbiased | 20000 | 1.141 | 0.907 | 0.795 | 17.6% / 6.4% |
| serg | 2909 | 7.065 | 7.035 | 0.996 | 36.9% / 36.1% |
| gen | 20000 | 0.286 | 0.281 | 0.982 | – |
| kaggle | 10000 | 0 | 0 | – | – |

* Limit 1 (first solution): hard 0.576, hard11 0.625, hard1106 0.523, magictour 0.669, 17clue
  0.709, unbiased 0.831, fresh 0.812, serg 1.011.
* Solution counts: identical across variants for every test puzzle at limits 1, 2, and 1000.
  Every limit-1 solution was validated against the clues and the Sudoku constraints.

**By difficulty** (hard, quintiles of the *mean of both* decision counts, to avoid
regression-to-the-mean bias). The ratio falls steadily with difficulty: 0.72, 0.64, 0.58, 0.52,
0.44 (hard11: 0.74 → 0.50). The heuristic's advantage compounds with tree depth. Binning by the
baseline alone exaggerates this (0.86 → 0.38).

### 7.2 Runtime

**Primary held-out result.** Paired time ratio FINAL/original, limit 2, transform seed 3.
Each cell is the median over independent processes, with [min, max] across processes. Each
process uses interleaved repetitions in shuffled order.

| category | P-core GCC (5 processes) | E-core GCC (3 processes) |
|---|---|---|
| hard (35.4 µs/puzzle) | **0.667** [0.665, 0.668] | **0.681** [0.681, 0.682] |
| hard11 | 0.734 [0.730, 0.737] | 0.752 [0.750, 0.752] |
| hard1106 | 0.712 [0.702, 0.719] | 0.729 [0.720, 0.732] |
| magictour | 0.859 [0.822, 0.911] | 0.877 [0.856, 0.877] |
| 17clue | 0.954 [0.941, 1.001] | 0.966 [0.964, 0.967] |
| fresh | 0.969 [0.963, 0.982] | 0.979 [0.975, 0.980] |
| unbiased | 0.970 [0.969, 0.971] | 0.981 [0.980, 0.983] |
| kaggle (0 decisions; control) | 1.007 [0.980, 1.010] | 1.000 [0.998, 1.004] |
| gen | 1.034 [0.764, 1.049] | 1.023 [1.018, 1.027] |
| serg | **1.144** [1.123, 1.165] | **1.165** [1.160, 1.167] |
| geometric mean | **0.892** | **0.904** |

For the ungated IX3 (P-core, same runs) the geomean is 0.927. It sits in the crowded TU, so it
also carries that TU's code-generation penalty (kaggle 1.07×). On serg it reaches 1.18×.

Single-process runs (`research/results/tables/test_time_*.txt`):

| configuration | hard | 17clue | unbiased | serg | gen | geomean |
|---|---|---|---|---|---|---|
| P-core, Clang 18 | 0.682 | 0.971 | 0.982 | 1.189 | 1.077 | 0.911 |
| P-core, limit 1 (first solution) | 0.702 | 0.965 | 0.973 | 1.230 | 1.018 | 0.907 |

**Process-level layout effects.** A single process can be off by up to 30% for a whole category.
For example, gen's baseline ran at 323 ns instead of 245 in one process, stable within that
process (MAD 1.8 ns). Interleaving inside one process does not average this out, which is why
the primary table uses independent processes. The [min, max] ranges for gen and magictour show
the residual.

**Standard harness vs the pristine upstream binary** (`run_benchmark -n50000 -e1`, historical
corpora, P-core; median of 3 processes, process spread < 0.5%):

| corpus | pristine tdoku µs | this build `tdoku` | `tdoku_ix` (FINAL) | decisions orig → FINAL |
|---|---|---|---|---|
| forum_hardest_1905 | 35.50 | 1.004× | **0.673×** | 55.08 → 29.62 |
| magictour_top1465 | 7.40 | 1.005× | 0.845× | 9.07 → 5.98 |
| 17_clue | 2.75 | 1.005× | 0.957× | 0.62 → 0.35 |
| unbiased | 2.51 | 1.007× | 0.981× | 1.12 → 0.89 |
| kaggle | 0.844 | 1.015× | 1.028× | 0 → 0 |
| gen_puzzles | 0.245 | 1.011× | 1.047× | 0.28 → 0.27 |
| serg_benchmark | 1.95 | 1.006× | **1.153×** | 7.13 → 6.97 |

**Where the time goes.**
* Original: selection costs about 30–40 TSC cycles per node (stats build, including rdtsc
  overhead), and a hard-puzzle node costs about 1600 TSC cycles.
* FINAL: about 160 cycles per scored node on hard puzzles.
* So roughly 8% extra per node is bought back by 46% fewer nodes.
* On serg the decisions barely change (0.996×) while 4 of about 7 decisions per puzzle are
  scored at full cost on cheap nodes.
* On kaggle, which makes no decisions, the times are equal within about 1%. That confirms the
  remaining differences come from the heuristic, not from code generation.

## 8. Interpretation

* **Does the original band hierarchy already capture most of the useful information?** It
  captures more than either alternative tested here.
  * Removing it (flat n with index ties) costs +49% decisions on hard puzzles.
  * Softening it (C) never helps.
  * A fair reading: band total N_B is a good *proxy* for how constrained a pair's neighbourhood
    is.
  * Intersection fractions are a *better-targeted* measure of the same thing. Used as the
    tie-break among all n=2 pairs, they beat the hierarchy by a wide margin. Used only inside
    the original's chosen band (Dtie_orig) they help less (0.89×), which shows that the
    cross-band comparison matters.
* **Does configuration-specific scoring add information beyond band–digit cardinality?**
  * Not as a choice of configuration. For n=2, which 99–100% of decisions are, choosing π only
    orders the children. For proving uniqueness (limit 2, unique puzzles) that cannot change the
    tree, and `Dcfg` confirms ≈1.000.
  * Ordering helps modestly when solutions exist and the search stops early: serg 0.92×,
    gen 0.89× in decisions.
  * As a *pair* score the information is substantial.
* **Sum of k vs fractions; arithmetic vs geometric mean; minimum.**
  * The absolute sum (D1) is disastrous (3× decisions). It prefers opposite bands that are
    already nearly fixed, where little information is left.
  * Normalizing by n_y is essential.
  * Arithmetic ≳ geometric = G > harmonic > p=−4 > min.
  * Minimizing one exceptionally small intersecting constraint (min, p≪0) is the *worst*
    fraction-based choice. The evidence favours total restriction over one severe restriction.
* **Is two-sided scoring worthwhile?** Yes, decisively.
  * One-sided AM gives 0.69× on hard; two-sided AM2 gives 0.52×.
  * The count-based product rule from SAT look-ahead solvers does worse than fractions (0.72×).
  * The caution in the task statement (negative-branch marginals may not change) is moot for
    n=2 pairs, where the negative branch *is* a positive assignment.
* **Does the positive-branch estimate correlate with actual propagation?**
  * With the direct effect on the intersecting bands, strongly: Spearman 0.95.
  * With total propagation, moderately: 0.41.
  * With failure, weakly: AUC 0.63.

  It is still almost as good as true lookahead for ranking pairs.
* **Is expensive scoring worthwhile only near the root of difficult trees?**
  * Root decisions matter most: skipping the first decisions costs most of the gain.
  * But the gain also needs the scoring deep in hard trees.
  * The useful gate is therefore *"is this search refuting anything?"* (a solver-state
    property), not depth.
* **Why serg and gen regress.**
  * serg puzzles have many solutions. With limit 2 the search mostly descends successful
    branches (0.26 refutations per puzzle), so there is nothing to prune, and the 4 ungated
    root decisions are pure overhead on very cheap nodes.
  * gen puzzles are mostly refuted during initialization, and when search happens it is tiny.
* **Smaller tree vs faster code.**
  * All runtime gains come from the smaller tree.
  * The new selection is about 5× *more* expensive per node, and per-node propagation is
    slightly more expensive too, because the chosen branches propagate more.
  * Easy-set ratios were overstated by about 2% before the TU fix, a code-layout effect.

## 9. Correctness testing

* `test/ix_tests.cc`:
  * geometry, compat vs reference, negative support;
  * for every heuristic, including all research rule families and ids 2–5: solution counts
    (limit 100000 and limits 1/2/3/7), identical **solution sets** via enumeration (≤ 2000),
    validity of limit-1 solutions, invariance under transposition and random transformations,
    pencilmark inputs with random non-solution eliminations;
  * empty-grid limits (1/2/10/1000, enumerate 500, pencilmark empty);
  * contradictory puzzles.

  It passed on `test/test_puzzles`, 300 held-out serg puzzles, and all 117 held-out hardest
  puzzles, under GCC (AVX2+GFNI+BMI2), GCC `-mavx2` only, GCC `-msse4.2`, and Clang.
* `run_tests` passes for `tdoku` and `tdoku_ix`.
* Research selector with `pair=1` reproduces the original decision-for-decision (0/9000 differ).
  Ids 2 and 3 reproduce research `pair=3,ix=7` (0/18000 differ). The test-set decisions of id 5
  were re-verified after the TU refactor (0/319,329 differ).

## 10. Reproduction

```bash
./BUILD.sh                                   # or: mkdir build && cd build && cmake .. && make
unzip data.zip
g++ -O2 -march=native -o build/iso_hash research/tools/iso_hash.cc
build/generate -p 0 -d 81 -e 0 -g 0 -a 1 -l 60000 -m 1 -n 10 | cut -c1-81 > fresh.txt   # see below
python3 research/scripts/make_splits.py . research/data fresh.txt
build/ix_tests && build/run_tests
research/scripts/run_counts.sh dev 2 research/results/dev/count_l2 1 A B C Dcfg Dtie_orig Dtie_flat Djoint E E2 opt
research/scripts/run_time_multi.sh test 2 5 3 research/results/test/multi_pcore_l2 "A_orig=h:0;FINAL=h:5;IX3=h:3" 2 3
python3 research/scripts/summarize_time_multi.py research/results/test/multi_pcore_l2.proc*.csv
build/run_benchmark -s tdoku,tdoku_ix data/puzzles4_forum_hardest_1905
```

**Not included in this branch:** the raw measurements and the generated corpora are omitted
to keep the change small. This covers everything under `research/results/` (summaries, timing
CSVs, harness outputs, pivot tables, compressed per-puzzle counts; about 8 MB) and the
generated puzzle corpora `research/generated/fresh*.txt.xz` (about 5 MB). They live on the
research branch the work was done on. File names cited in this report (e.g. `*_detail.txt`,
`dev_gate4_*.txt`) refer to that branch.

* The split manifests in `research/data/` record the SHA-256 of every source and sample file.
* The fresh corpora can be regenerated with `build/generate -p 0 -d 81 -e 0 -g 0 -a 1 -l 60000
  -m 1 -n 10`. The generator is not seeded, so a regenerated corpus is a different, equivalent
  sample: its hashes will not match the manifest.
* Deterministic results (decision counts, solution counts) are reproducible exactly from the
  splits. Timings are reproducible in distribution.

## 11. Attribution and novelty

* Tdoku, its triad/band representation, and the baseline heuristic are Thomas Dillon's
  (BSD-2-Clause, see LICENSE).
* The two-sided product of eliminations (`E_*_prod`) is the well-known look-ahead "product"
  heuristic from SAT solving (e.g. march, Heule et al.). Minimum-remaining-values ("fail
  first") is classic CSP practice.
* The 8×8 bit-matrix transpose via `gf2p8affineqb` with the 0x8040201008040201 constant is a
  known GFNI idiom. The 1-WL colour refinement used for the isomorphism hash is standard.
* My adaptations and findings:
  * the compatibility-count primitive on Tdoku's configuration representation and its table;
  * the symmetric per-(box, digit) bilinear form B, which makes the two-sided fraction score
    computable for all pairs at once;
  * the branch-free implementation;
  * the refutation gate;
  * the empirical results above.

No code was imported from other solvers.

## 12. Follow-up: gating for branching-poor and enumeration workloads, and related scores

Motivation (from review of §7): when a search makes few branching decisions, or when decisions
merely enumerate solutions rather than refute, time spent choosing *where* to branch is wasted.
Can the expensive score be gated efficiently, e.g. only when much branching is still ahead?

### 12.1 What predicts the search remaining below a decision?

The stats build records, for every decision, cheap state features and the number of decisions in
the subtree that decision spawns (`research_bench_stats -D`; dev, original heuristic, 3000
puzzles per category; `research/results/tables/dev_decision_features.txt`).

| category | decisions | share with subtree 1 | Spearman(log subtree, ·): remaining configs | unfixed bands | n=2 pairs | depth |
|---|---|---|---|---|---|---|
| hard | 162,978 | 0.34 | **+0.72** | +0.59 | +0.40 | −0.58 |
| magictour | 5,515 | 0.34 | +0.54 | +0.36 | +0.39 | −0.41 |
| 17clue | 1,524 | 0.57 | +0.45 | +0.23 | +0.32 | +0.04 |
| unbiased | 3,393 | 0.54 | +0.46 | +0.34 | +0.34 | −0.20 |
| serg | 14,240 | 0.16 | +0.87 | +0.84 | +0.78 | −0.72 |
| gen | 947 | 0.09 | +0.95 | +0.82 | +0.75 | −0.38 |

The total of remaining band configurations (a free by-product of the original selector's
popcounts) is the best single predictor of remaining search in every category.

It does not tell you whether that search can be *shrunk*. serg subtrees are not small (mean 4.2)
but consist of descents to solutions (0.26 refutations per puzzle), so no choice of branch shrinks
them.

At the root, the categories occupy distinct ranges of remaining configurations (p10/p50/p90):

| category | root total |
|---|---|
| hard | 150/155/161 |
| magictour | 115/131/147 |
| serg | 103/109/116 |
| unbiased | 76/96/117 |
| 17clue | 68/86/131 |

### 12.2 Gates evaluated

All gates are runtime parameters of one compiled heuristic (id 4), so every gate runs the same
machine code. **CTRL**, a gate that never opens, makes the original's decisions through that
same code and serves as an in-binary control for code-generation effects.

* **O4**: first 4 decisions, or after ≥1 refutation (= FINAL revision 1).
* **O4R**: as O4, but the refutation count restarts at every solution. Only refutations since
  the last solution open the gate.
* **T120R**: open while remaining configurations ≥ 120, or after a refutation since the last
  solution.
* **O1R**, **O0**: open for the first 1 / 0 decisions; O0 opens only after a refutation.
* Explored on dev but not carried forward:
  * T ∈ {100, 110, 130, 140}.
  * A lower bound T_lo ∈ {60, 70, 80} below which the gate closes near the leaves. It has no
    effect: decisions are rarely made with fewer than about 70 configurations left.

Three implementation lessons came out of this:

1. **The closed-gate path must be the original code.** The first gated version re-derived digit
   and configuration from the original's choice and branched through the generic
   `BranchOnConfig`. That cost about 9% on serg even with the gate almost always closed. A copy
   of the original branching code that also counts refutations removes it: serg went from 1.09×
   to about 1.0× with O0.
2. **Solved leaves use the original's cheap check first.** The large intersection selector is
   never entered just to discover that every band is fixed.
3. **Initialization must be shared.** Each translation unit compiles its own copy of the
   initialization and propagation code, and on Golden Cove with GCC the experimental copy was
   5–9% slower on zero-decision puzzles (kaggle). Revision 2 calls the original translation
   unit's compiled initialization (`TdokuInitState`), which brings the zero-decision control
   back to 0.996×.

### 12.3 Dev and validation results (time relative to the original)

Dev, limit 2, 3 processes; ratios relative to CTRL, i.e. the gating effect alone
(`dev_gate1bin_*.txt`):

| gate | geomean (9 cat.) | hard | magictour | 17clue | serg | enumeration gen / serg (limit 1000) |
|---|---|---|---|---|---|---|
| O4 (rev. 1) | 0.888 | 0.679 | 0.832 | 0.967 | 1.125 | 1.130 / 1.066 |
| O4R | 0.890 | 0.685 | 0.841 | 0.961 | 1.147 | **0.992 / 0.977** |
| T120R | **0.887** | 0.687 | 0.877 | 0.967 | **1.013** | 0.997 / 1.002 |
| O1R | 0.905 | 0.741 | 0.866 | 0.970 | 1.043 | 1.020 / 0.990 |
| O0 | 0.915 | 0.772 | 0.910 | 0.973 | 1.005 | 1.151 / 1.088 |

**Validation.** The selection criterion was declared before the run: mean log time ratio vs the
original over 19 cells. The cells are:

* the 9 standard categories at limit 2;
* derived multi-solution sets (hard and unbiased puzzles with 1 or 3 clues deleted) at limit 2;
* those 4 sets plus serg and gen at limit 1000.

Results: O4 0.970, **O4R 0.943, T120R 0.940**. Worst cells were 1.1091 for O4R and 1.1089 for
T120R.

The declared rule (ties within 0.005 go to the smaller worst case) cannot separate O4R and
T120R. Their breakdown, however, shows that T120R's edge is serg alone. On the *other*
multi-solution family (hard minus 3 clues) T120R is the worst gate: 1.109 vs 1.070 for O4R.

The threshold therefore dodges one generator's characteristic root size rather than detecting
enumeration-like search. **O4R was chosen as revision 2 on that a-priori ground (no
dataset-tuned threshold).** This judgment call was made before any test2 data was seen.

### 12.4 Related scoring rules (dev, decisions relative to the original, limit 2)

All rules are two-sided over the flat n=2 class
(`dev_score2_counts.txt` / `dev_score3_counts.txt` / `dev_score4_counts.txt`).

| score | 17clue | hard | hard11 | magictour | unbiased | note |
|---|---|---|---|---|---|---|
| **AM2** Σ(q⁺+q⁻) (used) | 0.530 | 0.531 | 0.602 | 0.646 | 0.785 | |
| GM2 Σ log q | 0.553 | 0.533 | 0.599 | 0.654 | 0.801 | |
| Σ q^p, p ∈ {0.25, 0.5, 0.75, 2} | 0.534–0.557 | 0.528–0.547 | 0.596–0.621 | 0.646–0.663 | 0.787–0.793 | flat in p |
| PROD2 (Σq⁺)(Σq⁻) | 0.558 | 0.542 | 0.616 | 0.677 | 0.805 | rewards balance |
| AM2, ties by band total (λ→0) | 0.549 | **0.519** | **0.582** | 0.666 | 0.783 | ±2–3%, opposite signs |
| AM2 + 0.05–0.2·N_B/9 | 0.547–0.550 | 0.519–0.520 | 0.581–0.582 | 0.661–0.664 | 0.782–0.783 | same as tie-break |
| INF2 (ignore fixed opposite bands) | 0.640 | 0.779 | 0.863 | 0.731 | 0.849 | much worse |
| log₂(n_y)-weighted | 2.357 | 4.316 | 3.906 | 2.019 | 1.647 | catastrophic |
| CELL2 (own-band cell residuals only) | 0.854 | 1.436 | 1.487 | 1.063 | 1.184 | worse than original |
| AM2 + μ·CELL2, μ = 0.5–2 | 0.531–0.534 | 0.531–0.537 | 0.601–0.604 | 0.632–0.638 | 0.781–0.792 | no gain |
| EXC2 Σ (k−1)/(n_y−1) | **0.511** | 0.603 | 0.672 | **0.621** | 0.791 | |
| FORCE (AM2 − λ·#fixes), λ = 0.25 / 1 | **0.503** / 0.518 | 0.592 / 0.777 | 0.657 / 0.820 | **0.619** / 0.665 | 0.789 / 0.810 | |

Readings:

* AM2 sits on a broad optimum. Changing the aggregation (geometric, powers, product) moves
  results by at most ±3%.
* An opposite band in which the digit is already fixed must still count: its q = 1 correctly
  marks the crossing as uninformative. Dropping such bands (INF2), or weighting by log n_y,
  destroys the signal.
* The band's own box cells carry no useful signal here.
* Scores that reward *fixing* an opposite band (EXC2, FORCE) help short trees (17-clue −4–5%,
  magictour −4%) but hurt deep ones (hard +11–46%). Greedy forcing pays off near the leaves,
  balanced shrinkage at depth.
* Since runtime gains are concentrated in deep trees, AM2 was kept. The best alternative (ties
  broken by band total) changes the 8-category geomean by −0.3% with opposite signs across
  categories, which did not justify a fast implementation.

### 12.5 Second held-out evaluation (test2) of revision 2

**Data.** A new disjoint sample from the *unused* remainder of the test-split pools (same
isomorphism-hash rule; no overlap with dev, val, or the first test set):

* hard 10k, hard11 3k, 17clue 4k, unbiased 10k, kaggle 5k, gen 10k;
* **fresh2**: 17,963 test-split puzzles from a newly generated corpus;
* derived multi-solution sets: 2000 each of hard and unbiased with 1 or 3 clues deleted;
* serg: the first test set's file *reused* (no unused serg puzzles remain), flagged as such.

Manifest: `research/data/test2/MANIFEST.json`. Transform seed 5.

Configuration:
* **FINAL2** is revision 2 (id 5: O4R, shared initialization, own translation unit).
* **FINAL1** reproduces revision 1's gate (O4) through the tunable id 4.
* **CTRL** is the in-binary control.
* Paired time ratios vs the original are medians over 5 processes (P-core) or 3 (E-core);
  [min, max] are in `research/results/tables/test2_time_*.txt`.

Search tree (decisions, limit 2): hard 0.546, hard11 0.599, 17clue 0.547, fresh2 0.772,
unbiased 0.794, hard_minus1 0.833, hard_minus3 0.980, unbiased_minus1/3 0.963/0.982,
serg 0.984. At limit 1000 the ratios are 0.95–1.0. Solution counts are identical to the original
for every puzzle at limits 2 and 1000.

**Time, limit 2 (uniqueness check / first two solutions):**

| category | FINAL2 P-core | FINAL2 E-core | FINAL1 P-core | CTRL P-core |
|---|---|---|---|---|
| hard | **0.698** | **0.695** | 0.691 | 1.019 |
| hard11 | 0.756 | 0.756 | 0.759 | 1.022 |
| 17clue | 0.968 | 0.958 | 0.960 | 1.003 |
| fresh2 | 1.024 | 0.990 | 0.997 | 0.998 |
| unbiased | 1.000 | 0.989 | 1.004 | 1.022 |
| kaggle (0 decisions) | 0.999 | 0.999 | 0.994 | 1.000 |
| gen | 1.049 | 1.028 | 1.042 | 1.002 |
| hard_minus1 | 0.944 | 0.939 | 0.940 | 1.018 |
| hard_minus3 | 1.112 | 1.105 | 1.094 | 0.989 |
| unbiased_minus1 | 1.117 | 1.114 | 1.109 | 1.004 |
| unbiased_minus3 | 1.124 | 1.121 | 1.111 | 0.993 |
| serg (reused) | 1.151 | 1.141 | 1.122 | 0.989 |
| geometric mean | 0.985 | 0.976 | 0.975 | 1.005 |

**Time, limit 1000 (solution enumeration):**

| category | FINAL2 P-core | FINAL2 E-core | FINAL1 P-core | FINAL1 E-core |
|---|---|---|---|---|
| gen | 0.977 | 0.977 | 1.141 | 1.187 |
| hard_minus1 | 0.936 | 0.941 | 0.977 | 1.042 |
| hard_minus3 | 0.973 | 0.985 | 1.107 | 1.184 |
| unbiased_minus1 | 0.979 | 0.984 | 1.036 | 1.099 |
| unbiased_minus3 | 0.980 | 0.980 | 1.085 | 1.182 |
| serg (reused) | 0.983 | 0.968 | 1.074 | 1.105 |
| geometric mean | **0.971** | **0.972** | 1.069 | 1.132 |

**Standard harness vs pristine.** The repeat of the §7.2 harness comparison for revision 2 was
**inconclusive**. It ran while the machine was under interactive load (load average up to 5.4):
absolute times were up to 40% above quiet runs, and per-repetition paired ratios spread from
0.68 to 1.5. The medians (forum_hardest 0.70×, serg 1.16×) agree with the driver results, but I
do not rely on them (`research/results/test2/harness_run_benchmark*.txt`).

**Conclusions of the follow-up:**

* **Enumeration.** Restarting the refutation count at every solution is an effective, cheap,
  dataset-independent gate. Revision 1 cost +7–19% on every enumeration workload; revision 2
  *gains* 2–6% on all of them on both cores. That gain comes from the refutation phases between
  solutions, which still benefit from good branching.
* **Uniqueness checks on proper puzzles.** Unchanged from revision 1, within noise: about 0.70×
  on hard puzzles, 0.96× on 17-clue, and neutral on easy sets.
* **Remaining weakness.** Limit-2 searches on underconstrained puzzles (serg, puzzles with 3
  clues deleted, unbiased minus 1 clue) still cost +11–15%. These searches are short (about 7
  decisions to two solutions), and the 4 ungated root decisions are spent on choices that
  cannot shrink a solution-rich tree.
  * None of the cheap state features tested separates these from hard proper puzzles at the
    root without a dataset-specific threshold (§12.3). Remaining configurations separate serg
    but not hard-minus-3.
  * Detecting them seems to need information the solver doesn't have cheaply before
    branching, such as an estimate of solution density.
  * A caller that knows it is checking candidates for uniqueness on underconstrained inputs
    (e.g. a puzzle generator) should simply use heuristic 0.
* **Related scores.** No related score beat AM2 in deep trees. Forcing-oriented scores win on
  shallow trees but lose more on deep ones.


## 13. Does it win by guessing better or by refuting earlier?

For a uniquely solvable puzzle, the complete search (limit 2) splits exactly into three parts:

* **P**: decisions taken in states still consistent with the solution (the solution path).
* **W1**: decisions inside refuted subtrees explored *before* the solution because a path decision
  explored the wrong branch first.
* **W2**: decisions inside refuted subtrees explored *after* the solution, i.e. proving
  uniqueness.

A first-solution search (limit 1) costs exactly P + W1. Ordering the two branches better (guessing)
can only move work between W1 and W2. Earlier refutation shrinks P and the refuted subtrees
themselves.

The stats build measures this with the known solution as an oracle (`research_bench_stats -O`).
The identities T = P + W1 + W2 and L1 = P + W1 hold on every puzzle. Data: dev set, up to 5000
unique puzzles per category (`research/results/tables/dev_oracle_decomposition.txt`).

| category | variant | T | P | W1 | W2 | refuted decisions per path node | first branch correct | off-path child refuted immediately | L1 = P + W1 |
|---|---|---|---|---|---|---|---|---|---|
| hard | original | 54.33 | 6.25 | 23.80 | 24.28 | 7.69 | 0.499 | 0.314 | 30.05 |
| hard | **FINAL** | 29.69 | 5.44 | 12.03 | 12.22 | 4.45 | 0.500 | 0.372 | 17.47 |
| hard | one-sided AM (Dtie_flat_am) | 38.15 | 6.16 | 12.93 | 19.05 | 5.19 | **0.364** | 0.378 | 19.09 |
| hard11 | original | 64.43 | 6.64 | 28.83 | 28.95 | 8.70 | 0.504 | 0.311 | 35.47 |
| hard11 | **FINAL** | 39.52 | 5.93 | 16.74 | 16.86 | 5.67 | 0.506 | 0.352 | 22.66 |
| hard1106 | original | 114.16 | 7.87 | 53.64 | 52.64 | 13.50 | 0.480 | 0.286 | 61.52 |
| hard1106 | **FINAL** | 64.62 | 6.91 | 27.88 | 29.83 | 8.35 | 0.505 | 0.349 | 34.79 |
| magictour | original | 9.66 | 3.37 | 3.15 | 3.14 | 1.87 | 0.503 | 0.549 | 6.51 |
| magictour | **FINAL** | 6.46 | 2.73 | 1.93 | 1.80 | 1.37 | 0.514 | 0.626 | 4.66 |
| 17clue | original | 0.66 | 0.35 | 0.20 | 0.12 | 0.88 | 0.491 | 0.785 | 0.55 |
| 17clue | **FINAL** | 0.35 | 0.29 | 0.03 | 0.03 | 0.20 | 0.487 | 0.897 | 0.32 |
| unbiased | original | 1.15 | 0.92 | 0.12 | 0.11 | 0.25 | 0.492 | 0.839 | 1.04 |
| unbiased | **FINAL** | 0.90 | 0.79 | 0.06 | 0.05 | 0.14 | 0.491 | 0.890 | 0.85 |

**Answer: it wins by refuting earlier, not by guessing better.**

* **Guessing.** FINAL picks the first branch correctly 49–51% of the time, the same coin flip as
  the original. The two-sided score is symmetric in the two children, and the lower-indexed
  configuration is asserted first, so FINAL has no guessing component at all.
* **Refuted subtrees.** On hard puzzles, 97% of the reduction in decisions (54.3 → 29.7) comes from
  smaller refuted subtrees (W1 + W2: 48.1 → 24.3). The average refuted subtree hanging off a
  path node shrinks from 7.7 to 4.5 decisions, and the off-path child is refuted by propagation
  alone more often (31% → 37%).
* **Path.** The solution path is also somewhat shorter (6.25 → 5.44 decisions), because the chosen
  branches propagate more.
* **First-solution searches.** These benefit in the same proportion, because W1 and W2 shrink
  equally (L1 30.1 → 17.5).
* **One-sided scores behave differently.** The one-sided rule (`Dtie_flat_am`) picks the
  configuration that most restricts the crossing bands and asserts it first. That makes it a
  "fail-first" guesser: its first branch is *wrong* 64% of the time on hard puzzles. Those wrong
  branches are cheap to refute, so W1 < W2 (12.9 vs 19.1). Most of its gain therefore comes from
  refuting its own wrong guesses quickly, not from shrinking the refutations of both children.
  The two-sided rule shrinks both sides evenly and ends up smaller overall.
* **Untapped room.** With perfect guessing a first-solution search would cost only P (6.25 vs
  5.44 decisions). Guessing could still help first-solution searches substantially, for the
  original and for FINAL alike. It cannot help uniqueness checks, which must explore W2 anyway.

## 14. Predicting solution-to-solution search: score looseness and depth

Two proposed signals:

* **Score aggregates.** If even the candidate splits barely constrain the intersecting bands, the
  rest of the solve probably walks from solution to solution.
* **Backtrack distance (depth).** Be generous near the root, where a refutation discards a large
  part of the search, and frugal deep down.

### 14.1 What can be predicted

The label is: "the subtree below this decision contains no refuted branch". Decisions were
recorded under the ungated scorer (dev; `dev_gate_signal_auc.txt`, `dev_gate_tradeoff2.txt`).

* **Structural fact.** In a uniquely solvable puzzle *every* subtree contains a refutation, since
  one child of each decision cannot hold the unique solution. Refutation-free subtrees therefore
  occur only in multi-solution inputs. There they make up 54% (hard −1 clue) to 92% (unbiased −3)
  of decisions.
* **Within multi-solution sets** (AUC for refutation-free, direction-adjusted):

  | signal | AUC |
  |---|---|
  | remaining configurations | 0.67–0.78 |
  | **mean candidate score** | 0.61–0.77 |
  | minimum candidate score | 0.60–0.75 |
  | depth | 0.63–0.70 |
  | maximum ("worst") candidate score | 0.50–0.57 (weak: some candidate is nearly always fully unconstrained, q = 1) |

* **Pooled trade-off** (the real requirement: close loose multi-solution searches *without*
  closing nodes of proper puzzles). The rule "mean score ≥ 340 (of 360)":
  * closes 0.7% / 0.8% / 10% / 10% / 1% of the refutation-weighted nodes of hard / hard11 /
    magictour / 17-clue / unbiased;
  * closes 87% / 35% / 35% / 47% / 48% / 69% of the refutation-free nodes of serg /
    hard−1 / hard−3 / unbiased−1 / unbiased−3 / gen.

  Unlike the remaining-configurations threshold (§12.3), it generalizes across both
  multi-solution families.
* **Depth.** Cutting the scoring off at depth D saves selections and loses refutation weight in
  roughly equal proportion. Deep refuted subtrees are small (about 2 decisions), but most decisions
  live there (hard, D=6: 21% of selections skipped, 11% of refutation weight lost).

### 14.2 Cheap implementation

* Scores exist only where the selector ran, so the gate **inherits** them. If a node's candidate
  scores show a loose position, its whole subtree uses the original rule (`loose_[depth]`,
  propagated downward).
* The minimum score is free: it is the selector's argmin. The mean costs a few vector
  operations and is compiled in only when used.
* Depth is a counter maintained in the experimental branch functions.

All of these are runtime knobs of id 4: `TdokuSetIxGateExtra`, gate spec
`g:fails:min_total:open:open_total:reset:loose_min:loose_mean:max_depth:open_depth`. Id 5 has
compile-time equivalents (`TDOKU_FINAL_LOOSE_MIN` / `_MEAN`), off by default.

### 14.3 Results (time vs the original; one binary, 3 processes)

| gate (on top of rev. 2 = O4R) | dev geomean (19 cells) | val geomean (19 cells) | val worst | serg limit 2 (dev / val) | hard (val) |
|---|---|---|---|---|---|
| none (rev. 2) | 0.954 | 0.9460 | 1.138 | 1.12 / 1.14 | 0.685 |
| loose if **mean ≥ 340** | **0.945** | **0.9411** | **1.112** | **1.03 / 1.05** | 0.690 |
| loose if min ≥ 320 | 0.946 | 0.9452 | 1.114 | 1.07 / 1.11 | 0.692 |
| depth cutoff 8 | 0.948 | 0.9420 | 1.126 | 1.09 / 1.13 | 0.696 |
| open at depth < 3 (instead of first 4 decisions) | 0.951 | – | – | 1.09 / – | – |

Dev values are relative to the original. Relative to the in-binary control, the dev geomeans are
0.957 / 0.948 / 0.949 / 0.950 / 0.954 (`dev_gate4_*.txt`, `val_gate4_*.txt`).

### 14.4 Conclusion

* **Score looseness works, mainly where intended.** The serg overhead drops from +14% to +4–5% on
  both dev and validation, with no loss on proper puzzles. The overall gain is about 0.5%
  geomean, and the worst case improves (1.138 → 1.112).
* **Not adopted by default.** The validation improvement (0.0049) falls just below the adoption
  threshold declared beforehand (0.005), so the default stays revision 2.
  `-DTDOKU_FINAL_LOOSE_MEAN=340` enables the gate; adopting it should be confirmed on a fresh
  sample first.
* **Depth is not a useful lever here.** A cutoff trades selections for refutation weight almost
  one-for-one, and opening by depth near the root instead of by decision count is no better.
* **What remains.** Short limit-2 searches on underconstrained puzzles still pay +7–11%, because
  their roots are not loose yet, and the first scores are paid before any signal exists. The
  minimum possible cost is one selection per solve unless the root itself can be judged loose
  from something cheaper. A candidate is the original selector's band totals, which are already
  computed; I have not tested it.


## 15. Combinations (depth-scheduled thresholds, reopening, depth cutoff) and revision 3

### 15.1 Depth-dependent looseness thresholds

The idea was to lower the looseness threshold with depth, being more willing to stop scoring deep
in the tree. The mean candidate score rises with depth in *all* puzzle kinds, because positions
loosen as they fill. On hard proper puzzles the p90 goes 311 → 318 → 324 → 331 → 338 → 345 at
depths 0/2/4/6/8/10 (`dev_score_by_depth.txt`).

* **Decreasing schedules** T(d) = max(320, T0 − s·d) still lie on about the same trade-off frontier
  as constant thresholds (`dev_depth_threshold_tradeoff.txt`). A high threshold at shallow depth
  protects the loose-looking but refutation-rich roots of 17-clue/magictour, and misfires deep
  down cost little because those subtrees are small.
* **Increasing schedules** misfire on shallow 17-clue/unbiased roots.

### 15.2 Further combinations

* **Reopen-on-refutation**: a loose subtree stays closed only until a refutation happens inside it.
* **Looseness + depth cutoff.**

### 15.3 Dev and validation (time vs the original, geomean over 19 cells, one binary)

| gate on top of rev. 2 | dev | val run 1 | val run 2 | serg limit 2 (dev / val) |
|---|---|---|---|---|
| none (rev. 2) | 0.955 | 0.946 | 0.949 | 1.12 / 1.14–1.15 |
| mean ≥ 340 | **0.947** | **0.941** | 0.949 | 1.02 / 1.03–1.05 |
| mean ≥ 340 + depth cutoff 8 | 0.948 | – | **0.940** | 1.03 / 1.04 |
| mean ≥ max(320, 350 − 2d) | 0.948 | – | 0.945 | 1.06 / 1.09 |
| mean ≥ max(320, 360 − 3d) | 0.951 | – | – | 1.09 |
| mean ≥ 340 + reopen | 0.949 | – | – | 1.04 |
| mean ≥ 330 + reopen | 0.955 | – | – | 1.02 |
| mean ≥ max(320, 350 − 4d) + reopen | 0.958 | – | – | 1.07 |

Observations:

* None of the combinations is clearly better than the constant threshold.
* Reopening helps limit-2 searches slightly but hurts enumeration: refutations between solutions
  keep re-triggering scoring that cannot pay off there.
* **Measurement caveat.** The two validation runs differ by up to 0.008 for the same variant. The
  limit-2 component is stable (mean ≥ 340: 0.9353 / 0.9364). The enumeration cells (limit 1000) are
  noisy, with process-to-process ranges up to 30% on single cells. Differences under about 1%
  between gates are therefore not resolved by this setup.
* Following the rule declared before the run (best validation geomean; a combination must beat
  constant 340 by > 0.003), the finalist was **mean ≥ 340 + depth cutoff 8**.

### 15.4 Confirmation on a third held-out sample (test3), used once

**Data.** test3 was drawn from the remaining unused test-split pools:

* hard 10k, hard11 3k, kaggle 5k, unbiased 10k, gen 10k;
* 17clue: all 681 remaining;
* fresh: 18,052 puzzles from a newly generated corpus (fresh3);
* derived multi-solution sets: hard and unbiased with 1 or 3 clues deleted, 2000 each;
* **serg_unseen**: the 3062 serg puzzles never used in any sample (from the dev/val pools,
  isomorphism-disjoint from every used puzzle; labelled as such).

Manifest: `research/data/test3/MANIFEST.json`. Transform seed 6.

**Adoption rule, declared before the run.** The finalist must have a lower geomean vs the original
than rev. 2 over all cells, and a worst cell no more than 0.01 worse, on both the P-core and the
E-core.

| test3 | rev. 2 | finalist | plain mean ≥ 340 (reported, not selected) |
|---|---|---|---|
| P-core geomean (18 cells; 5 processes) | 0.9788 | **0.9730** | 0.9735 |
| P-core worst | 1.145 (serg_unseen) | 1.108 (unbiased−3) | 1.102 |
| E-core geomean (3 processes) | 0.9805 | **0.9723** | 0.9730 |
| E-core worst | 1.148 (serg_unseen) | 1.123 (unbiased−3) | 1.117 |
| compiled id 5, P-core (3 processes) | 0.9836, worst 1.159 | **0.9740**, worst 1.128 | – |
| serg_unseen limit 2 (P / E) | 1.145 / 1.148 | **1.046 / 1.039** | 1.047 / 1.035 |
| hard limit 2 (P / E) | 0.710 / 0.706 | 0.703 / 0.698 | 0.711 / 0.694 |
| enumeration geomean (P / E) | 0.974 / 0.977 | 0.973 / 0.979 | 0.972 / 0.981 |

Decisions: hard 0.551× (rev. 2: 0.543×), 17clue 0.657×, fresh 0.775×. Solution counts are
identical to the original everywhere.

**The rule is met on both cores, and the finalist is adopted as revision 3 (default id 5).** The
depth cutoff contributes nothing measurable: plain mean ≥ 340 performs the same within noise. It
was kept because the selection rule had fixed the finalist before the test data were seen.

### 15.5 Limitations

* The looseness signal comes from the AVX2 selector. In builds without AVX2 the fallback selector
  does not report scores, so revision 3 behaves like revision 2 with a depth cutoff.
* Short limit-2 searches on heavily underconstrained puzzles still cost +8–12% (hard −3,
  unbiased −1/−3 clues). Their roots are not loose, and the first scores are paid before any
  signal exists.
* The comparisons among gate variants are limited by the enumeration-cell noise described above.
