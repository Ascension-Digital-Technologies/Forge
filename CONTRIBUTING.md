# Contributing to Forge

Forge accepts changes that preserve deterministic output, verified IR boundaries, and the interpreter/JIT differential contract.

## Development workflow

1. Configure and run the strict release gate:

   ```sh
   ./scripts/release-gate.sh
   ```

2. Run the sanitizer gate before opening a pull request:

   ```sh
   ./scripts/sanitizer-gate.sh
   ```

3. Add focused regression coverage for every parser, verifier, optimizer, ABI, object, or code-generation change.
4. Add or tighten a code-quality baseline when a change intentionally improves emitted code.
5. Keep generated build output outside the source tree except under the ignored `build/` directory used by presets.

## Change requirements

- Build cleanly with warnings treated as errors.
- Pass the full test matrix on supported platforms.
- Preserve deterministic canonical IR and object output.
- Verify machine IR after transformations that change control flow, operands, or virtual-register numbering.
- Never weaken a regression threshold merely to make a change pass; document and justify intentional baseline updates.

## Commit scope

Keep commits focused and describe user-visible behavior, correctness impact, and validation performed. Large backend changes should include a short design note under `docs/` when they introduce a new invariant or pipeline stage.
