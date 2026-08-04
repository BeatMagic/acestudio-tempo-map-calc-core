# AGENTS.md

Context for coding agents working in this repo. Human readers should start from
[README.md](README.md); this file collects the layout, commands, and constraints an agent needs
before changing anything here.

## What this repo is

The canonical tick↔seconds tempo-curve math for ACE Studio, as dependency-free C++23. The same
`src/TempoCurveCore.cpp` compiles into the desktop app (pinned there as a git submodule) and into the
published `@timedomain/acestudio-tempo-map-calc` npm package (as WebAssembly). By design there is no
second implementation to keep in sync.

## Layout

| path | what it is |
| --- | --- |
| `include/TempoCurveCore.h`, `src/TempoCurveCore.cpp` | the entire core; the `.cpp`'s per-segment integrators are the algorithm |
| `fixtures/tempo_curve_cases.json` | golden inputs/outputs, asserted against by this repo *and* by downstream consumers. Generated, so **never hand-edit** it |
| `tools/gen_fixture.cpp` | regenerates that fixture from the core |
| `test/tst_TempoCurveCore.cpp` | analytic unit test against an independent oracle |
| `test/run_fixture.cpp`, `wasm/run_fixture.mjs` | fixture runners, native and WASM |
| `wasm/` | Emscripten build plus the embind surface for the npm package |
| `types/tempo_map_calc.d.ts` | hand-authored frozen public API for the package; source, not generated |
| `docs/` | human documentation, indexed from the README |

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
```

Full details, including the WASM parity run: [docs/building.md](docs/building.md).

## Hard constraints

- **Dependency-free and framework-free.** No third-party headers, no event loop. It must compile
  under a plain toolchain *and* under Emscripten. `tst_TempoCurveCore` builds the core with nothing
  extra on its include path, so a stray include fails the build.
- **The conversion path stays read-only and non-allocating.** Editing, undo, and storage belong to
  the consumers, not here.
- **Never hand-edit `fixtures/tempo_curve_cases.json`.** Regenerate it; see
  [MAINTAINERS.md](MAINTAINERS.md#regenerating-the-shared-fixture). `gen_fixture` refuses to emit if
  the core ever disagrees with its oracle.
- **A change that moves the numbers is a behavioral change.** It changes the desktop engine and the
  shipped WASM library at once, needs a regenerated fixture in the same commit, and has propagation
  steps in [MAINTAINERS.md](MAINTAINERS.md).
- **Verify doc claims against the code, not against neighbouring comments.** The math statements in
  `docs/` are load-bearing for people porting this to other languages, so a plausible-sounding
  restatement is a real defect. `src/TempoCurveCore.cpp` is the authority.

## Conventions

- Conventional Commits (`type(scope): summary`).
- Every change goes through a PR, and merges only with green CI: native plus WASM parity on two
  operating systems, and the npm package build.
