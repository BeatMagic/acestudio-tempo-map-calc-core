# ACE Studio Tempo Map Calculation Core

The exact math [ACE Studio](https://acestudio.ai) uses to convert between musical position (ticks)
and wall-clock time (seconds) across a curve of tempo changes.

- **Canonical, not approximated.** This is the code the DAW runs, not a reimplementation and not a
  dense-sampling estimate of someone else's curve.
- **Dependency-free C++23.** One header, one source file. No framework, no event loop, no
  allocations on the conversion path.
- **Prebuilt for JavaScript.** WebAssembly on npm, with TypeScript types and a zero-copy bulk path.
- **Portable by construction.** A language-neutral golden fixture lets a port in any language prove
  it agrees with ACE Studio.

## Why this is public

There's no standard for the exact *shape* of a tempo ramp. Given two control points at different
BPMs, nothing pins down how the tempo moves between them, and DAWs don't expose the curve they use.
Tools that sync tempo (ARA, for one) don't match it. They approximate, typically by sampling
densely enough to keep the error bounded.

This is the curve itself, so you don't have to approximate: an extension, a companion tool, or
another host can line up exactly. Consider it an open invitation.

## Quick start

### JavaScript / TypeScript

```bash
npm install @timedomain/acestudio-tempo-map-calc
```

```js
import createTempoMapCalc from "@timedomain/acestudio-tempo-map-calc";

const mod = await createTempoMapCalc();
const calc = new mod.TempoMapCalc();

// Control points as three parallel arrays (ticks, BPMs, bends), strictly ascending by position.
calc.hydrate([480, 1440, 2400], [120, 180, 90], [0, 0, 0]);

calc.pos2Time(960); // → seconds at tick 960
calc.time2Pos(1.5); // → tick position at 1.5 s

calc.delete(); // free the WASM-side object
```

### C++

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```

```cpp
#include "TempoCurveCore.h"
using namespace tempo_curve;

std::vector<TempoPoint> points { /* pos, bpm, bend, bendFactor = exp(bend) */ };
recomputeTimes(points);                          // once, after building or editing
const double seconds = pos2Time(points, 960.0);
```

## Documentation

Both API references assume the terms defined in the tempo model, so start there.

- **[The tempo model](docs/tempo-model.md)**: what a tempo map *is* here. Ticks, control points, how
  tempo interpolates between them, what `bend` shapes, and how the time axis is anchored.
- **[C++ API](docs/cpp-api.md)**: the `std::span` conversion API, and the two things a caller owns
  before converting.
- **[JavaScript API](docs/javascript-api.md)**: the npm package in full. Hydration, scalar and
  zero-copy bulk conversion, object lifetime.
- **[Porting to another language](docs/porting.md)**: the recipe, and how to prove your port agrees
  with ACE Studio using the shared fixture.
- **[Building and testing](docs/building.md)**: building from source, the native and WASM test
  suites, and what CI proves.

## License

[MIT](LICENSE)
