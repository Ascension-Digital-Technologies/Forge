# Integrated Forge vs LLVM benchmark

This benchmark compares one application-style workload built from equivalent operations through Forge and Clang/LLVM. It combines packed memory transforms, shared/deep expression DAGs, a real dynamic vectorized memory loop, integer reductions, state updates, dependent integer mixing, branch-heavy control flow, call chains, multi-recurrence loops, branch merges, floating-point dot products, and floating recurrence work.

Both implementations are compiled at `-O3` for the selected vector ISA. The default acceptance target is AVX-512. The harness alternates execution order and rejects the run unless both implementations produce the identical final checksum.

Reproduce the complete comparison—including integrated runtime, compile-time medians, and the 27-kernel broad matrix—with:

```sh
python3 benchmarks/real-app/run.py --build build/release-strict --x86-vector avx512
```

Current recorded acceptance run (August 12, 2026):

- integrated checksum: `db6c484d95ce84e7`
- Forge integrated runtime: 3.718479 ms
- LLVM integrated runtime: 8.925403 ms
- Forge integrated speedup: **2.400283x**
- Forge compile median: 36.779 ms
- LLVM compile median: 201.211 ms
- Forge compile speedup: **5.471x**
- 27-kernel geometric-mean Forge runtime speedup: **1.182x**
- broad object `.text`: Forge 2692 bytes, LLVM 3709 bytes

The earlier Pass 18 O3 convergence bug is fixed. It was caused by memory dataflow using the alias relation as a fixed-point state-equality relation: identical imprecise locations return `may_alias`, so the analysis could report change forever. Convergence now compares abstract state identity while optimization legality continues to use conservative alias queries.
