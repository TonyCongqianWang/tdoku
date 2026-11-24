// Intersection-aware branching heuristic for the Tdoku solver (opt-in: configuration 1 << 8).
//
// The search reuses the original solver's representation and propagation unchanged and only
// changes which (band, digit, configuration) it branches on. It is compiled in its own
// translation unit so that the original solver's code is not affected.
//
// Selection: among the band-digit pairs with exactly two remaining configurations {p1, p2} in
// bands that are not yet fixed, choose the pair that minimizes
//     S = sum over the 3 crossing bands y of  W[n_y] * (k_y(p1) + k_y(p2)),   W[n] = 60 / n,
// where n_y is the number of configurations of the digit in crossing band y and k_y(p) is the
// number of those that remain locally compatible with p, i.e. that place the digit in the shared
// box at a cell where p allows it. S / 60 is the summed fraction of the crossing bands'
// configurations that survive in both branches: a small S means both branches strongly constrain
// the crossing bands. Ties go to the lowest band, digit and configuration index.
// k_y(p1) + k_y(p2) is the number of compatible (horizontal, vertical) configuration pairs in the
// shared box, so one value per (box, digit) serves both orientations; all 54 scores are computed
// at once with AVX2 (and GFNI when available).
//
// Gate: the score is only computed where it tends to pay off. It is used for the first 4
// decisions and after a refuted branch since the last solution, but not below a node whose
// candidates' mean score is >= 340 of 360 (a loose position, typical of searches that walk from
// solution to solution) and not at depth >= 8. Otherwise the original heuristic decides.
// Without AVX2 the original heuristic is used throughout.

#define TDOKU_IX_TU
#include "solver_dpll_triad_simd.cc"


namespace {

template<int solution_mode>
struct SolverIntersection {
    using Base = SolverDpllTriadSimd<solution_mode>;
    static constexpr uint32_t NONE = UINT32_MAX;
    static constexpr int kOpenDecisions = 4;
    static constexpr int kLooseMeanScore = 340;
    static constexpr int kMaxDepth = 8;

    State solution_{};
    size_t limit_ = 1;
    size_t num_solutions_ = 0;
    size_t num_guesses_ = 0;
    size_t num_fails_ = 0;  // refuted branches since the last solution
    int depth_ = 0;         // decisions on the current path
    uint8_t loose_[128]{};  // loose_[k]: the node at depth k is in a loose subtree

    struct DepthGuard {
        int &d;
        explicit DepthGuard(int &x) : d(x) { ++d; }
        ~DepthGuard() { --d; }
    };

    struct Choice {
        uint32_t band;  // 0..5 (3 * vertical + idx), or NONE if no candidate pair exists
        int digit;
        int config;
        uint16_t score_mean;
    };

    static const Cells08 &ConfigLaneMask(int config) {
        static const Cells08 masks[6]{
                {0xffff, 0, 0, 0, 0, 0, 0, 0}, {0, 0xffff, 0, 0, 0, 0, 0, 0},
                {0, 0, 0xffff, 0, 0, 0, 0, 0}, {0, 0, 0, 0xffff, 0, 0, 0, 0},
                {0, 0, 0, 0, 0xffff, 0, 0, 0}, {0, 0, 0, 0, 0, 0xffff, 0, 0}};
        return masks[config];
    }

    void CountSolutions(State &state) {
        auto band_and_value = Base::ChooseBandAndValueToBranch(state);
        if (band_and_value.first == NONE) {
            num_solutions_++;
            num_fails_ = 0;
            if (solution_mode == 1 && num_solutions_ == limit_) solution_ = state;
            return;
        }
        int k = depth_ < 127 ? depth_ : 127;
        bool loose = k > 0 && loose_[k - 1];
        loose_[k] = loose;
        bool use_ix = !loose && depth_ < kMaxDepth &&
                      (num_guesses_ < kOpenDecisions || num_fails_ >= 1);
#ifdef __AVX2__
        if (use_ix) {
            Choice choice = ChooseIx(state);
            if (choice.band != NONE) {
                loose_[k] = choice.score_mean >= kLooseMeanScore;
                if (choice.band < 3) {
                    BranchOnConfig<0>(choice.band, choice.digit, choice.config, state);
                } else {
                    BranchOnConfig<1>(choice.band - 3, choice.digit, choice.config, state);
                }
                return;
            }
        }
#else
        (void) use_ix;
#endif
        if (band_and_value.first < 3) {
            BranchOnBandAndValue<0>(tables.mod3[band_and_value.first], band_and_value.second, state);
        } else {
            BranchOnBandAndValue<1>(tables.mod3[band_and_value.first], band_and_value.second, state);
        }
    }

