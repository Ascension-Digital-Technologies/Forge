# Production readiness

Forge 4.0.0 is gated as a production-quality compiler core for its documented feature set.

## Required release gates

- Strict Release build with `-Wall -Wextra -Wpedantic -Werror` or the MSVC equivalent
- Complete CTest matrix
- ASan and UBSan matrix with leak detection
- Parser and binary fuzz-smoke targets
- Verification of every checked-in `.fir` example
- Deterministic canonical optimization output at every optimization level
- Deterministic ELF and COFF objects
- Native ELF compile, link, and execute workflow
- Isolated install-tree consumer build and execution
- Exact code-quality baselines for backend optimizations
- Repository-hygiene check

## Supported production surface

- Textual and binary Forge IR
- IR verification and canonical printing
- Optimization pipelines through `-O0/-O1/-O2/-O3/-Os/-Oz`
- Interpreter execution
- x86-64 JIT execution on supported hosts
- System V AMD64 and Windows x64 scalar/pointer calls
- ELF64 and COFF AMD64 object generation
- Installed static C++ library and command-line tools
- CMake package consumption through `Forge::forge`

## Explicitly unsupported or incomplete

- Native by-value aggregate ABI classification
- True variadic function definitions
- Unwind/debug metadata
- Segmented live-range register allocation
- Architectures other than x86-64

A release must not claim these capabilities until their implementation, semantic tests, platform tests, and release gates are complete.
