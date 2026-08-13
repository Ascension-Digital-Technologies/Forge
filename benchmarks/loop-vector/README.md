# Dynamic loop-vector benchmark

This benchmark compares Forge's machine-level counted-loop vectorizer with both an equivalent Forge scalar loop and Clang/LLVM generated code. The current acceptance configuration uses `-O3` on both compilers and applies the corresponding x86 target flags to Clang.

The fixture is a canonical countdown loop over contiguous i64 memory. Forge retains the original scalar loop as the exact tail path and inserts a full-width vector loop ahead of it. A semantically equivalent count-up fixture is kept as the Forge scalar control because it is outside this recognizer's canonical form.

Run:

```sh
python3 benchmarks/loop-vector/run.py --target avx2 --samples 5 --forge-opt O3 --llvm-opt O3
python3 benchmarks/loop-vector/run.py --target avx512 --samples 5 --forge-opt O3 --llvm-opt O3
```

`scalar/vector` is Forge scalar time divided by Forge vector time. `vector/llvm` is Forge vector time divided by Clang/LLVM time, so values below `1.0x` mean Forge generated faster code.

The current AVX-512 acceptance run reaches about 4x Forge-scalar speedup at 1K-4K elements and beats LLVM on those larger cases. AVX2 still trails LLVM despite materially improving over Forge scalar, so target-specific results are reported separately rather than generalized.
