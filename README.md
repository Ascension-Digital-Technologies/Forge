# Forge

**A verified, deterministic compiler core and x86-64 backend for building language frontends.**

Forge provides a compact SSA intermediate representation, verification and optimization pipelines, an interpreter, an x86-64 JIT/backend, deterministic ELF64 and COFF object emission, and a frontend SDK for C++ and C. It is designed for language implementers who want a smaller, auditable compiler foundation without giving up strict correctness boundaries, native code generation, or incremental builds.

> **Release:** 4.0.0  
> **License:** Apache-2.0  
> **Public C API:** v9

## Why Forge

- **Verified at every boundary.** Parsed IR, decoded binary IR, optimized IR, lowered machine IR, and machine transformations are validated before execution or object generation.
- **Deterministic by design.** Canonical IR, optimization output, incremental manifests, ELF objects, and COFF objects are covered by reproducibility tests.
- **Frontend-friendly.** A typed C++ builder, stable handles, source ranges, structured diagnostics, metadata, source maps, and an opaque C API make Forge practical from C++, C, Rust, Zig, Dash, Python extensions, and other FFI-capable languages.
- **Native and incremental.** Forge can cache encoded functions, rebuild dependency-affected callers only, assemble deterministic native objects, and cache final linked executables.
- **Reference-model testing.** The interpreter is used as the semantic reference for JIT differential testing.

## Capabilities

### IR and frontend SDK

- Typed SSA IR with block arguments, globals, calls, callbacks, aggregates, and ownership-aware verification
- Canonical textual IR and versioned little-endian binary IR
- Typed `forge::ir::Opcode` values and ergonomic `IRBuilder`
- Automatic SSA naming and stable index-backed function/block handles
- Full source ranges and structured diagnostics
- Namespaced module metadata and per-operation frontend attributes
- Deterministic JSON source maps
- Opaque, versioned C API in `<forge-c/forge.h>`

### Optimization and execution

- Verified optimization levels: `-O0`, `-O1`, `-O2`, `-O3`, `-Os`, and `-Oz`
- Shared machine liveness, global dead-code elimination, CFG cleanup, copy propagation, and block layout
- Interpreter, x86-64 JIT, and differential execution mode
- Per-pass statistics and timing

### x86-64 backend

- System V AMD64 and Windows x64 scalar/pointer calling conventions
- Integer and floating-point code generation
- Call-aware register allocation and spill-slot reuse
- Spill caching, dead-store elimination, rematerialization, immediate selection, memory-operand folding, compare/branch fusion, compact branches, and leaf-frame omission
- Deterministic ELF64 and COFF AMD64 relocatable objects

### Incremental builds

- Semantic and frontend SHA-256 fingerprints
- Per-function change detection and dependency invalidation
- Deterministic parallel build scheduling
- Atomic artifact cache storage
- Cached encoded-function artifacts with unresolved fixups
- Deterministic incremental ELF/COFF assembly
- Cache-aware native linking and final executable reuse

## Requirements

- CMake 3.21 or newer
- A C++20 compiler
- Ninja for the checked-in presets
- An x86-64 host for JIT execution tests

Linux and Windows are exercised in CI. Native ELF link-and-run validation is performed on Linux; COFF object generation is validated on supported CI platforms.

## Build

Run the complete strict release gate:

```sh
./scripts/release-gate.sh
```

PowerShell:

```powershell
.\scripts\release-gate.ps1
```

Manual preset workflow:

```sh
cmake --preset release-strict
cmake --build --preset release-strict
ctest --preset release-strict
```

Run the complete ASan/UBSan gate on supported Unix toolchains:

```sh
./scripts/sanitizer-gate.sh
```

## Command-line tools

| Tool | Purpose |
|---|---|
| `forge` | Verify, optimize, and compile Forge IR |
| `forge-as` | Assemble textual IR into binary IR |
| `forge-dis` | Disassemble binary IR into canonical text |
| `forge-opt` | Run the verified optimization pipeline |
| `forge-codegen` | Inspect lowering, allocation, encoding, and metrics |
| `forge-run` | Execute through the interpreter, JIT, or differential mode |

```sh
forge verify examples/native-i64.fir
forge-opt examples/optimization.fir -O3 --stats --pass-timing
forge compile examples/native-i64.fir -O2 --format=elf -o native.o
forge-run --engine=compare examples/interpreter.fir factorial 10
```

