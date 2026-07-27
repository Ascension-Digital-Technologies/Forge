# Optimizer and analysis

Forge 1.3 provides reusable analyses and deterministic scalar optimization passes for frontend authors and compiler integrations.

## Core analyses

`forge::analysis::FunctionAnalysisManager` caches and invalidates:

- control-flow graphs and reachability,
- use/definition information,
- dominator trees,
- conservative pointer-origin alias analysis,
- natural-loop information.

### Alias model

Pointers are classified as stack, global, argument, or unknown origins. Constant `ptr.offset` and `field.address` operations retain a precise byte offset. Alias queries return `no_alias`, `may_alias`, or `must_alias`.

Forge proves distinct stack allocations and distinct globals do not alias. Different pointer arguments remain conservative and may alias. Unknown pointer operations also remain conservative.

```cpp
forge::analysis::FunctionAnalysisManager analyses(function);
const auto left = analyses.aliases().location("%left", 8);
const auto right = analyses.aliases().location("%right", 8);
const auto relation = analyses.aliases().alias(left, right);
```

## Memory forwarding

`MemoryForwardingPass` tracks known values within a basic block. It:

- forwards a stored SSA value into a later load from the same exact location,
- removes repeated loads from an unchanged location,
- preserves facts across writes proven not to alias,
- invalidates facts at calls, unknown memory effects, and possible aliases.

## Loop information and LICM

Natural loops are identified from dominator-backed CFG backedges. Each loop records its header, latch, block set, and unique preheader when one exists.

`LoopInvariantCodeMotionPass` hoists non-trapping operations from canonical loop headers when every SSA operand is defined outside the loop or was already proven invariant. It does not hoist loads, calls, division, remainder, or operations with side effects.

## Standard pipelines

- `-O0`: no optimization passes.
- `-O1`: SCCP, algebraic simplification, copy propagation, and DCE.
- `-O2`: adds CSE, alias-aware memory forwarding, and CFG simplification.
- `-O3`: adds LICM and another aggressive scalar cleanup cycle.
- `-Os` and `-Oz`: retain code-size-oriented pipelines.

Pass reports remain available through `forge-opt --stats --pass-timing` and `forge compile --pass-stats`.
