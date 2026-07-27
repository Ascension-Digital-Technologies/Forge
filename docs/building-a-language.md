# Building a Language with Forge

Forge frontends lower parsed and type-checked source programs into Forge semantic IR. Frontends should depend on `forge::ir`, `forge::pass`, and the stable C API; machine IR and target encoders are internal implementation layers.

## C++ builder

```cpp
forge::ir::Context context;
auto& module = context.create_module("hello");
forge::ir::IRBuilder builder(context, module);
std::vector<forge::ir::ValueDecl> args{
    {"%a", {forge::ir::TypeKind::i64}},
    {"%b", {forge::ir::TypeKind::i64}},
};
auto& function = builder.create_function("add", {forge::ir::TypeKind::i64}, args);
auto& entry = builder.create_block(function, "entry");
builder.position_at_end(entry);
builder.set_source_location({"program.lang", 1, 1});
auto result = builder.create_add({forge::ir::TypeKind::i64}, "%a", "%b");
builder.create_return(result);
```

The builder creates unique SSA names and attaches optional source locations. Always call `verify_module` before optimization or code generation.

## C API

Include `<forge-c/forge.h>`. The API uses opaque handles and is suitable for C-compatible FFI layers. The module is owned by its context; handle objects must be released with their matching destroy function.

## Recommended frontend pipeline

1. Parse source into an AST.
2. Resolve names and types in the frontend.
3. Create Forge functions and blocks.
4. Lower expressions and control flow through `IRBuilder`.
5. Attach source locations before emitting operations.
6. Verify the module.
7. Run a Forge optimization pipeline.
8. JIT, interpret, or emit ELF/COFF objects.

## Stability boundary

- `forge::ir::IRBuilder`, `forge::ir::Opcode`, and `forge-c/forge.h`: frontend-facing.
- `forge::ir` module structures: supported but lower-level.
- `forge::machine`, `forge::codegen`, and object-writer internals: unstable implementation details.

## Frontend SDK v2 surface

Forge 3.0 adds production-oriented construction helpers for stack allocations, loads, stores, pointer offsets, calls, block parameters, branches, and unreachable terminators. The C API uses stable index-backed handles: adding functions or blocks does not invalidate previously returned handles.

Use `forge_module_print(module, nullptr, 0)` to query the required canonical-text buffer size, allocate that many bytes, and call it again to serialize the module. `forge_last_error()` returns the current thread-local error; `forge_clear_error()` resets it.

A standalone starter project is available in `examples/frontend/template/`.


## Builder safety and diagnostics

Forge 3.1 rejects duplicate function/block names and instructions appended after a terminator.
Use `IRBuilder::verify()` in C++, or call `forge_module_verify` followed by the structured
`forge_module_diagnostic_*` accessors from C. The C API version is 3.


## Stable handles and source ranges

For frontends that grow modules incrementally, prefer `FunctionHandle` and `BlockHandle` over retaining references into module-owned vectors. Handles remain valid when later functions and blocks are appended. Use `set_source_range` to associate the complete frontend token span with each generated operation.


## Frontend metadata and source maps

Forge 3.3 lets frontends attach namespaced, non-semantic metadata without creating backend-only opcodes.
Use module metadata for language/toolchain identity and `set_next_attribute` for the next emitted operation.
Attributes are retained through direct SDK construction and exported through `build_source_map_json`.
Recommended keys include `frontend.ast_id`, `frontend.semantic_type`, and `frontend.debug_name`.

```cpp
builder.set_module_metadata("frontend.language", "Dash");
builder.set_next_attribute("frontend.ast_id", "1042");
builder.set_source_range("main.ds", 8, 5, 8, 19);
auto value = builder.create_add(i64_type(), left, right);
auto json = build_source_map_json(module);
```

These annotations do not affect verification or native code generation. Frontend-specific operations must be legalized into Forge core IR before verification and compilation.


## Incremental compilation and frontend caches

Forge 3.4 exposes deterministic SHA-256 fingerprints that separate semantic IR from frontend-only state.

```cpp
auto snapshot = forge::ir::build_incremental_snapshot(module);
auto manifest = forge::ir::build_incremental_manifest_json(snapshot);
auto key = forge::ir::build_cache_key(module, "dash", "-O2;x86_64");
```

`semantic_fingerprint` excludes source ranges, module metadata, and frontend attributes.
`frontend_fingerprint` includes those values, making it suitable for source maps, IDE data, and diagnostic caches.
Per-function fingerprints are sorted by function name and can be compared with `compare_incremental_snapshots` to identify added, removed, semantically modified, or frontend-only changed functions.

## Incremental artifact caching

Use `forge/ir/incremental.hpp` to snapshot the previous and current modules, then use
`forge/ir/artifact_cache.hpp` to create a deterministic selective build plan:

```cpp
const auto previous = forge::ir::build_incremental_snapshot(old_module);
const auto current = forge::ir::build_incremental_snapshot(new_module);
const auto plan = forge::ir::build_incremental_build_plan(
    previous, current, "my-language", "-O2;x86_64");
```

Each function is classified as `rebuild`, `reuse`, `frontend-refresh`, or `remove`.
The plan contains semantic and frontend cache keys. Store object code, machine code,
source maps, diagnostics, or custom blobs with `forge::ir::ArtifactCache`; writes are
committed atomically and entries are addressed by validated SHA-256 keys.