    // the original's branching on the lowest configuration of a digit, counting refutations
    template<int vertical>
    void BranchOnBandAndValue(int band_idx, const Cells08 &value_mask, State &state) {
        DepthGuard depth_guard(depth_);
        Band &band = state.bands[vertical][band_idx];
        Cells08 value_configurations = band.configurations & value_mask;
        num_guesses_++;
        State state_copy = state;
        Cells08 assignment_elims = value_configurations.ClearLowBit();
        state_copy.bands[vertical][band_idx].eliminations |= assignment_elims;
        if (Base::template BandEliminate<vertical>(state_copy, band_idx)) {
            CountSolutions(state_copy);
            if (num_solutions_ == limit_) return;
        } else {
            num_fails_++;
        }
        Cells08 negation_elims = value_configurations ^ assignment_elims;
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (Base::template BandEliminate<vertical>(state, band_idx)) {
            CountSolutions(state);
        } else {
            num_fails_++;
        }
    }

    // branch on asserting vs. eliminating configuration `config` of `digit` in the band
    template<int vertical>
    void BranchOnConfig(int band_idx, int digit, int config, State &state) {
        DepthGuard depth_guard(depth_);
        Band &band = state.bands[vertical][band_idx];
        Cells08 value_configurations = band.configurations & tables.one_value_mask[digit];
        num_guesses_++;
        State state_copy = state;
        Cells08 assignment_elims = value_configurations.and_not(ConfigLaneMask(config));
        state_copy.bands[vertical][band_idx].eliminations |= assignment_elims;
        if (Base::template BandEliminate<vertical>(state_copy, band_idx)) {
            CountSolutions(state_copy);
            if (num_solutions_ == limit_) return;
        } else {
            num_fails_++;
        }
        Cells08 negation_elims = value_configurations & ConfigLaneMask(config);
        state.bands[vertical][band_idx].eliminations |= negation_elims;
        if (Base::template BandEliminate<vertical>(state, band_idx)) {
            CountSolutions(state);
        } else {
            num_fails_++;
        }
    }

#ifdef __AVX2__
    // The score is computed for all 54 pairs at once in a "digit-per-byte" layout (byte d of a
    // vector holds a small integer for digit d): band configurations and box rows are
    // transposed into this layout, compatibility sets are pshufb lookups into the 8-entry rows
    // of IxTables::compat, popcounts are nibble lookups.

    struct IxTables {
        __m256i elem_ctrl[8];  // pshufb: bytes 0..7 <- low byte of element e, 8..15 <- high
        __m256i bit[8];        // set1_epi8(1 << c)
        __m256i compat[3];     // byte A (0..7): configs whose element in peer i lies in set A
        // the two configurations using element 0/2 (lanes) resp. 1 of peer j, as single bits
        __m256i ua02[3], ub02[3], ua1[3], ub1[3];
        // GFNI transposition controls (element e's low byte to qword byte 7 - e, high byte to
        // 15 - e) for band configurations (elements 0..5), box rows 0/2 and box row 1.
        __m256i gf_ctrl_cfg, gf_ctrl_row02, gf_ctrl_row1;

