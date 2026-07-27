# Building a language with Forge

A Forge frontend owns parsing, name resolution, type checking, and language-specific semantics. It lowers valid programs into Forge IR through the C++ SDK or opaque C API.

## Recommended frontend pipeline

1. Parse source into an AST.
2. Resolve names and types.
3. Create Forge functions and blocks.
4. Lower expressions and control flow through `IRBuilder`.
5. Attach source ranges and optional frontend metadata.
6. Verify the module.
7. Run an optimization pipeline.
8. Interpret, JIT, or emit an object file.

## C++ builder

Prefer stable `FunctionHandle` and `BlockHandle` values rather than retaining references into module-owned containers.

```cpp
forge::ir::Context context;
auto& module = context.create_module("hello");
forge::ir::IRBuilder builder(context, module);

std::vector<forge::ir::ValueDecl> parameters{
    {"%a", forge::ir::i64_type()},
    {"%b", forge::ir::i64_type()},
};

auto function = builder.create_function_handle(
    "add", forge::ir::i64_type(), parameters);
auto entry = builder.create_block_handle(function, "entry");

builder.position_at_end(entry);
builder.set_source_range("program.lang", 1, 1, 1, 12);
auto result = builder.create_add(
    forge::ir::i64_type(), "%a", "%b");
builder.create_return(result);

const auto diagnostics = builder.verify();
```

The builder manages insertion points, unique SSA names, source spans, duplicate symbol checks, and terminator safety.

## C API

Include `<forge-c/forge.h>`. The API uses opaque, stable handles and explicit destroy functions.

```c
#if FORGE_C_API_VERSION != 9
#error "Unsupported Forge C API"
#endif
```

Use the two-call buffer pattern for canonical IR, source maps, fingerprints, manifests, and build-plan JSON: call with a null output buffer to obtain the required size, allocate, then call again.

## Diagnostics and source maps

Operations may carry complete source ranges. Frontends can enumerate verifier diagnostics through C++ or the structured C diagnostic accessors. Namespaced metadata and one-shot operation attributes support AST IDs, semantic type names, debug labels, IDE navigation, and profiling correlation.

Metadata is non-semantic: it does not change verification, optimization, or native code generation.

## Incremental builds

Use incremental snapshots to distinguish semantic changes from frontend-only changes. Build plans classify functions as rebuild, reuse, frontend refresh, or remove. Dependency analysis propagates semantic invalidation to transitive callers.

Public headers include:

```cpp
#include <forge/ir/incremental.hpp>
#include <forge/ir/artifact_cache.hpp>
#include <forge/ir/build_driver.hpp>
#include <forge/ir/dependency_build.hpp>
#include <forge/object/incremental.hpp>
#include <forge/object/native_link.hpp>
```

## API stability

- `forge::ir::IRBuilder`, stable handles, public IR helpers, and `forge-c/forge.h` are frontend-facing.
- Public incremental and object orchestration headers are supported SDK surfaces.
- `forge::machine`, target encoders, and internal writer implementation details are unstable.

## Examples

- `examples/frontend/tiny_frontend.cpp`
- `examples/frontend/template/`
- `tests/frontend_builder.cpp`
- `tests/c_api.c`
