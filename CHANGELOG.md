## Unreleased

### O3 convergence, compile throughput, and AVX loop cleanup

- Fixed a non-terminating fixed-point bug in memory forwarding and dead-store dataflow for loops carrying imprecise pointer locations. Dataflow convergence now compares abstract-state identity instead of requiring `must_alias`, while optimization legality remains conservatively alias-aware.
- Added a regression containing a loop-carried unknown pointer through both memory forwarding and dead-store elimination.
- Split the normal `PassManager::run` path from the instrumented reporting path so production compilation does not allocate per-pass timing records unless requested.
- Changed `forge compile` to skip whole-module verification after every optimization pass and verify once at the optimized-IR boundary; `forge-opt` retains per-pass verification for diagnostics.
- Reduced the broad O3 compile median on the acceptance host from roughly 1.47 s to roughly 36.8 ms, versus roughly 201 ms for Clang/LLVM on the equivalent reference source.
- Moved `vzeroupper` from individual wide packed operations to ABI call/return boundaries and replaced legacy SSE scalar broadcast setup with VEX `vmovd/vmovq` before AVX2/AVX-512 broadcasts, eliminating hot-loop AVX/SSE transition penalties.
- The O3 AVX-512 dynamic loop now measures about 4.13x/3.95x over Forge scalar at 1024/4096 elements and about 0.82x LLVM time on both larger cases.
- Added a reproducible `benchmarks/real-app/run.py` end-to-end Forge-vs-LLVM benchmark. The integrated workload now includes the formerly excluded dynamic vector loop and records a 2.40x Forge runtime advantage, 5.47x compile-time advantage, and 1.182x broad-matrix geometric-mean runtime advantage on the acceptance host.


### Dynamic counted-loop vectorization

- Added machine-level vectorization for canonical countdown loops over contiguous i32/i64 in-place `add`, `and`, `or`, and `xor` updates with loop-invariant scalar operands.
- The transform inserts a target-width vector header/body and preserves the original scalar loop as the exact tail, handling arbitrary dynamic trip counts without over-reading memory.
- Added target-aware profitability gating and loop-vector telemetry for candidates, cost/target rejections, selections, and selected 128/256/512-bit widths.
- Added `forge compile --x86-vector=sse2|sse41|avx|avx2|avx512`, making the existing target profiles and native XMM/YMM/ZMM backends selectable through the production CLI.
- Added structural AVX2 regression coverage proving a four-lane i64 vector loop, retained scalar tail, valid machine IR, and VEX emission.
- Added `benchmarks/loop-vector`, a focused dynamic-trip-count benchmark against an equivalent Forge scalar loop and Clang/LLVM. Five-sample medians reached 1.555x/1.582x scalar speedup for AVX2 at 1024/4096 elements and 3.127x/3.185x for AVX-512.

### Native AVX-512/ZMM and opmask backend

- Added native 512-bit AVX-512/ZMM emission for packed integer SLP operations using EVEX-encoded `VMOVDQU32/64`, `VPADDD/Q`, `VPSUBD/Q`, `VPANDD/Q`, `VPORD/Q`, `VPXORD/Q`, and `VPBROADCASTD/Q`.
- Added 16-lane i32 SLP candidate formation and width selection that chooses 128, 256, or 512 bits based on both target capability and actual pack size.
- Raised the AVX-512 backend capability from 256 to 512 bits while preserving AVX2 and SSE2 fallback paths.
- Extended packed map, map2, map3, arbitrary chain, DAG, and reusable-DAG emission to accept 64-byte ZMM chunks.
- Added machine-level active-lane metadata plus k-register mask materialization with `KMOVW`; masked ZMM loads use zeroing semantics and masked stores preserve inactive lanes.
- Added verifier checks for valid AVX-512 lane masks and regression coverage proving 512-bit selection, EVEX emission, and opmask generation.
- Byte-level validation disassembles the masked eight-lane i64 kernel as `vpbroadcastq zmm`, masked `vmovdqu64`, `vpaddq zmm`, masked `vmovdqu64`, and `vzeroupper`. Direct execution on an AVX-512 host updated only the five active lanes as expected.
- Full validation: 66/66 tests passing.

