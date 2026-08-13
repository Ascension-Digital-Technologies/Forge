# Broad Forge/LLVM benchmark matrix

This benchmark is an optimization acceptance gate, not a single-score contest. It runs deterministic randomized differential checks before timing and reports each workload independently so a compiler change cannot hide a severe regression behind one unusually large win.

Workload families currently cover dependent integer arithmetic, multi-recurrence loops, data-dependent control flow, high register pressure, floating-point recurrence, fixed-offset memory loads, integer and floating-point call chains, i64/i32 SIMD reductions, packed integer expression maps, scalar memory updates, call-crossing liveness, branch diamonds, floating dot products, and overwritten-store elimination.

```bash
python3 benchmarks/broad/run.py --samples 9 --opt-level O2
python3 benchmarks/broad/run.py --samples 9 --opt-level O3
python3 benchmarks/broad/run.py --samples 5 --check
```

`--check` applies the per-kernel limits in `thresholds.json`. The limits are intentionally independent and moderately tolerant of host noise. They are not claims that the present ratios are final performance targets; they prevent broad regressions while optimization work proceeds. The two read-only memory limits were recalibrated after removing the old store-forwarding bias; the reported ratios themselves are never adjusted.

Before compiling, the runner also rejects any broad-benchmark function symbol that appears in `src/` or `include/`, preventing direct benchmark-name special casing in production code. Each requested sample is measured in both Forge-first and LLVM-first order to cancel systematic order bias. Read-only memory kernels use preinitialized input rings so the timing does not accidentally reward or punish a backend because of a store-forwarding artifact immediately before the call. Integer recurrence references use defined modulo-2^64 arithmetic rather than C signed-overflow undefined behavior.

Results are written to `build/broad-bench/results-o2.json` or `results-o3.json`, including Forge and LLVM `.text` sizes.

## Extended coverage

The broad gate also includes:

- `multi_recur_100`: four simultaneously loop-carried integer recurrences and a rotating backedge.
- `branch_merge_200`: a branch diamond with SSA merge parameters on every iteration.
- `float_dot4`: floating-point loads, multiplication, reduction, and XMM pressure.
- `memory_sum_i32_8`: contiguous eight-lane i32 reduction, exercising packed `PADDD` lowering and 32-bit wraparound semantics.
- `memory_add4`: contiguous in-place four-lane i64 map-add/store, exercising scalar broadcast plus packed load/compute/store SLP.
- `memory_xor4_i32`: four-lane i32 XOR update through the generic packed expression path.
- `memory_and4_i64`: four-lane i64 AND update through the same operation-generic SLP path.
- `memory_xor_copy8_i32`: eight-lane out-of-place i32 XOR map with distinct source and destination buffers, exercising two-vector copy/transform SLP.
- `memory_add_copy8_i32`: eight-lane vector-to-vector i32 add map with two independent source arrays and a third destination, exercising true packed array+array SLP.
- `memory_chain_copy8_i32`: eight-lane three-source expression chain `(a ^ b) + c`, exercising packed intermediates that remain in XMM registers across two operations.
- `memory_deep_chain_copy8_i32`: eight-lane five-source expression chain `(((a ^ b) + c) & d) - e`, exercising variable-depth packed chains with four ordered operations and no scalar intermediate materialization.
- `memory_branch_dag_copy8_i32`: eight-lane five-source branching expression `((a ^ b) + (c & d)) - e`, exercising true packed subtree formation and postfix DAG evaluation.
- `memory_shared_dag_copy8_i32`: eight-lane five-source shared-subexpression DAG where `(a ^ b)` feeds two packed branches and is computed once.
- `memory_pressure_dag_copy8_i32`: eight-lane high-pressure shared DAG with ten reusable packed intermediates, exercising cost-based cache retention, XMM eviction, and recomputation under pressure.
- `store_overwrite`: alias-aware elimination of a store fully overwritten before any read.
- `global_store_overwrite`: global dead-store elimination across basic-block boundaries.

These are intentionally distinct from the original Fibonacci, branch-walk, and scalar floating recurrence kernels. They prevent improvements to one exact CFG or recurrence shape from standing in for broad backend quality.

Very small read-only reduction kernels can be sensitive to code placement and front-end effects because the measured call itself is around 1–2 ns on modern CPUs. The gate keeps those rows for transparency, but changes are accepted based on generated-code quality, repeated runs, semantic validation, and the broader matrix rather than a single volatile ratio.
