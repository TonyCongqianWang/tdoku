// Shared helpers for the branching-score research tools: band/box access, exact MRV, and the
// raw features of the general score (see SPEC.md).
#pragma once
#include "solver_dpll_triad_simd_ix.cc"

namespace {

using Base = SolverDpllTriadSimd<0>;

// element (mini-row / mini-column) of configuration c in band peer p (see the table in the solver)
const int kElem[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}, {0, 2, 1}, {1, 0, 2}};

struct Lanes {
    uint16_t l[8];
};

Lanes BandLanes(const State &s, int b) {
    Lanes x;
    _mm_storeu_si128((__m128i *) x.l, s.bands[b / 3][b % 3].configurations.vec);
    return x;
}

// box index of peer p of band b, and the cell candidate set at (row, col) of that box
int BoxOf(int b, int p) { return b < 3 ? (b % 3) * 3 + p : p * 3 + (b % 3); }

uint16_t Cell(const State &s, int box, int row, int col) {
    alignas(32) uint16_t c[16];
    _mm256_store_si256((__m256i *) c, s.boxen[box].cells.vec);
    return c[row * 4 + col];
}

int CountDigit(const Lanes &x, int d) {
    int n = 0;
    for (int c = 0; c < 6; c++) n += x.l[c] >> d & 1;
    return n;
}

int Popcount(const Lanes &x) {
    int n = 0;
    for (int c = 0; c < 6; c++) n += __builtin_popcount(x.l[c]);
    return n;
}

bool Eliminate(State &s, int b) {
    return b < 3 ? Base::BandEliminate<0>(s, b) : Base::BandEliminate<1>(s, b - 3);
}

// add eliminations for digit d in band b: keep only config c (plus) or remove config c (minus)
void AddElims(State &s, int b, int d, int c, bool plus) {
    alignas(16) uint16_t e[8] = {0};
    for (int k = 0; k < 6; k++)
        if ((k == c) != plus) e[k] = (uint16_t) (1u << d);
    Band &band = s.bands[b / 3][b % 3];
    band.eliminations = band.eliminations | Cells08{_mm_load_si128((const __m128i *) e)};
}

// exact MRV: band with the fewest configurations (lowest index on ties), then the lowest digit
// with the fewest configurations >= 2. false if every band is fixed.
bool ChooseMRV(const State &s, int &b, int &d) {
    int best = 1000;
    b = -1;
    for (int k = 0; k < 6; k++) {
        int p = s.bands[k / 3][k % 3].configurations.Popcount();
        if (p >= 10 && p < best) { best = p; b = k; }
    }
    if (b < 0) return false;
    Lanes x = BandLanes(s, b);
    int bn = 100;
    for (int k = 0; k < 9; k++) {
        int n = CountDigit(x, k);
        if (n >= 2 && n < bn) { bn = n; d = k; }
    }
    return true;
}

int LowConfig(const Lanes &x, int d) {
    for (int c = 0; c < 6; c++) if (x.l[c] >> d & 1) return c;
    return -1;
}

// crossing compatibility k_j(S) and n_j for peer j of band b, digit d, configuration set S
void Crossing(const State &s, int b, int d, unsigned S, int j, int &k, int &n) {
    int y = b < 3 ? 3 + j : j;  // crossing band
    Lanes yl = BandLanes(s, y);
    int box = BoxOf(b, j);
    int p_in_y = b % 3;  // this box is peer (b % 3) of the crossing band
    k = n = 0;
    for (int cy = 0; cy < 6; cy++) {
        if (!(yl.l[cy] >> d & 1)) continue;
        n++;
        int ey = kElem[cy][p_in_y];
        bool ok = false;
        for (int c = 0; c < 6; c++) {
            if (!(S >> c & 1)) continue;
            int eb = kElem[c][j];
            int row = b < 3 ? eb : ey, col = b < 3 ? ey : eb;
            if (Cell(s, box, row, col) >> d & 1) ok = true;
        }
        k += ok;
    }
}

// a decoded copy of the state: band configuration lanes and box cell candidate sets
struct Snap {
    Lanes band[6];
    uint16_t cell[9][16];
    explicit Snap(const State &s) {
        for (int b = 0; b < 6; b++) band[b] = BandLanes(s, b);
        for (int x = 0; x < 9; x++) _mm256_storeu_si256((__m256i *) cell[x], s.boxen[x].cells.vec);
    }
};

// Crossing() on a decoded state
void Crossing(const Snap &s, int b, int d, unsigned S, int j, int &k, int &n) {
    const Lanes &yl = s.band[b < 3 ? 3 + j : j];
    const uint16_t *cells = s.cell[BoxOf(b, j)];
    int p_in_y = b % 3;
    k = n = 0;
    for (int cy = 0; cy < 6; cy++) {
        if (!(yl.l[cy] >> d & 1)) continue;
        n++;
        int ey = kElem[cy][p_in_y];
        bool ok = false;
        for (int c = 0; c < 6; c++) {
            if (!(S >> c & 1)) continue;
            int eb = kElem[c][j];
            int row = b < 3 ? eb : ey, col = b < 3 ? ey : eb;
            if (cells[row * 4 + col] >> d & 1) ok = true;
        }
        k += ok;
    }
}

// number of digits e != d that can still use mini-row (peer j, element r) of band b
int Possible(const Lanes &x, int d, int j, int r) {
    int cnt = 0;
    for (int e = 0; e < 9; e++) {
        if (e == d) continue;
        bool any = false;
        for (int c = 0; c < 6; c++)
            if ((x.l[c] >> e & 1) && kElem[c][j] == r) any = true;
        cnt += any;
    }
    return cnt;
}

}  // namespace
