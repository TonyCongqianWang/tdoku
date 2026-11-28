# A general branching score for Tdoku (gate excluded)

## Decisions

A decision is a triple (B, d, c):

- B: an unfixed band, h0..h2 or v0..v2. Its index b is 0..5, with b < 3 horizontal.
- d: a digit with n = n(B, d) = |C(B, d)| >= 2, where C(B, d) is the remaining configurations of d in B.
- c: one of those configurations, c ∈ C(B, d).

It branches into:

- `+`: C(B, d) := {c}, so d takes configuration c;
- `-`: C(B, d) := C(B, d) \ {c}.

For n = 2, (B, d, c1) and (B, d, c2) give the same two subtrees, so one candidate is enough.
For n >= 3, the two branches differ, so the choice of c matters.

## Score

The selected decision is the argmin of

    s(B, d, c) =  f_R(R(B), m(B))                    band restriction
                + f_n(n(B, d))                        band-digit restriction
                + f_X+(X+(B, d, c)) + f_X-(X-(B, d, c))   crossing-band neighbours
                + f_M+(M+(B, d, c)) + f_M-(M-(B, d, c))   same-band neighbours (mini-rows)

Ties go to the lowest b, then d, then c.

The f's may be non-linear: lookup tables, thresholds, or infinite penalties, which act as filters
or produce a lexicographic order. Cross terms are allowed where needed, as f(x, y) of two inputs.
Only the ordering matters, so any monotone transform of s gives the same policy.

### Band restriction

- R(B) = sum over d of n(B, d), the band's total configuration count. This is the original's band key (9..54).
- m(B) = the smallest n(B, d) with n >= 2.

### Crossing neighbours (same digit, linked through the shared boxes)

B meets the three crossing bands Y_j (j = 0..2) in boxes. For a set S of configurations of d in B:

- rows_j(S) are the mini-rows (or mini-columns) that S allows d in box j.
- k_j(S) = |{ y ∈ C(Y_j, d) : the cell (rows_j(S), position of y in that box) still has d as a candidate }|.
- n_j = n(Y_j, d).

The `+` branch uses S = {c}; the `-` branch uses S = C(B, d) \ {c}.
The per-box survival fraction is ρ_j = k_j / n_j. X± aggregates (ρ_0, ρ_1, ρ_2), for example as:

- the sum;
- the product, which estimates the fraction of joint crossing configurations that survive;
- the minimum;
- the number of boxes with k_j = 1, i.e. forced.

### Same-band neighbours (other digits, mini-row capacity)

Every mini-row (box j, row r) of a band holds exactly 3 digits. This is the rule that BandEliminate's "count == 3" step propagates.

- poss_j(r) = the number of digits e ≠ d with some configuration in C(B, e) that uses (j, r).
- M+: d occupies (j, r_j(c)) in each box j, leaving 2 slots, so slack_j+ = poss_j(r_j(c)) - 2.
  When the slack is 0, every remaining candidate digit is forced into that mini-row.
- M-: if no configuration left in C(B, d) \ {c} uses (j, r_j(c)), then d vacates that mini-row and leaves 3 slots, so slack_j- = poss_j(r_j(c)) - 3. Otherwise the term is absent.

## Existing heuristics as special cases

| heuristic | f_R | f_n | X± | M± |
|---|---|---|---|---|
| original / exact MRV | R, lexicographically first | n | 0 | 0 |
| flatlex | 0 | +∞ unless n = 2 | 0 | 0 |
| final (ungated) | 0 | +∞ unless n = 2 | sum of ρ over both configurations (= X+ of c1 + X+ of c2) | 0 |
| score in original band | R, lexicographically first | +∞ unless n = 2 | as final | 0 |

## Oracle labels

cost(B, d, c) = 1 + G(+) + G(-).

- G is the number of guesses that the rollout policy (exact MRV) needs to search the subtree completely, with limit 2, i.e. the full tree for a unique puzzle.
- A refuted branch has G = 0.
- Costs above a budget (4 × the MRV choice's cost + 64) are censored.

Nodes are sampled from the exact-MRV search trees: at most K per puzzle, evenly spaced in visiting order.

Offline metrics, per node:

- top-1 rate;
- regret log(cost(chosen) / cost(best)).

Nodes are weighted equally within a puzzle and puzzles equally within a dataset.
The final measure is the end-to-end guess count of the policy (geomean per dataset). It is reported against complexity, as a Pareto front.

## Complexity ladder

0. The original (R, n) lexicographic order. No parameters.
1. Final, ungated: the n = 2 filter plus the crossing sum.
2. Plus a band weight w_R·R, which interpolates between "in original band" and "flat". 1 parameter.
3. Plus f_n as a small table, admitting n = 3 candidates.
4. Plus the same-band slack terms M±.
5. Separate non-linear `+` and `-` terms, with per-feature lookup tables.
6. Cross terms, e.g. f(n, m(B)) and f(R, has_two(B)).
