// noinline wrappers of the choosers for static analysis (llvm-mca): see mca.sh
#include "features.h"
#include "simd_ds.h"
#include "simd_dv.h"
#include "simd_lv.h"

static sds::Tables *ds_tables;
static sdv::Tables *dv_tables;
static slv::Tables *lv_tables;
extern "C" {
__attribute__((noinline, flatten)) int k_mrv(const State *s) {
    asm volatile("# LLVM-MCA-BEGIN mrv" ::: "memory");
    auto r = Base::ChooseBandAndValueToBranch(*s);
    asm volatile("# LLVM-MCA-END" ::: "memory");
    return (int) r.first + _mm_cvtsi128_si32(r.second.vec);
}
__attribute__((noinline, flatten)) int k_ix(const State *s) {
    asm volatile("# LLVM-MCA-BEGIN ix" ::: "memory");
    auto c = SolverIntersection<0>::ChooseIx(*s);
    asm volatile("# LLVM-MCA-END" ::: "memory");
    return (int) c.band + c.digit;
}
__attribute__((noinline, flatten)) int k_ds(const State *s) {
    asm volatile("# LLVM-MCA-BEGIN ds" ::: "memory");
    int b, d, c;
    sds::Choose(*s, *ds_tables, b, d, c);
    asm volatile("# LLVM-MCA-END" ::: "memory");
    return b + d + c;
}
}
extern "C" __attribute__((noinline, flatten)) int k_dv(const State *s) {
    asm volatile("# LLVM-MCA-BEGIN dv" ::: "memory");
    int b, d, c;
    sdv::Choose(*s, *dv_tables, b, d, c);
    asm volatile("# LLVM-MCA-END" ::: "memory");
    return b + d + c;
}
extern "C" __attribute__((noinline, flatten)) int k_lv(const State *s) {
    asm volatile("# LLVM-MCA-BEGIN lv" ::: "memory");
    int b, d, c;
    slv::Choose(*s, *lv_tables, b, d, c);
    asm volatile("# LLVM-MCA-END" ::: "memory");
    return b + d + c;
}
int main() { ds_tables = new sds::Tables(sds::Params{}); dv_tables = new sdv::Tables(sdv::Params{}); lv_tables = new slv::Tables(slv::Params{}); return 0; }
