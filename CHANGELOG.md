# Changelog

All notable changes to Forge are documented here. Forge follows semantic versioning for released source archives and installed package metadata.

## 4.0.0 — Production documentation and release polish

- Reorganized the README around the supported product surface, frontend workflow, native incremental pipeline, installation, and release guarantees.
- Corrected stale C API and release-version references.
- Reconstructed the 3.8.0 and 3.9.0 release history accurately.
- Tightened the production-readiness contract and public limitations.
- Verified all documented commands, package versions, and installed SDK entry points against the release tree.

## 3.9.0 — Cache-aware native linking

- Added native executable linking directly from cached per-function artifacts.
- Added deterministic final-link cache keys including linker identity, arguments, library paths, and libraries.
- Added atomic final-binary cache restoration and executable-permission handling.
- Added native link/run and linker-bypass regression coverage.

## 3.8.0 — Native incremental object assembly

- Added encoded per-function native artifacts with ABI/version validation.
- Added cache-backed deterministic ELF64 and COFF assembly without re-lowering unchanged functions.
- Preserved unresolved call, function-address, and global-address fixups for final assembly.
- Added byte-for-byte monolithic equivalence and native link/run coverage.

## 3.7.0 — Dependency-aware incremental builds

- Added direct-call dependency graphs and transitive caller invalidation.
- Added callee-before-caller parallel build levels and cycle-safe scheduling.
- Added deterministic cached-artifact assembly and custom assembler hooks.
- Added C API v9 dependency schedule export.

## 3.6.0 — Parallel incremental build driver

- Added deterministic worker-shard scheduling for independent function artifacts.
- Added cache-aware parallel execution with rebuild and frontend-refresh callbacks.
- Added exact cache, failure, and artifact-byte summaries.
- Added C API v8 schedule export.

## 3.5.0 — Incremental artifact cache

- Added deterministic per-function artifact keys and selective build plans.
- Added rebuild, reuse, frontend-refresh, and remove decisions.
- Added atomic filesystem-backed artifact storage.
- Added C API v7 build-plan access.

## 3.4.0 — Incremental frontend compilation

- Added semantic and frontend SHA-256 fingerprints.
- Added per-function snapshots and added/removed/modified/frontend-only change detection.
- Added deterministic manifests and configuration-sensitive cache keys.
- Added C API v6 fingerprint, manifest, and cache-key access.

## 3.3.0 — Frontend metadata and source maps

- Added module metadata and per-operation frontend attributes.
- Added deterministic JSON source-map export with full source ranges.
- Added C API v5 metadata and source-map access.

## 3.2.0 — Stable C++ handles and source ranges

- Added index-backed C++ function and block handles.
- Added safe handle resolution and named lookup APIs.
- Added complete operation source ranges.
- Added C API v4 diagnostic-location accessors.

## 3.1.0 — Frontend diagnostics and builder safety

- Added structured C API verifier diagnostics.
- Rejected duplicate builder symbols and instructions after terminators.
- Added builder insertion-state queries and source-located construction errors.

## 3.0.0 — Stable frontend SDK

- Expanded `IRBuilder` with memory, call, copy, pointer-offset, and CFG helpers.
- Promoted the opaque C API to v2 and replaced unstable raw pointers with index-backed handles.
- Added pure-C installed-package validation and a standalone frontend template.

## 2.9.0 — Frontend SDK and Apache-2.0 licensing

- Licensed Forge under Apache-2.0.
- Added typed frontend opcodes, automatic SSA naming, source locations, and the initial opaque C API.
- Added frontend SDK tests, a sample frontend, and the language-authoring guide.

## 2.8.0 — Repository production cleanup

- Reorganized samples under `examples/` and replaced the milestone README with product documentation.
- Added repository policy files, install-consumer validation, CPack packaging, and stronger CI hygiene.

## 2.7.0 — Sanitizer-complete release hardening

- Completed the full ASan/UBSan matrix with leak detection.
- Added reproducible sanitizer gates for Unix and Windows.

## 2.6.0 — Production release gate

- Added optimization levels, shared verified pass pipelines, pass diagnostics, fuzz smoke tests, and full example verification.

## 2.5.0 — Object and linking hardening

- Added deterministic ELF/COFF output, duplicate-symbol rejection, non-executable-stack ELF metadata, and native link/execute validation.

## 2.4.0 — ABI call marshaling hardening

- Added cycle-safe mixed-class argument marshaling, Windows shadow-space validation, and dual-ABI call metrics.

## 2.3.0 — Floating-point backend hardening

- Added NaN-correct floating comparisons, floating compare/branch fusion, and floating zeroing idioms.

## 2.2.0 — Machine liveness and global DCE

- Added shared machine liveness, fixed-point global dead-code elimination, and safe fallthrough spill-cache preservation.

## 2.1.0 — Register allocator upgrade

- Added call-aware register classes, weighted spill costs, pressure metrics, and allocation hints.

## 2.0.0 and earlier

Earlier releases established the verified IR, interpreter/JIT differential model, x86-64 encoder, ELF/COFF writers, spill-store elimination, multi-entry spill caching, rematerialization, machine cleanup, CFG/layout optimization, immediate forms, memory-source folding, and exact code-quality gates.
