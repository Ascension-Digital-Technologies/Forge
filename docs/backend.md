# Forge x86-64 backend

Forge lowers verified SSA IR into target-neutral machine IR before target encoding. The x86-64 backend supports System V AMD64 and Windows x64 scalar/pointer calling conventions, JIT image emission, ELF64 objects, and COFF AMD64 objects.

## Allocation pipeline

1. Number blocks and instructions deterministically.
2. Compute shared block use/def and live-in/live-out sets.
3. Build instruction-level live-after data and live intervals.
4. Apply loop-depth and use-frequency spill weighting.
5. Constrain call-crossing integer values to callee-saved registers or spills.
6. Apply copy, unary, and two-address allocation hints where call-safe.
7. Run linear scan over stable virtual-register order.
8. Color non-overlapping spills into reusable aligned stack slots.
9. Compute the final frame and save only assigned nonvolatile registers.
10. Encode abstract locations and verify all relocation/layout assumptions.

The allocator currently assigns one location to each virtual register for its complete interval. Segmented live ranges and transition copies are not yet part of the allocation model.

## Calling conventions

Call marshaling snapshots every register-bound argument before writing any ABI destination. This prevents cycles and reordered XMM/GPR assignments from clobbering later arguments.

### System V AMD64

- Independent integer and floating register sequences
- Six integer/pointer argument registers
- Eight scalar XMM argument registers
- Overflow arguments on the stack
- Exact call-site stack alignment

### Windows x64

- Position-based first four argument slots
- Integer or XMM register selected by each slot's type
- Arguments after the fourth on the stack
- Mandatory 32-byte shadow space
- Exact alignment padding

Aggregates are currently lowered through pointers or hidden result storage. True native by-value aggregate classification and variadic signatures remain future work.

## Instruction selection and code quality

The machine optimizer and encoder support:

- Integer and floating arithmetic
- Signed/unsigned comparisons and NaN-correct floating predicates
- Compare/branch fusion
- Immediate arithmetic, shifts, comparisons, returns, and memory stores
- Stack memory-source arithmetic and load/return folding
- x86 base-plus-disp32 address modes
- Floating zero idioms
- Constant rematerialization
- Deferred spill-store elimination and store/load forwarding
- Two-entry integer and floating spill caches
- Spill-slot reuse and frame compaction
- Frameless leaf functions
- Fixed-point short conditional/unconditional branch relaxation
- Fallthrough selection and branch inversion

Exact counters and encoded-byte ceilings are enforced by focused CTest quality fixtures.

## JIT memory policy

The JIT allocates read/write memory, copies a fully relocated image, changes the mapping to read/execute, flushes the instruction cache, and owns the mapping for the engine lifetime. Named lookup never transfers memory ownership.

## Object emission

ELF and COFF writers provide deterministic section, symbol, and relocation ordering. ELF objects include `.note.GNU-stack`; COFF raw sections use deterministic alignment. Duplicate definitions are rejected before symbol-table construction. CI verifies byte-for-byte reproducibility and links a generated ELF object into a native executable.

## Platform boundaries

The backend does not yet provide:

- Native by-value aggregate ABI classification
- True variadic function definitions
- Unwind and debug-frame metadata
- Segmented live-range allocation
- Non-x86-64 targets

These are tracked as future backend milestones rather than implied capabilities.
