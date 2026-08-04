# AGENTS.md

Build and test: [docs/building.md](docs/building.md).

## Constraints

- **Dependency-free and framework-free.** The core must compile under a plain toolchain *and* under
  Emscripten. `tst_TempoCurveCore` builds it with nothing extra on the include path, so a stray
  include fails the build.
- **`fixtures/tempo_curve_cases.json` is generated. Never hand-edit it.** Regenerate it with
  `gen_fixture`; see [MAINTAINERS.md](MAINTAINERS.md#regenerating-the-shared-fixture).
- **Anything that moves the numbers changes both consumers at once** (the desktop app and the
  published WASM package compile the same source), and needs a regenerated fixture in the same
  commit.
