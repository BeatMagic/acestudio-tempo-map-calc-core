# Maintainers

Notes for the people who keep this repo in sync with the products that consume it. Nothing here
is needed to *use* the core — see `README.md` for that.

## Who consumes this

The same `src/TempoCurveCore.cpp` + `include/TempoCurveCore.h` feed two builds:

1. **ACE Studio desktop** — pulls this repo in as a git submodule and compiles it in-tree behind a
   thin wrapper that owns editing, undo, and storage. Studio's public API is unchanged by living
   on top of the core.
2. **`@timedomain/acestudio-tempo-map-calc`** — the npm package built **from this repo itself**
   (`wasm/tempo_embind.cpp` + `package.json` here): stock Emscripten compiles the core to
   WebAssembly and the prebuilt artifact ships on public npm. No submodule; the package tracks the
   core sources in its own tree, so a core change is a package change in the same commit.

Because both compile the *same* file, there is no port to drift: the tempo curve is canonical by
construction. Design rationale lives in ADR 0086 in the (internal) ACE Studio repo.

## Shared-source contract

Anything a change here must preserve, so both builds keep working:

- **Dependency-free and framework-free.** No third-party headers, no event loop. It must keep
  compiling under a plain toolchain *and* under Emscripten. The standalone `tst_TempoCurveCore`
  target builds the core with nothing extra on its include path, so a stray dependency fails the
  build.
- **Read-only, non-allocating conversion path.** Mutation/undo/storage stay in the consumers.
- A behavioral change (anything that moves the numbers) changes the desktop engine *and* the
  shipped WASM library at once. Treat it as such.

## Propagation on a behavioral change

The npm package lives in this repo, so it moves with the sources automatically. Studio pins this
repo at a **specific commit** (submodule), so a core change is never silently absorbed there — it
opts in by bumping its pin in a reviewed PR.

1. **Land the change here first.** Merge to `main` only with green CI (native + WASM parity on
   both OSes, plus the npm package build + frozen-API parity). `main` is the branch Studio pins to
   and the branch releases are tagged from.
2. **Behavioral change** (conversion math or its results):
   - **npm package:** already tracks the new sources — [cut a release](#releasing-the-npm-package)
     so the published `.wasm` is rebuilt from them.
   - **Studio:** in the submodule dir, `git checkout <new-sha>`, then `git add` the submodule path
     from the superproject and open a PR. Studio's `tst_TempoAutomation` +
     `tst_TempoAgentContractE2E` re-validate the wrapper against the bumped core.
   - Keep Studio and the published package on the **same** core commit whenever a behavioral change
     lands, so the desktop engine and the shipped WASM library never diverge.
3. **Non-behavioral change** (docs, tests, CI, fixture tooling): bump Studio's pin / cut a package
   release whenever convenient; no artifact rebuild is forced.

There is deliberately no cross-repo hash/lock guard — this repo's own CI is the correctness and
parity net, and Studio's pin moves by review.

## Releasing the npm package

The WASM artifact is built for distribution **only** in release CI (`.github/workflows/publish.yml`),
never committed. Publishing uses npm **trusted publishing (OIDC)** — no long-lived publish token in
the repo. To publish `@timedomain/acestudio-tempo-map-calc`:

1. Bump `version` in `package.json` and merge to `main`.
2. Tag the release commit `vX.Y.Z` and push the tag (or publish a GitHub Release, which creates the
   tag).
3. `publish.yml` builds the artifact with pinned Emscripten, runs the frozen-API parity test, and
   runs `npm publish --access public`. It authenticates over OIDC via the package's trusted
   publisher — no `NPM_TOKEN` secret — and npm generates provenance automatically.

Build and check the package locally with `npm run build` then `node --test test/parity.test.mjs`,
or inspect the tarball with `npm pack --dry-run`.

## Regenerating the shared fixture

`fixtures/tempo_curve_cases.json` holds the golden inputs/outputs the tests (and any port) assert
against — the core's own full-precision outputs, with constant-tempo cases cross-checked against a
closed-form oracle. Don't hand-edit it; regenerate from the core:

```bash
cmake -S . -B build -DBUILD_FIXTURE_GENERATOR=ON && cmake --build build --target gen_fixture -j
./build/gen_fixture > fixtures/tempo_curve_cases.json
```

`gen_fixture` refuses to emit if the core ever disagrees with the oracle. A behavioral change
needs a fresh fixture committed alongside it.
