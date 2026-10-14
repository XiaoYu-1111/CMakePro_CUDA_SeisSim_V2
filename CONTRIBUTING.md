# Contributing to SeisWave Lab

The project is maintained as a research software workspace. Contributions are reviewed by the repository maintainer; there is currently no guaranteed support response time or formal multi-maintainer governance process.

## Reporting an issue

Use the repository issue tracker when available. Include software version/commit, Windows/compiler/CUDA/driver versions, GPU and memory, the selected module and a minimal sequence of operations. For numerical issues provide a small model, actual source/receiver coordinates, spacing, dt, boundary configuration and raw records. Remove private paths or confidential data before sharing. Screenshots help describe UI failures but do not replace numerical inputs.

## Proposing a change

1. Discuss major numerical or architectural changes in an issue first.
2. Keep changes focused and explain the previous behavior, the new behavior and any data-format impact.
3. Preserve independent 2D/3D mutable state and lifecycles. Shared components should normally be stateless utilities.
4. Use descriptive camelCase names for new functions/variables and a purpose comment before functions, including ownership, validation or state changes where relevant.
5. Follow [BUILDING](docs/build/BUILDING.md). Build directories, recordings and generated results belong in the parent `out`, not the source tree. Do not submit binaries, build caches or personal configuration.
6. Run the relevant existing regression checks when verifying a change and report results, skips and unavailable environments separately. Numerical claims need a documented benchmark and tolerances, not only a successful build.
7. Update user documentation and format descriptions when behavior changes; scientific screenshots must come from the real application without fabricated data.

Do not mass-rename vendored dependencies or mix a numerical-method change with unrelated UI refactoring. Existing compatibility names may be kept until their callers and tests can migrate together.

## Licensing and research integrity

Submit only material you have the right to contribute, under terms compatible with the project's MIT license and dependency licenses. Preserve attribution. Synthetic examples should state their assumptions; external datasets need a source and redistribution terms.

Disclose significant AI assistance in a change description, including which parts were assisted and which checks were performed. Human contributors remain responsible for design decisions, correctness, licensing and scientific interpretation. Paper-specific disclosures must follow the selected journal's current policy.