### Native AVX2/YMM packed integer backend

- Added native 256-bit AVX2/YMM emission for target-selected packed integer SLP operations, including in-place/scalar maps, vector-to-vector maps, chained maps, arbitrary-depth chains, DAGs, and reusable DAGs.
- Added VEX3 encoding helpers for `VMOVDQU`, `VMOVDQA`, `VPADDD/Q`, `VPSUBD/Q`, `VPAND`, `VPOR`, `VPXOR`, and `VPBROADCASTD/Q`, plus `VZEROUPPER` cleanup at packed AVX2 boundaries.
- Machine SLP pseudos now carry the selected physical vector width into code generation so backend emission follows the profitability decision rather than inferring a width later.
- AVX2 target profiles now expose a 256-bit backend width. AVX-512 hardware profiles now expose native 512-bit ZMM backend width.
- Added regression coverage proving AVX2 lowering selects 256-bit packs and emits VEX code while the existing SSE2 path remains available.
- Byte-level validation disassembles the generated 4-lane i64 map as `vpbroadcastq` + `vmovdqu ymm` + `vpaddq ymm` + `vmovdqu ymm` + `vzeroupper`; direct execution produced the expected lane results.
- Full validation: 64/64 tests passing.

### Target-aware SLP profitability

- Added a configurable machine-level `SlpCostModel` covering vector feature availability, vector width, instruction latency/throughput, scalar/vector memory costs, setup overhead, register budget, register-pressure penalties, and minimum required speedup.
- Added x86-64 SSE2, SSE4.1, AVX, AVX2, and AVX-512 target profiles with explicit ISA feature availability, vector-register budgets, and AVX-512 mask-register capacity.
- Separated hardware vector width from backend-emittable width. Hardware width remains separate from backend-emittable width: SSE/AVX1 profiles use 128-bit XMM emission, AVX2 uses native 256-bit YMM emission, and AVX-512 uses native 512-bit ZMM/EVEX emission.
- Replaced the single generic vector-operation estimate with instruction-sensitive integer ALU costs plus explicit broadcast/shuffle costs and memory-layout multipliers for aligned, unaligned, interleaved, strided, and gather/scatter access patterns.
- Applied profitability selection to contiguous in-place/map, map2/map3, chain, DAG, and DAG-reuse SLP candidates before they are committed, including mixed-operation costing for chains and DAGs.
- Added SLP decision telemetry for estimated scalar/vector, memory, shuffle, and register-pressure costs plus selected 128/256/512-bit widths.
- Added `LowerOptions::slp_cost_model`, plumbing target-aware SLP selection through the normal IR-to-machine lowering pipeline rather than requiring callers to invoke the optimizer manually.
- Added regression coverage proving legal packs are rejected when vector support is unavailable or vector costs are unfavorable, while profitable candidates are selected and target/backend width constraints are preserved.

# Changelog

## 2.0.12 - 2026-08-12

- Added cost-based scheduling for reusable packed SLP DAGs. Shared nodes are scored by recomputation cost and reuse count so the backend can retain valuable packed intermediates while allowing cheap values to be recomputed under XMM pressure.
- Added bounded XMM cache eviction for reusable DAG evaluation, preferring eviction of low-value retained vectors instead of forcing packed spill traffic.
- Added a high-pressure eight-lane shared-DAG benchmark with ten reusable packed intermediates; final corrected 13-sample runs measured 0.454x LLVM at O2 and 0.470x at O3.
- Added exact 64-bit SIMD tail loads/stores for two-lane i32 chunks, preventing 128-bit over-reads and adjacent-lane overwrites.
- Fixed integer spill-cache register ownership so r8/r9 cache slots are disabled when those physical registers are already allocated to live virtual values, preventing pointer/value corruption under high register pressure.
- Added differential regressions for high-pressure reusable DAG scheduling, SIMD tail-width correctness, and scalar register-pressure/spill-cache safety.
- Refreshed the README to the final 27-kernel acceptance matrix; Forge .text measured 3074 bytes versus LLVM 3571 bytes at O2 and 3587 bytes at O3.


