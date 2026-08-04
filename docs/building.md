# Building and testing

For working on this repo, or building the C++ core from source. Consumers of the npm package need
none of this — see [the JavaScript API](javascript-api.md).

## Native

Any C++23 compiler; no dependencies:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

That runs two tests:

- **`tst_TempoCurveCore`** — the analytic unit test. It checks the conversion API against an
  independent closed-form oracle (elapsed time at a constant tempo), plus oracle-free properties
  (monotonicity, invertibility, sign sensitivity) for the bent segments no independent oracle covers.
- **`run_fixture`** — drives the core over the shared fixture
  ([`fixtures/tempo_curve_cases.json`](../fixtures/tempo_curve_cases.json)) on both the scalar and
  bulk paths, and emits `build/native.json` for the parity check below.

The core library and `tst_TempoCurveCore` have **no dependencies at all**. `run_fixture` additionally
uses Python to turn the fixture JSON into a C++ header at build time, so there is no runtime JSON
parser; without Python it is skipped and everything else still builds and tests.

## WASM ⟷ C++ parity

Needs [Emscripten](https://emscripten.org/) and Node. Run the native `ctest` first, so
`build/native.json` exists:

```bash
bash wasm/build.sh build-wasm
node wasm/run_fixture.mjs build-wasm/tempo_wasm.mjs fixtures/tempo_curve_cases.json build/native.json
```

This builds the *same* sources to WebAssembly and asserts the WASM build agrees with the native build
over the shared fixture, on both the scalar and bulk-range paths — within each case's tolerance
rather than bit-for-bit. Cross-arch and WASM last-ULP differences are fine; behavioral drift is not.

## What CI proves

`.github/workflows/ci.yml` runs both of the above on Linux (x86_64) and macOS (arm64), plus the npm
package build and its frozen-API parity test. So every change is proven to build dependency-free
under clang on two architectures, and to keep the native and WASM builds in agreement.

## Maintainer tasks

Regenerating the shared fixture, releasing the npm package, and propagating a behavioral change to
the consumers of this core are covered in [MAINTAINERS.md](../MAINTAINERS.md).
