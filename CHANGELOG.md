# Changelog

All notable changes to Forge are documented here. Forge follows [Semantic Versioning](https://semver.org/).

## 1.0.1 - 2026-07-27

### Added

- MiniLang, a complete educational frontend showing source text through lexer, parser, AST, semantic lowering, Forge IR verification, interpretation, source maps, and x86-64 JIT execution.
- A permanent strict-build regression for the complete frontend example.

### Documentation

- Expanded the language-building guide and README to make MiniLang the recommended starting point for frontend authors.
- Documented stable handle usage, frontend-owned semantic checks, source-range propagation, and interpreter/JIT differential validation.

## 1.0.0 — First stable release

Forge 1.0.0 establishes the first stable public release of the compiler core and frontend SDK.

### Compiler infrastructure

- Typed, verified SSA IR with canonical text and binary serialization
- Deterministic optimization pipelines with six optimization levels
- Reference interpreter and x86-64 JIT with differential testing
- x86-64 machine lowering, register allocation, code generation, and ABI support
- Deterministic ELF64 and COFF AMD64 object emission

### Frontend SDK

- Typed C++ IR builder with stable function and block handles
- Source ranges, structured diagnostics, metadata, and source maps
- Opaque C API v9 for FFI integrations
- Incremental fingerprints, manifests, dependency analysis, and build planning

### Incremental native pipeline

- Per-function native artifacts and atomic artifact caching
- Dependency-aware invalidation and deterministic parallel scheduling
- Cached ELF/COFF object assembly
- Cache-aware native linking and final executable reuse

### Release quality

- Apache-2.0 licensing
- Strict warnings-as-errors builds
- ASan, UBSan, leak, fuzz-smoke, deterministic-output, native-link, and installed-package gates
- Professional project documentation and package-consumer examples
- MinGW-safe `windows.h` inclusion under strict `-Werror` builds

## Pre-1.0 development

Before 1.0, Forge developed its verified IR, interpreter, JIT, backend optimizations, object writers, frontend SDK, and incremental build system through internal milestone releases. Those milestones are intentionally consolidated here so the public changelog begins with the stable 1.0 API and support contract.