        IxTables() noexcept {
            auto make_gf = [](int first, int count) {
                alignas(32) int8_t b[32];
                for (int k = 0; k < 32; k++) b[k] = (int8_t) 0x80;
                for (int lane = 0; lane < 2; lane++) {
                    for (int e = 0; e < count; e++) {
                        b[lane * 16 + 7 - e] = (int8_t) (2 * (first + e));
                        b[lane * 16 + 15 - e] = (int8_t) (2 * (first + e) + 1);
                    }
                }
                return _mm256_load_si256((const __m256i *) b);
            };
            gf_ctrl_cfg = make_gf(0, 6);
            gf_ctrl_row02 = make_gf(0, 3);
            gf_ctrl_row1 = make_gf(4, 3);
            for (int e = 0; e < 8; e++) {
                char lo = (char) (2 * e), hi = (char) (2 * e + 1);
                elem_ctrl[e] = _mm256_setr_epi8(lo, lo, lo, lo, lo, lo, lo, lo,
                                                hi, hi, hi, hi, hi, hi, hi, hi,
                                                lo, lo, lo, lo, lo, lo, lo, lo,
                                                hi, hi, hi, hi, hi, hi, hi, hi);
                bit[e] = _mm256_set1_epi8((char) (1 << e));
            }
            // kElem[c][p]: element (row within the box for horizontal bands, column for
            // vertical bands) where configuration c places the digit in the band's box peer p
            // (see the configuration table above struct Band).
            const int kElem[6][3] = {
                    {0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}, {0, 2, 1}, {1, 0, 2}};
            for (int p = 0; p < 3; p++) {
                uint8_t using_elem[3] = {0, 0, 0};
                for (int c = 0; c < 6; c++) using_elem[kElem[c][p]] |= (uint8_t) (1u << c);
                alignas(32) uint8_t t[32] = {0};
                for (int a = 0; a < 8; a++) {
                    uint8_t m = 0;
                    for (int e = 0; e < 3; e++) if ((a >> e) & 1) m |= using_elem[e];
                    t[a] = t[16 + a] = m;
                }
                compat[p] = _mm256_load_si256((const __m256i *) t);
                auto lo = [](uint32_t m) { return (uint8_t) (m & (0u - m)); };
                auto hi = [](uint32_t m) { return (uint8_t) (m & (m - 1)); };
                alignas(32) uint8_t a[32] = {0}, b[32] = {0}, c[32] = {0}, d[32] = {0};
                for (int k = 0; k < 16; k++) {
                    a[k] = lo(using_elem[0]); a[16 + k] = lo(using_elem[2]);
                    b[k] = hi(using_elem[0]); b[16 + k] = hi(using_elem[2]);
                    c[k] = c[16 + k] = lo(using_elem[1]);
                    d[k] = d[16 + k] = hi(using_elem[1]);
                }
                ua02[p] = _mm256_load_si256((const __m256i *) a);
                ub02[p] = _mm256_load_si256((const __m256i *) b);
                ua1[p] = _mm256_load_si256((const __m256i *) c);
                ub1[p] = _mm256_load_si256((const __m256i *) d);
            }
        }
    };

    // byte k of the result is 0xff iff bit (k & 7) of the byte selected by ctrl is set
    static inline __m256i ExpandBits(const __m256i &x, const __m256i &ctrl, const __m256i &bitsel) {
        __m256i sel = _mm256_and_si256(_mm256_shuffle_epi8(x, ctrl), bitsel);
        return _mm256_cmpeq_epi8(sel, bitsel);
    }

    static inline __m256i PopcountBytes(const __m256i &x, const __m256i &lut, const __m256i &lo4) {
        __m256i lo = _mm256_shuffle_epi8(lut, _mm256_and_si256(x, lo4));
        __m256i hi = _mm256_shuffle_epi8(lut, _mm256_and_si256(_mm256_srli_epi16(x, 4), lo4));
        return _mm256_add_epi8(lo, hi);
    }

#ifdef __GFNI__
    // 8x8 bit-matrix transpose per qword: byte d = set of elements containing digit d
    static inline __m256i GfTranspose(const __m256i &x, const __m256i &ctrl) {
        return _mm256_gf2p8affine_epi64_epi8(
                _mm256_set1_epi64x((long long) 0x8040201008040201ull),
                _mm256_shuffle_epi8(x, ctrl), 0);
    }
#endif