## Build a frontend

### C++

```cpp
#include <forge/ir/builder.hpp>
#include <forge/ir/verifier.hpp>

forge::ir::Context context;
auto& module = context.create_module("example");
forge::ir::IRBuilder builder(context, module);

std::vector<forge::ir::ValueDecl> parameters{
    {"%left", forge::ir::i64_type()},
    {"%right", forge::ir::i64_type()},
};

auto function = builder.create_function_handle(
    "add", forge::ir::i64_type(), parameters);
auto entry = builder.create_block_handle(function, "entry");

builder.position_at_end(entry);
builder.set_source_range("main.ds", 1, 1, 1, 15);
auto result = builder.create_add(
    forge::ir::i64_type(), "%left", "%right");
builder.create_return(result);

const auto diagnostics = builder.verify();
```

See [`docs/building-a-language.md`](docs/building-a-language.md), [`examples/frontend/tiny_frontend.cpp`](examples/frontend/tiny_frontend.cpp), and the standalone template in [`examples/frontend/template/`](examples/frontend/template/).

### C and other FFI languages

The opaque C API is installed as:

```c
#include <forge-c/forge.h>
```

It supports module/function/block construction, CFG and memory operations, diagnostics, source ranges, metadata, source maps, incremental manifests, build plans, and dependency-aware schedules. The current public API version is `FORGE_C_API_VERSION == 9`.

## Incremental native pipeline

```cpp
#include <forge/ir/artifact_cache.hpp>
#include <forge/ir/dependency_build.hpp>
#include <forge/object/incremental.hpp>
#include <forge/object/native_link.hpp>
```

A frontend can:

1. Fingerprint the current and previous modules.
2. Build a dependency-aware selective rebuild plan.
3. Compile only changed or invalidated functions.
4. Reuse encoded function artifacts from the cache.
5. Assemble deterministic ELF64 or COFF objects.
6. Link only when the final binary cache key misses.

Production link integrations should set a stable toolchain identity, such as `clang-22.1.0+lld-22.1.0`, so linker upgrades invalidate incompatible final binaries.

## Install and consume

```sh
cmake --preset release-strict
cmake --build --preset release-strict
cmake --install build/release-strict --prefix "$PWD/_install"
```

Consumer project:

```cmake
find_package(Forge 4.0 CONFIG REQUIRED)
target_link_libraries(my_compiler PRIVATE Forge::forge)
```

The release matrix installs Forge into an isolated prefix and builds and runs separate C and C++ consumers against the installed package.

## Repository layout

```text
include/forge/       Public C++ API
include/forge-c/     Stable opaque C API
src/                 Compiler implementation
tools/               Command-line tools
tests/               Unit, differential, ABI, object, fuzz, and quality tests
examples/            Verified IR examples and frontend samples
docs/                Architecture, backend, frontend, and release documentation
cmake/               Installed CMake package support
scripts/             Reproducible release and sanitizer gates
.github/workflows/   CI and release packaging
```

## Release quality

Every release must preserve:

- Warnings-as-errors builds
- The complete CTest matrix
- ASan/UBSan and leak-detection cleanliness
- Parser and binary fuzz-smoke coverage
- Verification of every checked-in `.fir` example
- Deterministic optimization output and native objects
- Native link-and-execute validation
- Installed C and C++ package-consumer compatibility
- Exact backend code-quality ceilings
- Repository hygiene checks

See [`docs/release-readiness.md`](docs/release-readiness.md) for the complete contract.

## Supported boundaries

Forge 4.0.0 is production-ready for its documented compiler-core and x86-64 scalar/pointer feature set. The following remain intentionally outside the supported surface:

- Native by-value aggregate ABI classification
- True variadic function definitions
- Unwind and debug metadata
- Segmented live-range register allocation
- Architectures other than x86-64

These limitations are explicit and are not represented as completed features.

## Documentation

- [Architecture](docs/architecture.md)
- [Backend](docs/backend.md)
- [Building a language](docs/building-a-language.md)
- [Release readiness](docs/release-readiness.md)
- [Contributing](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Changelog](CHANGELOG.md)

## License

Forge is licensed under the [Apache License 2.0](LICENSE).
