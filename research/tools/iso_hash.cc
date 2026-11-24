// iso_hash: prints a 64-bit hash for each input puzzle that is invariant under all Sudoku
// symmetries (digit relabeling, band/stack permutation, row/column permutation within
// bands/stacks, transposition).
//
// Method: clues are vertices of a graph whose edge labels record (same row, same col, same box,
// same band, same stack, same digit). These relations are preserved by every non-transposing
// symmetry; transposition swaps row<->col and band<->stack, so the hash is the minimum over
// the puzzle and its transpose. Vertex colors are refined for 3 rounds (1-dimensional
// Weisfeiler-Leman) and the sorted multiset of final colors is hashed.
//
// The hash is an invariant, not a canonical form: isomorphic puzzles always collide;
// non-isomorphic puzzles collide only rarely (measured in research/data/README.md). Collisions
// only cause *extra* puzzles to be kept in the same split, never leakage across splits.
//
// usage: iso_hash < puzzles > hashes      (one hex hash per non-comment line, "-" if not 81 chars)

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

static inline uint64_t Mix(uint64_t x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

uint64_t Signature(const string &p, bool transpose) {
    struct Clue { int r, c, d; };
    vector<Clue> clues;
    for (int i = 0; i < 81; i++) {
        if (p[i] >= '1' && p[i] <= '9') {
            int r = i / 9, c = i % 9;
            if (transpose) swap(r, c);
            clues.push_back({r, c, p[i] - '1'});
        }
    }
    size_t n = clues.size();
    vector<uint64_t> color(n, 1), next(n);
    auto relation = [&](const Clue &a, const Clue &b) {
        int rel = 0;
        rel |= (a.r == b.r) << 0;
        rel |= (a.c == b.c) << 1;
        rel |= (a.r / 3 == b.r / 3 && a.c / 3 == b.c / 3) << 2;
        rel |= (a.r / 3 == b.r / 3) << 3;
        rel |= (a.c / 3 == b.c / 3) << 4;
        rel |= (a.d == b.d) << 5;
        return (uint64_t) rel;
    };
    for (int round = 0; round < 3; round++) {
        for (size_t i = 0; i < n; i++) {
            vector<uint64_t> nb;
            nb.reserve(n);
            for (size_t j = 0; j < n; j++) {
                if (i == j) continue;
                uint64_t rel = relation(clues[i], clues[j]);
                if (rel == 0) continue;  // unrelated pairs carry no information
                nb.push_back(Mix(rel * 0x9e3779b97f4a7c15ULL ^ color[j]));
            }
            sort(nb.begin(), nb.end());
            uint64_t h = Mix(color[i] + 0x1234567ULL);
            for (uint64_t x : nb) h = Mix(h ^ x) + 0x9e3779b97f4a7c15ULL;
            next[i] = h;
        }
        color.swap(next);
    }
    sort(color.begin(), color.end());
    uint64_t h = Mix(n);
    for (uint64_t x : color) h = Mix(h ^ x) + 0x7f4a7c159e3779b9ULL;
    return h;
}

int main() {
    ios::sync_with_stdio(false);
    string line;
    while (getline(cin, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.size() < 81) { cout << "-\n"; continue; }
        string p = line.substr(0, 81);
        uint64_t h = min(Signature(p, false), Signature(p, true));
        char buf[24];
        snprintf(buf, sizeof(buf), "%016llx", (unsigned long long) h);
        cout << buf << "\n";
    }
    return 0;
}