All notable changes to Forge are documented here. Forge follows [Semantic Versioning](https://semver.org/).

## 2.0.10 - 2026-08-12

### Added
- Added true branching integer SLP DAG formation for contiguous i32/i64 `add`, `sub`, `and`, `or`, and `xor` expression trees.
- Added a compact postfix DAG program carried in machine IR, with verifier stack checking and bounded XMM evaluation depth.
- Added structural and interpreter/JIT differential regressions for branching expressions such as `((a ^ b) + (c & d)) - e`.
- Added `memory_branch_dag_copy8_i32` to the honest broad Forge/LLVM benchmark matrix.

### Performance
- The new eight-lane five-source branching DAG measured approximately `0.570x` LLVM at O2 and `0.625x` at O3 in the corrected acceptance runs.
- Broad benchmark `.text` measured 2097 bytes for Forge versus 2179 bytes for LLVM O2 and 2195 bytes for LLVM O3 in the updated 25-kernel matrix.

## 2.0.9 - 2026-08-12

- Replaced the fixed two-operation SLP-chain ceiling with variable-depth packed i32/i64 expression chains. The machine representation carries an ordered operation stream and N+1 source pointers for N packed operations, without changing the public aggregate layout of `machine::Instruction`.
- Added structural and interpreter/JIT differential regressions for a four-operation i32 chain using five independent sources, including wraparound arithmetic, noncommutative subtraction, source preservation, and six-pointer ABI invocation.
- Expanded the honest broad matrix to 24 kernels with `memory_deep_chain_copy8_i32`; the final 13-sample acceptance runs measured 0.545x LLVM at O2 and 0.540x at O3, while Forge tied LLVM's O2 `.text` size at 1987 bytes.
- Kept all benchmark-integrity protections unchanged: randomized semantic checks, both timing orders, preinitialized read-only inputs, and production-source benchmark-symbol leakage rejection.

## 2.0.8 - 2026-08-12

### Packed expression chains

- Added chained integer SLP maps for contiguous i32/i64 expressions of the form `(a[i] op1 b[i]) op2 c[i]`, preserving the intermediate entirely in XMM registers instead of materializing it through scalar registers or memory.
- Added dedicated `binary_i32_contiguous_map3` / `binary_i64_contiguous_map3` machine operations with explicit first/second packed opcodes, verifier coverage, liveness handling, machine-IR printing, and SSE2 lowering.
- Chained SLP supports ordered lane-wise `add`, `sub`, `and`, `or`, and `xor` operations across 2/4/8 lanes; noncommutative subtraction keeps operand order intact.
- Added structural codegen coverage for i32 `(a ^ b) + c` and interpreter/JIT differential coverage for both i32 wraparound and i64 `(a - b) ^ c`, including source-preservation checks.
- Expanded the honest broad benchmark matrix to 23 kernels with `memory_chain_copy8_i32`; the final 13-sample acceptance runs measured about 0.701x LLVM at both O2 and O3.
- Updated the root README with the final 23-kernel acceptance table and current broad-benchmark code sizes.

### Validation

- Full strict suite: 66/66 passing.
- Corrected 13-sample broad performance gates pass at both `-O2` and `-O3`.
- Benchmark-integrity protections remain unchanged: randomized differential checks, alternating timing order, unbiased read-only inputs, and production-source benchmark-symbol leakage checks all remain enabled.

## 2.0.7 - 2026-08-12

- Added true vector-to-vector SLP maps for contiguous i32/i64 `add`, `sub`, `and`, `or`, and `xor` expressions with independent left/right source arrays and a destination array.
- Added dedicated machine-IR `binary_i32_contiguous_map2` / `binary_i64_contiguous_map2` operations, verifier coverage, machine-IR printing, liveness handling, and SSE2 lowering.
- Added two-vector scheduling for eight-lane maps so independent 128-bit loads and ALU operations can overlap before stores.
- Added JIT/interpreter differential coverage for i32 vector addition and noncommutative i64 vector subtraction, including wraparound values and source-preservation checks.
- Expanded the honest broad benchmark matrix with `memory_add_copy8_i32`; the final 13-sample acceptance run measured about 0.675x LLVM at O2 and 0.671x at O3.
- Fixed the packed-map verifier so the new i64 vector-to-vector form is classified with 64-bit packed operations rather than i32 rules.

## 2.0.6 - 2026-08-12

### Added
- Generalized machine-level SLP expression packs for contiguous i32/i64 `add`, `sub`, `and`, `or`, and `xor` operations.
- Added separate-source/separate-destination packed maps in addition to in-place updates, with 2/4/8-lane legality checks.
- Added SSE2 `PSUBD`/`PSUBQ`, `PAND`, `POR`, and `PXOR` lowering through the generic packed-map backend.
- Added structural and interpreter/JIT differential coverage for i32 XOR, i64 AND, noncommutative i64 subtraction, and out-of-place copy/transform maps.
- Expanded the honest broad benchmark to 21 kernels with `memory_xor4_i32`, `memory_and4_i64`, and `memory_xor_copy8_i32`.

### Changed
- Packed integer maps now schedule two independent vectors together when possible to expose load/ALU overlap.
- Machine-IR printing exposes packed scalar operation and lane count, and the verifier rejects unsupported packed operations or invalid lane counts.
- README performance results now reflect the final pass-7 21-kernel O2/O3 acceptance runs; the eight-lane out-of-place XOR map measures about 0.70x LLVM in both modes on this host.

## 2.0.5 - 2026-08-12

### SIMD / SLP expansion

- Added legality-first contiguous `i32` addition-reduction SLP for 4/8/16/32 lanes using SSE2 packed dword arithmetic while preserving scalar Forge IR semantics.
- Added packed in-place contiguous `i64` map-add/store recognition for 2/4/8 lanes (`dst[i] += delta`) with SSE2 scalar broadcast, packed loads, `PADDQ`, and packed stores.
- Added strict ABI-entry forwarding for terminal reductions and packed map-add leaves so untouched pointer/scalar arguments can be consumed directly from their incoming registers rather than relocated first.
- Added structural codegen regressions for both new SIMD families and interpreter-vs-JIT differential coverage, including integer wraparound and mutable-buffer equivalence cases.
- Expanded the honest broad benchmark with `memory_sum_i32_8` and `memory_add4`; benchmark-source leakage checks and alternating timing order remain unchanged.
- Restored the README benchmark table from the final expanded O2/O3 acceptance runs and explicitly documents the layout sensitivity of sub-2 ns reduction microbenchmarks.

### Validation

- Full strict suite: 66/66 passing.
- Corrected 9-sample broad performance gates pass at both `-O2` and `-O3` with 18 kernels.
- New packed `memory_add4` measures about 0.834x LLVM at O2 and 0.995x at O3 in the final acceptance runs.
- Forge broad-benchmark `.text` is 1673 bytes in both final runs (LLVM: 1507 bytes O2 / 1523 bytes O3).

## 2.0.4 - 2026-08-12

### Windows x64 JIT ABI hardening

- Fixed a Windows-only differential-test crash caused by treating `rdi`/`rsi` as SysV-style volatile leaf scratch registers. On Windows x64 those registers are nonvolatile and must be preserved by callees; the allocator now keeps them out of the unsaved leaf pool on Windows.
- Added a Windows-only allocator regression that rejects unsaved `rdi`/`rsi` assignments in leaf functions, protecting native C++ caller state during repeated JIT calls.
- Kept the System V leaf optimization unchanged, where `rdi`/`rsi` are legitimately caller-saved.
- Extended strict packed-reduction return forwarding so a terminal SIMD reduction can materialize its scalar result directly in `rax`.
- Restored the corrected Forge-vs-LLVM benchmark results to the project README, including O2/O3 ratios, code sizes, methodology, and reproduction commands.

### Validation

- Full local strict suite: 64/64 passing after the ABI fix.
- Explicit Windows-x64 allocator regression passes on the Linux host, and COFF AMD64 object tests remain green.
- Corrected 7-sample broad performance gates pass at both `-O2` and `-O3`; Forge `.text` is 1597 bytes in both runs.
- Benchmark-integrity protections remain unchanged.

## 2.0.3 - 2026-08-12

### SIMD / SLP backend

- Added legality-first machine-level SLP recognition for straight-line contiguous `i64` addition reductions with 4, 8, or 16 scalar lanes. The matcher is structural and does not inspect function names or benchmark identifiers.
- Added the internal `reduce_add_i64_contiguous` machine pseudo-op so packed lowering remains a backend concern and does not leak vector-only types into Forge's public scalar IR.
- Added SSE2 packed reduction lowering using unaligned 128-bit loads, `PADDQ`, two independent accumulator chains for 8/16-lane reductions, and a final horizontal reduction.
- Eliminated the scalar load/add tree when a reduction is packed, preserving one memory read per original scalar lane rather than duplicating accesses.
- Improved machine-IR printing for pointer loads and `$memptr` arithmetic so folded displacements are visible during backend audits.
- Added regression coverage proving an ordinary contiguous eight-element `i64` reduction selects the packed machine operation and leaves no redundant scalar loads.

### Validation

- Full strict suite: 66/66 passing.
- Corrected 13-sample Forge-vs-LLVM broad performance gates pass at both `-O2` and `-O3`, with randomized semantic validation and benchmark-integrity checks unchanged.
- `memory_sum_8` improved to approximately 0.83x LLVM at both O2 and O3 after balanced packed accumulation, while `branch_walk`, call, floating-call, branch-merge, and floating-dot results remained stable.
- Forge broad-benchmark `.text` is 1600 bytes at both O2 and O3 in the final acceptance run (LLVM: 1411 bytes O2 / 1427 bytes O3).

## 2.0.2 - 2026-08-12

### Optimizer and backend performance

- Added a pre-runtime-unroll scalar-cleanup fixpoint so if-converted / CFG-fused loops are costed after dead copies and duplicate expressions have been removed, exposing profitable loops that previously looked artificially too large.
- Refined generic runtime counted-loop unrolling to use 8x for tiny bodies, 4x for medium bodies, and 2x for heavier straight-line bodies, improving branch-heavy loops without excessive code growth.
- Added two-address register affinity for integer `select`, allowing the false arm to occupy the final `CMOV` destination and eliminating repeated initializer copies.
- Added compact byte-sized `TEST r8, imm8` lowering for low-byte masks, shrinking common bit-test / select sequences without changing semantics.
- Added strict call-free floating ABI expression forwarding, keeping simple incoming XMM argument chains in ABI registers through the return rather than copying through allocator temporaries.
- Added strict call-free integer ABI-to-RAX expression forwarding for single-use leaf expression chains.
- Added deduplicated RIP-relative floating literal pools and rematerialized floating-literal loads, replacing repeated `movabs + movq` materialization.
- Added compact 8-byte SysV call-alignment reservation and safe first-call ABI-register forwarding, reducing wrapper/call-chain shuffles.
- Added single-use integer constant rematerialization directly at outgoing call argument uses.
- Added direct compute -> pointer-store -> return forwarding so terminal stored values can remain in RAX/EAX through the store.
- Preserved selective 16-byte alignment for actual internal call targets while rejecting broader hot-loop padding that did not produce a reliable net speedup.

### Benchmark integrity and validation

- Kept all corrected benchmark methodology unchanged: randomized differential validation, alternating Forge/LLVM timing order, benchmark-symbol production-source scan, unbiased read-only memory setup, and defined integer reference arithmetic.
- Rejected residual-countdown and hot-loop-alignment experiments when repeated measurements did not justify their code-size or layout cost.
- Full strict suite: 66/66 passing.
- Corrected 13-sample Forge-vs-LLVM broad performance gates pass at both `-O2` and `-O3`.
- `branch_walk` is approximately 0.85x LLVM on the final O2 run, `branch_merge` approximately parity, call-chain approximately parity, floating-call and floating-dot workloads approximately parity, with Forge `.text` at 1601 bytes versus LLVM's 1411 bytes for O2.

## 2.0.1 - 2026-08-12

### Performance and backend integrity

- Generalized pointer-offset folding across multiple effective address-only uses, including loads already folded into `$memptr` arithmetic, so shared struct/array addresses encode directly as `[base+disp]` at each use.
- Added direct integer pointer loads and stores to and from allocated physical registers, avoiding unnecessary `rax` shuttles.
- Added direct destination accumulation for integer pointer-memory arithmetic instead of routing through `rax` when the result has a physical register.
- Added scalar floating pointer-load folding into SSE arithmetic memory operands (`addss/addsd`, `subss/subsd`, `mulss/mulsd`, `divss/divsd`), eliminating short-lived XMM temporaries and spill traffic in array/vector-style kernels.
- Extended machine verification for floating `$memptr` arithmetic so the address operand is correctly treated as integer-class while value operands remain floating-class.
- Made stack-passed entry arguments frameless when no real local/spill frame exists, with rsp-relative entry parallel-copy sources that account for callee-save pushes, reserved outgoing call space, and temporary copy-cycle stack movement.
- Preserved the hardened benchmark methodology and rejected broader ABI precoloring / incomplete frameless-entry experiments that failed semantic or ABI gates.

### Validation

- Full strict suite: 66/66 passing.
- Corrected randomized Forge-vs-LLVM broad performance gates pass at both `-O2` and `-O3`.

## 2.0.0 - 2026-07-29

### Compiler and optimizer

- Added bounded scalar-cleanup fixpoint iteration at `-O2` and `-O3`.
- Added loop-capable mem2reg, cross-block scalar promotion, global load forwarding, and local/global alias-aware dead-store elimination.
- Added generalized scalar-evolution reduction, conservative constant-trip unrolling, LICM hardening, merge-parameter simplification, and SSA diamond if-conversion.
- Added dominance-aware and commutative CSE, predicate canonicalization, address-expression CSE, extended algebraic identities, and power-of-two unsigned division/remainder strength reduction.
- Added broad x86 multiplication strength reduction for powers of two and scaled `LEA` families.
- Added generic runtime counted-loop unrolling with scalar cleanup, control-only induction elimination, whole-loop LICM, and CFG block fusion so optimized loops are exposed without benchmark-specific recognition.

### Backend and register allocation

- Expanded integer allocation to nine registers with ABI-safe entry capture and selective callee-saved use.
- Added cycle-correct shared parallel-copy scheduling for SSA edges, function entry, and outgoing integer/XMM call arguments.
- Added loop-edge affinity, destructive induction coalescing, direct physical copies, `xchg` swap lowering, hot-loop layout, and arithmetic-flags branch fusion.
- Added direct arithmetic, compare, `TEST`, `CMOV`, immediate, stack-memory, and return-value lowering improvements.
- Added frameless call-containing functions when no frame-resident data is required.
- Added pointer-load arithmetic folding into x86 memory operands, prologue-reserved outgoing call areas, broader in-place immediate arithmetic, and less conservative incoming-register capture.
- Added safe leaf-only use of additional caller-saved integer registers and improved constant rematerialization to reduce spills and frame traffic under pressure.

### Calls and floating point

- Removed blanket integer and floating argument snapshots in favor of cycle-safe ABI placement.
- Kept floating call arguments register-resident when they do not cross calls.
- Added direct call-return forwarding and call-result forwarding into arithmetic and subsequent calls for integer and floating values.
- Added register-resident floating entry arguments and direct XMM constant materialization.
- Added conservative direct tail-call lowering for register-only direct calls and delayed floating rematerialization so values are rebuilt near their real post-call use instead of spanning another call.

### Correctness, validation, and release engineering

- Hardened the broad Forge/LLVM benchmark against benchmark-specific compiler symbol checks, fixed-order timing bias, read-only-memory store-forwarding artifacts, and signed-overflow undefined behavior in recurrence references.
- Expanded benchmark semantic validation from a handful of fixed examples to deterministic randomized differential checks across all workload families.
- Fixed LICM terminator corruption, multi-register parallel-copy rotation, destructive recurrence coalescing, comparison-CSE typing, and XMM entry hazards.
- Fixed Clang/MSVC Windows test-build portability by including `<algorithm>` explicitly where standard algorithms are used.
- Expanded differential performance coverage to 16 independent Forge-versus-LLVM workload families with semantic checks, code-size accounting, and unchanged per-kernel gates.
- Added strict 66-test release validation, sanitizer-safe gates, deterministic ELF/COFF coverage, installed C/C++ consumer gates, and package checksums.
- Unified production driver commands: `forge inspect`, `forge explain`, and `forge doctor`.
- Finalized native aggregate ABI parameters and returns for System V AMD64 and Windows x64.

## 1.10.0 - 2026-07-27

- Added executable native aggregate return lowering for explicit C, System V AMD64, and Windows x64 calling conventions.
- Added INTEGER returns through RAX/RDX and SSE returns through XMM0/XMM1, including mixed INTEGER/SSE System V aggregates.
- Added caller-side reconstruction of register-returned aggregates into aligned Forge aggregate storage.
- Preserved hidden result-buffer lowering for ABI-indirect and larger aggregate returns.
- Added bidirectional native C++ interoperability coverage for integer and mixed floating-point aggregate returns.
- Corrected SSE pointer encoding for extended x86-64 base registers used by aggregate return reconstruction.

## 1.9.0 - 2026-07-27

- Added executable native by-value aggregate parameter lowering for System V AMD64 and Windows x64.
- Small register-classified aggregate parameters are exploded into ABI integer/SSE pieces at call sites and reconstructed in callee-local storage.
- Large and Windows-indirect aggregates preserve the existing pointer-based ABI path.
- Added bidirectional native C++ interoperability coverage: native-to-Forge and Forge-to-native by-value struct calls.
- Preserved hidden-result-buffer aggregate returns; direct aggregate return registers remain the final ABI-lowering milestone.

## 1.8.0 - 2026-07-27

- Added segmented-interference-aware global copy affinity coalescing across block boundaries and liveness holes.
- Added recovery of copy destinations that were unnecessarily stack-backed when the source physical register is globally safe.
- Added allocator metrics for global copy affinities and recovered copy spills.

## 1.7.0 - 2026-07-27

- Added critical-edge live-range splitting for call paths that converge with other predecessors.
- Added explicit split-edge blocks with post-call reloads.
- Added SSA merge repair through machine block parameters and predecessor edge arguments.
- Added allocator metrics for critical-edge split values, edge blocks, and merge parameters.
- Added strict regression coverage for mixed call/non-call control-flow convergence.

## 1.6.0 - 2026-07-27

- Added conservative CFG-wide call-boundary splitting into single-predecessor continuation blocks.
- Added cross-block transition accounting while preserving critical-edge safety.

## 1.5.0 - 2026-07-27

### Added

- True transition-based live-range splitting around calls.
- Explicit register-to-stack stores before high-pressure call boundaries.
- Explicit stack-to-register reloads into new post-call virtual registers.
- Independent allocation of pre-call and post-call live-range pieces.
- Split-transition statistics in `forge-codegen --stats`.
- A regression proving five call-crossing values are reduced to two callee-saved intervals with three balanced store/reload transitions.

### Changed

- Floating values live across calls are split when their remaining uses stay in the same block.
- Integer values beyond the two available callee-saved allocation registers are split conservatively at call boundaries.
- Split slots are reserved in the function-local frame and participate in normal frame alignment.

### Safety

- The first transition splitter is deliberately limited to same-block post-call uses. Values live through CFG successors remain on the existing conservative callee-saved/spill path until edge-copy splitting is introduced.

All notable changes to Forge are documented here. Forge follows [Semantic Versioning](https://semver.org/).

## 1.4.0 - 2026-07-27

### Added

- Segmented machine live intervals that retain disjoint per-block liveness regions.
- Exact interference-edge analysis derived from segmented liveness.
- Hole-aware register recovery for mutually exclusive CFG paths.
- Allocator statistics for segmented intervals, live-range holes, interference edges, and recovered registers.
- A permanent interleaved-branch regression proving false bounding-range spills are recovered.

### Changed

- Register-pressure measurements now use real liveness segments rather than one start/end bounding interval.
- Spilled values are reconsidered after linear scan and may reuse a physical register when no segmented interference exists.

## 1.3.0 - 2026-07-27

### Added

- Conservative pointer-origin and alias analysis for stack allocations, globals, pointer arguments, copies, and constant pointer offsets.
- Public natural-loop discovery with canonical latch, header, block-set, and unique-preheader information.
- Alias-aware memory forwarding that removes redundant loads and forwards stored values across unrelated memory writes.
- Safe loop-invariant code motion for non-trapping operations in canonical loop headers.
- `memory-forwarding` and `licm` passes in the standard `-O2`/`-O3` pipelines.
- Regression coverage for overlapping ranges, disjoint stack objects, call invalidation, redundant loads, and loop semantics.

### Changed

- `-O2` now includes alias-aware memory forwarding.
- `-O3` now performs loop-invariant code motion before CSE and memory forwarding.

## 1.2.0 - 2026-07-27

### Native ABI support

- Added public System V AMD64 and Windows x64 aggregate ABI classification.
- Added integer, SSE, memory, and indirect aggregate classes.
- Added per-function ABI summaries covering register use, stack bytes, variadic state, and aggregate parameters.
- Added function calling-convention metadata for platform, C, System V, Windows x64, and fast conventions.
- Added variadic, internal, weak, and hidden symbol metadata to textual and binary Forge IR.
- Added C API v10 setters and deterministic function ABI JSON.

### Native libraries

- Added deterministic static archive generation for ELF64 and COFF AMD64 objects.
- Added a real archive symbol index accepted by native linkers.
- Added GNU long-member-name support and deterministic archive metadata.
- Added `forge archive create` for `.a` and compatible `.lib` archives.
- Added `forge link-shared` for host-toolchain shared-library linking.
- Added native static-library and shared-library link-and-run release gates.

### Quality

- Added ABI classification, signature metadata, binary round-trip, archive determinism, corrupt-object, and native library tests.
- Increased the strict release matrix to 63 tests.
- Preserved repository-wide Apache-2.0 licensing headers and the source hygiene gate.

### Current boundary

Forge exposes native ABI classification for frontend lowering decisions. Named aggregates in the current machine-code pipeline remain represented through pointer or hidden-result-storage lowering; register-classified by-value aggregate code generation remains planned work.

## 1.1.0 - 2026-07-27

### Frontend Development Kit

- Added reusable source management, structured diagnostics, nested scopes, symbols, semantic declarations, and safe control-flow builders.
- Added `forge new-language` project scaffolding.
- Added repository-wide Apache-2.0 SPDX headers with Copyright 2026 Mario Vinciguerra.
- Added a repository hygiene gate that rejects unlicensed maintained source and build files.
- Fixed IRBuilder insertion-point lifetime by storing stable block handles across block-vector growth.
- Added MiniLang, a complete educational frontend showing source text through lexer, parser, AST, semantic lowering, Forge IR verification, interpretation, source maps, and x86-64 JIT execution.

## 1.0.0 - 2026-07-27

- Established the stable public compiler-core, frontend SDK, interpreter, x86-64 backend, object writers, incremental build pipeline, and release-quality contract.
- Added deterministic ELF64 and COFF AMD64 output, JIT/interpreter differential testing, cache-aware native linking, installed-package consumers, fuzz-smoke tests, and professional open-source documentation.

## Pre-1.0 development

Before 1.0, Forge developed its verified IR, interpreter, JIT, backend optimizations, object writers, frontend SDK, and incremental build system through internal milestone releases. Those milestones are intentionally consolidated here so the public changelog begins with the stable 1.0 API and support contract.
