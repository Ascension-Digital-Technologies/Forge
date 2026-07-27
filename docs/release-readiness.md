# Release readiness

Forge 1.0.0 is the first stable release of the documented compiler-core, frontend SDK, and x86-64 scalar/pointer backend.

## Required release gates

- Strict Release build with warnings treated as errors
- Complete CTest matrix
- ASan and UBSan matrix with leak detection
- Parser and binary fuzz-smoke targets
- Verification of every checked-in `.fir` example
- Deterministic canonical optimization output at every optimization level
- Deterministic ELF and COFF objects
- Native compile, link, and execute workflow
- Incremental object and final-binary cache tests
- Isolated installed C and C++ package consumers
- Exact backend code-quality baselines
- Repository-hygiene checks

## Supported production surface

- Textual and binary Forge IR
- IR construction, verification, and canonical printing
- Optimization levels `-O0`, `-O1`, `-O2`, `-O3`, `-Os`, and `-Oz`
- Reference interpreter
- x86-64 JIT on supported hosts
- System V AMD64 and Windows x64 scalar/pointer calls
- ELF64 and COFF AMD64 object generation
- C++ SDK and opaque C API v9
- Incremental fingerprints, dependency planning, native function artifacts, object assembly, and final-binary caching
- Installed CMake package through `Forge::forge`

## Explicitly unsupported or incomplete

- Native by-value aggregate ABI classification
- True variadic function definitions
- Unwind and debug metadata
- Segmented live-range register allocation
- Architectures other than x86-64

A release must not claim these capabilities until implementation, semantic tests, platform tests, and release gates are complete.
