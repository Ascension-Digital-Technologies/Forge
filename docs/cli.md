# Command-line reference

Forge installs six command-line tools.

## `forge`

Primary driver for verification, optimization, and native object compilation.

```sh
forge verify input.fir
forge compile input.fir -O2 --format=elf -o output.o
```

## `forge-opt`

Runs the optimization pipeline and can report pass statistics and timings.

```sh
forge-opt input.fir -O3 --stats --pass-timing
```

## `forge-as` and `forge-dis`

Convert between textual and binary Forge IR.

```sh
forge-as input.fir -o module.fbc
forge-dis module.fbc -o module.fir
```

## `forge-codegen`

Displays machine lowering, allocation, encoder statistics, and code-quality metrics.

## `forge-run`

Executes a function with the interpreter, JIT, or differential engine.

```sh
forge-run --engine=compare input.fir function_name 1 2
```

## Optimization levels

- `-O0`: minimal transformation
- `-O1`: low-cost cleanup
- `-O2`: standard production optimization
- `-O3`: aggressive optimization
- `-Os`: optimize for size
- `-Oz`: minimize code size

Use each tool's `--help` output as the authoritative option list for the installed version.