    // Returns the chosen pair (band NONE if no band-digit pair has exactly two configurations)
    // and the mean score of all candidate pairs. Caller guarantees an unfixed band exists.
    static inline Choice ChooseIx(const State &state) {
        static const IxTables T{};
        uint32_t unfixed = 0;
        for (int b = 0; b < 6; b++) {
            unfixed |= (uint32_t) (state.bands[b / 3][b % 3].configurations.Popcount() >= 10) << b;
        }
        const __m256i bitsel = _mm256_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128,
                                                1, 2, 4, 8, 16, 32, 64, -128,
                                                1, 2, 4, 8, 16, 32, 64, -128,
                                                1, 2, 4, 8, 16, 32, 64, -128);
        const __m256i lo4 = _mm256_set1_epi8(0x0f);
        const __m256i popcnt_lut = _mm256_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
                                                    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4);
        const __m256i w60_lut = _mm256_setr_epi8(0, 60, 30, 20, 15, 12, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                 0, 60, 30, 20, 15, 12, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        (void) bitsel;

        // configuration sets per digit: cfgv[x] low lane = horizontal band x, high = vertical
        __m256i cfgv[3], nv[3], wv[3];
        for (int x = 0; x < 3; x++) {
            __m256i r = _mm256_set_m128i(state.bands[1][x].configurations.vec,
                                         state.bands[0][x].configurations.vec);
#ifdef __GFNI__
            __m256i acc = GfTranspose(r, T.gf_ctrl_cfg);
#else
            __m256i acc = _mm256_setzero_si256();
            for (int c = 0; c < 6; c++) {
                acc = _mm256_or_si256(acc, _mm256_and_si256(
                        ExpandBits(r, T.elem_ctrl[c], bitsel), T.bit[c]));
            }
#endif
            cfgv[x] = acc;
            nv[x] = PopcountBytes(acc, popcnt_lut, lo4);  // configurations per digit
            wv[x] = _mm256_shuffle_epi8(w60_lut, nv[x]);   // weights 60 / n
        }
        __m256i w16[6], score[6];
        for (int x = 0; x < 3; x++) {
            w16[x] = _mm256_cvtepu8_epi16(_mm256_castsi256_si128(wv[x]));
            w16[3 + x] = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(wv[x], 1));
        }
        for (int b = 0; b < 6; b++) score[b] = _mm256_setzero_si256();

        // for every box (i, j): compatible configuration pairs per digit, added to the scores of
        // horizontal band i (weighted by vertical band j's counts) and vice versa
        for (int i = 0; i < 3; i++) {
            __m128i h128 = _mm256_castsi256_si128(cfgv[i]);
            __m256i hh = _mm256_set_m128i(h128, h128);
            for (int j = 0; j < 3; j++) {
                const __m256i cells = state.boxen[i * 3 + j].cells.vec;
                // digits' column sets in rows 0 (low lane) and 2 (high lane), and row 1 (low)
#ifdef __GFNI__
                __m256i r02 = GfTranspose(cells, T.gf_ctrl_row02);
                __m256i r1 = GfTranspose(cells, T.gf_ctrl_row1);
#else
                __m256i r02 = _mm256_setzero_si256(), r1 = _mm256_setzero_si256();
                for (int c = 0; c < 3; c++) {
                    r02 = _mm256_or_si256(r02, _mm256_and_si256(
                            ExpandBits(cells, T.elem_ctrl[c], bitsel), T.bit[c]));
                    r1 = _mm256_or_si256(r1, _mm256_and_si256(
                            ExpandBits(cells, T.elem_ctrl[4 + c], bitsel), T.bit[c]));
                }
#endif
                __m128i v128 = _mm256_extracti128_si256(cfgv[j], 1);
                __m256i vv = _mm256_set_m128i(v128, v128);
                // vertical configurations compatible with the digit in rows 0/2 (resp. 1)
                __m256i k02 = PopcountBytes(_mm256_and_si256(
                        vv, _mm256_shuffle_epi8(T.compat[i], r02)), popcnt_lut, lo4);
                __m256i k1 = PopcountBytes(_mm256_and_si256(
                        vv, _mm256_shuffle_epi8(T.compat[i], r1)), popcnt_lut, lo4);
                // times the number of horizontal configurations using that row of peer j
                auto has = [&](const __m256i &bitc) {
                    return _mm256_cmpeq_epi8(_mm256_and_si256(hh, bitc), bitc);
                };
                __m256i p02 = _mm256_add_epi8(_mm256_and_si256(has(T.ua02[j]), k02),
                                              _mm256_and_si256(has(T.ub02[j]), k02));
                __m256i p1 = _mm256_add_epi8(_mm256_and_si256(has(T.ua1[j]), k1),
                                             _mm256_and_si256(has(T.ub1[j]), k1));
                __m128i bsum = _mm_add_epi8(
                        _mm_add_epi8(_mm256_castsi256_si128(p02), _mm256_extracti128_si256(p02, 1)),
                        _mm256_castsi256_si128(p1));
                __m256i b16 = _mm256_cvtepu8_epi16(bsum);
                score[i] = _mm256_add_epi16(score[i], _mm256_mullo_epi16(w16[3 + j], b16));
                score[3 + j] = _mm256_add_epi16(score[3 + j], _mm256_mullo_epi16(w16[i], b16));
            }
        }

