<div align="center">

# Forge

**A verified compiler infrastructure toolkit for building native language frontends.**

[![Release](https://img.shields.io/badge/release-1.0.0-2563eb)](CHANGELOG.md)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599c)](CMakeLists.txt)
[![License](https://img.shields.io/badge/license-Apache--2.0-16a34a)](LICENSE)
[![C API](https://img.shields.io/badge/C%20API-v9-7c3aed)](include/forge-c/forge.h)
[![Target](https://img.shields.io/badge/target-x86--64-334155)](#platform-support)

Forge provides typed SSA IR, verification, optimization pipelines, an interpreter, an x86-64 JIT and native backend, deterministic ELF/COFF output, and frontend SDKs for C++ and C.

[Quick start](#quick-start) · [Frontend guide](docs/building-a-language.md) · [Architecture](docs/architecture.md) · [Contributing](CONTRIBUTING.md)

</div>

---

## Overview

Forge is a compact compiler core intended for language implementers who want a native backend without adopting a very large compiler framework. A frontend performs parsing, semantic analysis, and type checking, then lowers into verified Forge IR.

```text
Source language
      │
      ▼
Parser · resolver · type checker
      │
      ▼
Forge C++ SDK or C API
      │
      ▼
Verified SSA IR
      ├──────────────► Interpreter
      ▼
Optimization pipeline
      ▼
Machine IR · allocation · x86-64 encoding
      ├──────────────► JIT
      └──────────────► ELF64 / COFF object files
```

### Highlights

| Area | Capabilities |
|---|---|
| **Frontend SDK** | Typed builder, stable handles, source ranges, diagnostics, metadata, source maps, opaque C API |
| **IR** | Typed SSA, block arguments, globals, calls, aggregates, canonical text, binary serialization |
| **Optimization** | `-O0` through `-O3`, `-Os`, `-Oz`, liveness, DCE, CFG cleanup, copy propagation, pass reports |
| **Execution** | Reference interpreter, x86-64 JIT, runtime bindings, interpreter/JIT differential tests |
| **Native output** | System V and Windows x64 lowering, deterministic ELF64 and COFF AMD64 objects |
| **Incremental builds** | Fingerprints, dependency invalidation, parallel scheduling, cached functions, object and executable caching |

## Quick start

### Requirements

- CMake 3.21 or newer
- A C++20 compiler
- Ninja when using the checked-in presets
- An x86-64 host for JIT tests

### Build

```sh
cmake --preset release-strict
cmake --build --preset release-strict
ctest --preset release-strict
```

Windows PowerShell:

```powershell
.\scripts\release-gate.ps1
```

Unix release gate:

```sh
./scripts/release-gate.sh
```

Sanitizer gate on supported Unix toolchains:

```sh
./scripts/sanitizer-gate.sh
```

### Command-line examples

```sh
# Verify textual IR
forge verify examples/native-i64.fir

# Optimize and inspect pass statistics
forge-opt examples/optimization.fir -O3 --stats --pass-timing

# Emit an ELF object
forge compile examples/native-i64.fir -O2 --format=elf -o native.o

# Compare interpreter and JIT results
forge-run --engine=compare examples/interpreter.fir factorial 10
```

## Tools

| Tool | Purpose |
|---|---|
| `forge` | Verify, optimize, and compile Forge IR |
| `forge-as` | Assemble textual IR into binary IR |
| `forge-dis` | Disassemble binary IR into canonical text |
| `forge-opt` | Run optimization pipelines and report pass statistics |
| `forge-codegen` | Inspect lowering, allocation, encoding, and backend metrics |
| `forge-run` | Execute through the interpreter, JIT, or differential engine |

Optimization levels:

| Level | Intent |
|---|---|
| `-O0` | Minimal transformation and maximum IR fidelity |
| `-O1` | Low-cost cleanup |
| `-O2` | Standard production optimization |
| `-O3` | Aggressive optimization |
| `-Os` | Optimize for size |
| `-Oz` | Minimize code size |

See [docs/cli.md](docs/cli.md) for command details.

## Build a frontend

### C++

```cpp
#include <forge/ir/builder.hpp>

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
builder.set_source_range("main.lang", 1, 1, 1, 15);
auto result = builder.create_add(
    forge::ir::i64_type(), "%left", "%right");
builder.create_return(result);

const auto diagnostics = builder.verify();
```

### C and FFI

```c
#include <forge-c/forge.h>

#if FORGE_C_API_VERSION != 9
#error "Unsupported Forge C API version"
#endif
```

The opaque C API is suitable for Rust, Zig, Go, C#, Python extensions, Dash, and other languages with C FFI support.

Start with:

- [Building a language with Forge](docs/building-a-language.md)
- [Tiny frontend example](examples/frontend/tiny_frontend.cpp)
- [Standalone frontend template](examples/frontend/template/)

## Native backend

The x86-64 backend includes:

- System V AMD64 and Windows x64 scalar/pointer calling conventions
- Integer and scalar floating-point lowering
- Call-aware register allocation and reusable spill slots
- Spill caching, dead-store elimination, and rematerialization
- Immediate, memory-source, and address-mode instruction selection
- Compare/branch fusion and fallthrough-aware block layout
- Short branch relaxation and frameless leaf functions
- Deterministic ELF64 and COFF AMD64 object emission

The interpreter is the semantic reference implementation. Native behavior is checked through differential tests.

## Incremental native builds

Forge can rebuild dependency-affected functions only, reuse encoded native artifacts, assemble deterministic objects, and restore final executables from cache.

```text
Previous snapshot ─┐
                   ├─► dependency-aware build plan
Current module ────┘              │
                                  ▼
                        parallel function builds
                                  │
                     ┌────────────┴────────────┐
                     ▼                         ▼
               artifact cache            cache hits
                     └────────────┬────────────┘
                                  ▼
                     deterministic ELF / COFF
                                  ▼
                       cache-aware native link
```

Public headers:

```cpp
#include <forge/ir/artifact_cache.hpp>
#include <forge/ir/dependency_build.hpp>
#include <forge/object/incremental.hpp>
#include <forge/object/native_link.hpp>
```

## Install and consume

```sh
cmake --preset release-strict
cmake --build --preset release-strict
cmake --install build/release-strict --prefix "$PWD/_install"
```

Consumer project:

```cmake
find_package(Forge 1.0 CONFIG REQUIRED)
target_link_libraries(my_compiler PRIVATE Forge::forge)
```

The release matrix installs Forge into an isolated prefix and builds independent C and C++ consumers against the installed package.

## Platform support

| Capability | Linux x86-64 | Windows x86-64 |
|---|:---:|:---:|
| Compiler and SDK | ✅ | ✅ |
| Interpreter | ✅ | ✅ |
| Native JIT | ✅ | ✅ |
| System V ABI | ✅ | — |
| Windows x64 ABI | — | ✅ |
| ELF64 objects | ✅ | Generated and validated |
| COFF AMD64 objects | Generated and validated | ✅ |
| ASan + UBSan release gate | ✅ | Toolchain-dependent |

## Project status and boundaries

Forge 1.0.0 is the first stable public release of the documented compiler-core and x86-64 scalar/pointer feature set.

The following are not currently part of the supported surface:

- Native by-value aggregate ABI classification
- True variadic function definitions
- Unwind and debug metadata
- Segmented live-range register allocation
- Architectures other than x86-64

See [docs/release-readiness.md](docs/release-readiness.md) for the release contract and [docs/roadmap.md](docs/roadmap.md) for planned work.

## Repository layout

```text
include/forge/       Public C++ SDK
include/forge-c/     Stable opaque C API
src/                 Compiler implementation
tools/               Command-line tools
tests/               Unit, differential, ABI, object, fuzz, and quality tests
examples/            IR examples and frontend samples
docs/                Architecture, API, backend, and release documentation
cmake/               Installed CMake package support
scripts/             Reproducible release and sanitizer gates
.github/workflows/   CI and release packaging
```

## Documentation

| Document | Purpose |
|---|---|
| [Architecture](docs/architecture.md) | Pipeline, subsystem responsibilities, and invariants |
| [IR reference](docs/ir-reference.md) | Core IR concepts and verification rules |
| [Backend](docs/backend.md) | Machine IR, allocation, ABI, encoding, and objects |
| [CLI reference](docs/cli.md) | Command-line tools and optimization levels |
| [Building a language](docs/building-a-language.md) | C++ SDK and C API integration |
| [Release readiness](docs/release-readiness.md) | Supported production contract and release gates |
| [Roadmap](docs/roadmap.md) | Planned compiler work |
| [Contributing](CONTRIBUTING.md) | Development workflow and contribution standards |
| [Security](SECURITY.md) | Vulnerability reporting policy |
| [Code of Conduct](CODE_OF_CONDUCT.md) | Community participation expectations |

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. Changes must preserve deterministic output, verification boundaries, and the interpreter/JIT differential contract.

## License

Forge is licensed under the [Apache License 2.0](LICENSE).
