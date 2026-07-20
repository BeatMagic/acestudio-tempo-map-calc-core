# ACE Studio Tempo Map Calculation Core

This repo is the canonical reference for how the **tempo map** is calculated in
[ACE Studio](https://acestudio.ai) — the conversion between musical position (ticks) and
wall-clock time (seconds) across a curve of tempo changes.

The code is plain, dependency-free C++ (one header + one source file — no framework, no event
loop, no allocations on the conversion path), so it is easy to drop into your own workflow or
tools. You can compile it to WebAssembly with [Emscripten](https://emscripten.org/) like
[we do](https://www.npmjs.com/package/@timedomain/acestudio-tempo-map-calc), wrap it as a Python
wheel, or read it as the specification you [port to another language](#porting-to-another-language).

## Why this is public

There's no standard for the exact *shape* of a tempo ramp — given two control points at different
BPMs, nothing pins down how the tempo moves between them, and DAWs don't expose the curve they
use. Tools that sync tempo (ARA, for one) don't match the curve at all; they approximate it,
typically by sampling densely enough to keep the error bounded.

This is the exact math ACE Studio runs. Publishing it means you don't have to approximate: an
extension, a companion tool, or another host can line up on the same curve. Consider it an open
invitation.

## The model

A tempo map is a list of **control points** sorted strictly ascending by position. Each point
carries a position `pos` (in ticks), a tempo `bpm`, and a `bend` parameter that shapes the ramp
leaving that point.

- **Ticks.** Position is measured in ticks; `RESOLUTION` (480) is ACE Studio's ticks per quarter
  note (TPQN). At a constant tempo `B`, `seconds = ticks * (60 / RESOLUTION) / B`.
- **Between two adjacent points**, elapsed time is the integral of `1 / tempo` over the segment,
  and the tempo interpolation depends on `bend`:
  - equal BPM at both ends → **constant tempo** (time is linear in ticks);
  - `bend == 0` → BPM ramps **linearly** in ticks, which makes elapsed time a **logarithmic**
    function of position;
  - `bend != 0` → BPM follows an **exponential** ramp shaped by `bend` (`bendFactor = exp(bend)`).
- **Before the first point and after the last**, tempo is constant at that end point's BPM,
  extrapolated linearly. With no control points at all, the map is a constant default of 120 BPM.
- `recomputeTimes()` caches the cumulative time at each control point and shifts the whole axis so
  that **tick 0 maps to time 0** — the first control point need not sit at tick 0.

`pos2Time` and `time2Pos` are analytic inverses of each other, per segment. The closed forms for
each case live in `src/TempoCurveCore.cpp` (`getTimeRelativeTo` / `getPosRelativeTo`); that file
is the primary thing to read if you are porting the math.

## Public API

`include/TempoCurveCore.h` — everything is in `namespace tempo_curve`:

```cpp
struct TempoPoint {
    double pos = 0.0;         // position in ticks
    double bpm = 0.0;         // tempo at this point
    double bend = 0.0;        // ramp-shaping parameter
    double bendFactor = 1.0;  // exp(bend), precomputed by the caller
    double time = 0.0;        // cached cumulative time, filled by recomputeTimes()
};

// Re-anchor the cached time axis so tick 0 maps to time 0. Call after building/editing points.
void recomputeTimes(std::span<TempoPoint> points);

// Bulk conversion (inputs sorted ascending; output at least as large as input).
void pos2TimeRange(std::span<const TempoPoint> points, std::span<const double> positions, std::span<double> times);
void time2PosRange(std::span<const TempoPoint> points, std::span<const double> times, std::span<double> positions);

// Single-value convenience (non-allocating).
double pos2Time(std::span<const TempoPoint> points, double pos);
double time2Pos(std::span<const TempoPoint> points, double time);
```

The API is `std::span`-based, so it is container-neutral: store your points in any contiguous
buffer. The core is **read-only** — editing, undo, and storage belong to whatever embeds it.

Two things the caller owns before converting: set each point's `bendFactor` to `exp(bend)`, then
call `recomputeTimes()`. Points must be **sorted strictly ascending by `pos`** (no two points
share a position); the read-only conversion functions assume this.

## Build & test

**Native** (any C++23 compiler; no dependencies):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure   # tst_TempoCurveCore + run_fixture
```

The core library and `tst_TempoCurveCore` have no dependencies at all. The fixture test
(`run_fixture`) additionally uses Python to turn the fixture JSON into a C++ header at build time
(so there is no runtime JSON parser); without Python it is skipped and the rest still builds.

**WASM ⟷ C++ parity** (needs [Emscripten](https://emscripten.org/) + Node; run the native
`ctest` first so `build/native.json` exists):

```bash
bash wasm/build.sh build-wasm
node wasm/run_fixture.mjs build-wasm/tempo_wasm.mjs fixtures/tempo_curve_cases.json build/native.json
```

This builds the *same* sources to WebAssembly and asserts the WASM build agrees with the native
build over the shared fixture, on both the scalar (`pos2Time`/`time2Pos`) and bulk-range paths —
within each case's tolerance, not bit-for-bit (cross-arch/WASM last-ULP differences are fine;
behavioral drift is not).

CI (`.github/workflows/ci.yml`) runs both on Linux (x86_64) and macOS (arm64), so every change is
proven to build dependency-free under clang on two architectures and to stay in agreement between
the native and WASM builds.

## Porting to another language

1. Reimplement `pos2Time` / `time2Pos` (and, if you want the batch path, the range variants)
   following `src/TempoCurveCore.cpp`. The per-segment integrators are the whole algorithm.
2. Validate against `fixtures/tempo_curve_cases.json`: build each case's points, and assert your
   outputs match the expected values within `tolerance`. The numbers are the core's own
   full-precision outputs, so matching them means agreeing with ACE Studio's tempo math.
   `wasm/run_fixture.mjs` is a working example of a non-C++ consumer driving the fixture.

## License

[MIT](LICENSE)