        // argmin (and mean) over candidate pairs: unfixed band, exactly two configurations
        const __m256i two16 = _mm256_set1_epi16(2);
        __m256i key[6];
        __m256i mn = _mm256_set1_epi16(-1);
        __m256i sum32 = _mm256_setzero_si256();
        int ncand = 0;
        for (int b = 0; b < 6; b++) {
            __m128i n8 = b < 3 ? _mm256_castsi256_si128(nv[b])
                               : _mm256_extracti128_si256(nv[b - 3], 1);
            __m256i cand = _mm256_cmpeq_epi16(_mm256_cvtepu8_epi16(n8), two16);
            if (!((unfixed >> b) & 1)) cand = _mm256_setzero_si256();
            key[b] = _mm256_or_si256(score[b], _mm256_xor_si256(cand, _mm256_set1_epi16(-1)));
            mn = _mm256_min_epu16(mn, key[b]);
            sum32 = _mm256_add_epi32(sum32, _mm256_madd_epi16(
                    _mm256_and_si256(score[b], cand), _mm256_set1_epi16(1)));
            ncand += __builtin_popcount((uint32_t) _mm256_movemask_epi8(cand)) >> 1;
        }
        __m128i mn128 = _mm_min_epu16(_mm256_castsi256_si128(mn), _mm256_extracti128_si256(mn, 1));
        uint32_t min_val = (uint32_t) _mm_cvtsi128_si32(_mm_minpos_epu16(mn128)) & 0xffffu;
        if (min_val == 0xffffu) return {NONE, 0, 0, 0};
        __m128i s128 = _mm_add_epi32(_mm256_castsi256_si128(sum32),
                                     _mm256_extracti128_si256(sum32, 1));
        s128 = _mm_add_epi32(s128, _mm_shuffle_epi32(s128, 0x4e));
        s128 = _mm_add_epi32(s128, _mm_shuffle_epi32(s128, 0xb1));
        uint32_t score_mean = (uint32_t) _mm_cvtsi128_si32(s128) / (uint32_t) ncand;
        __m256i target = _mm256_set1_epi16((short) min_val);
        for (int b = 0; b < 6; b++) {
            uint32_t m = (uint32_t) _mm256_movemask_epi8(_mm256_cmpeq_epi16(key[b], target));
            if (m) {
                int d = __builtin_ctz(m) >> 1;
                alignas(32) uint8_t bytes[32];
                _mm256_store_si256((__m256i *) bytes, cfgv[b % 3]);
                return {(uint32_t) b, d, __builtin_ctz(bytes[(b < 3 ? 0 : 16) + d]),
                        (uint16_t) score_mean};
            }
        }
        return {NONE, 0, 0, 0};  // unreachable
    }
#endif  // __AVX2__

    size_t SolveSudoku(const char *input, size_t limit, char *solution, size_t *num_guesses) {
        limit_ = limit;
        num_solutions_ = 0;
        num_guesses_ = 0;
        num_fails_ = 0;
        depth_ = 0;
        bool pencilmark = input[81] >= '.';
        State state;
        if (pencilmark ? Base::InitPencilmarkByBox(input, state)
                       : Base::InitVanillaByBand(input, state)) {
            CountSolutions(state);
            if (solution_mode == 1) Base::ExtractSolution(solution_, solution);
        }
        *num_guesses = num_guesses_;
        return num_solutions_;
    }
};

SolverIntersection<0> solver_ix_none{};
SolverIntersection<1> solver_ix_last{};

} // namespace

size_t TdokuSolverIntersection(const char *puzzle, size_t limit, bool return_last,
                               char *solution, size_t *num_guesses) {
    return return_last ? solver_ix_last.SolveSudoku(puzzle, limit, solution, num_guesses)
                       : solver_ix_none.SolveSudoku(puzzle, limit, solution, num_guesses);
}
