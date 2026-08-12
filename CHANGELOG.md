# Changelog

All notable changes to Forge are documented here. Forge follows [Semantic Versioning](https://semver.org/).

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
