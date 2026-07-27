# Forge architecture

Forge 2.8 is organized as a verified, deterministic compiler pipeline. Each layer owns a narrow contract and rejects malformed input before passing it downstream.

## Pipeline

1. **IR front end** — lexical analysis, parsing, canonical printing, SSA data structures, and verification.
2. **Analysis and transforms** — deterministic optimization pipelines with explicit change statistics and optional verification after every pass.
3. **Machine lowering** — target-neutral control-flow and operation lowering.
4. **Machine optimization** — CFG cleanup, copy propagation, liveness, global dead-code elimination, instruction selection preparation, and virtual-register compaction.
5. **Register allocation and frame layout** — call-aware linear scan, weighted spill choices, spill-slot reuse, and final frame computation.
6. **x86-64 encoding** — ABI marshaling, instruction encoding, branch relaxation/layout, spill caching, and relocation production.
7. **Execution and artifacts** — interpreter, W^X JIT, ELF64 writer, and COFF AMD64 writer.

Control-flow values are transferred through typed block arguments rather than PHI operations. The verifier computes reachability and dominators, checks definition ordering, validates successor arity and types, and rejects malformed terminator placement.

No global pass registry or hidden mutable compiler state is used. Passes operate on explicit modules/functions and return deterministic statistics.

## Verification boundaries

Forge verifies data at the following boundaries:

- Parsed textual IR
- Decoded binary IR
- Optimized IR
- Lowered machine IR
- Machine IR after structural transformations
- Inputs to register allocation and encoding

The interpreter is the semantic reference model. Native behavior is compared against it through differential tests using identical typed arguments and bit-exact return comparisons.

## Binary IR

Binary IR is a transport/cache format, not a dump of C++ object layouts. Fields are encoded explicitly in little-endian form and decoded into owned IR objects. Readers reject incompatible versions and run the normal verifier before returning a usable module.

Compatibility follows a major/minor policy:

- Major revisions may change semantics or structural encoding.
- Minor revisions may add backward-compatible features.
- Readers reject payloads requiring a newer unsupported minor revision.

## Module data and ownership

Modules own deterministic ordered declarations for mutable globals and read-only constants. `global.address` produces an opaque pointer consumed by normal load/store/pointer-offset operations. The interpreter and JIT preserve declaration lifetime and read-only provenance.

Aggregate ownership and borrowing are represented explicitly in signatures and operations. External owned aggregate parameters are lowered to caller-local copies where supported. Native by-value aggregate ABI classification is not yet implemented and remains an explicit project boundary.

## Machine optimization contract

Machine transformations run before allocation and include:

- Transitive copy propagation
- Zero-offset and redundant-cast elimination
- Integer extension-chain cleanup
- Single-use constant folding and rematerialization
- Compare/branch fusion
- Stack memory-source folding
- Jump threading and unreachable-block removal
- Fallthrough-aware block ordering
- Fixed-point global dead-code elimination
- Dense virtual-register renumbering

Shared liveness supplies block use/def, live-in/live-out, instruction live-after, CFG successors, and fixed-point metrics to both optimization and allocation.

## Determinism

Stable declaration order, block order, virtual-register numbering, pass order, symbol order, section order, and relocation order are intentional invariants. CI compiles representative modules twice and requires byte-identical canonical output and object files.

## Security posture

Untrusted boundaries include text IR, binary IR, external symbol bindings, callback signatures, and generated object metadata. Release gates include strict verification, fuzz-smoke targets, ASan/UBSan, leak checks, deterministic output, native linking, and JIT/interpreter differential execution.
