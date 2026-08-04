# JavaScript API

[`@timedomain/acestudio-tempo-map-calc`](https://www.npmjs.com/package/@timedomain/acestudio-tempo-map-calc)
is this same core compiled to WebAssembly — no C++ or Emscripten toolchain needed to consume it. See
[the tempo model](tempo-model.md) for what the values mean.

## Install

```bash
npm install @timedomain/acestudio-tempo-map-calc
```

The published tarball carries the prebuilt `.wasm`, an ES-module loader, and TypeScript types. It is
built from these sources with pinned Emscripten in release CI, and never committed to git.

## Hydrate, then convert

```js
import createTempoMapCalc from "@timedomain/acestudio-tempo-map-calc";

const mod = await createTempoMapCalc();
const calc = new mod.TempoMapCalc();

// Three parallel arrays — positions in ticks, BPMs, bends — sorted strictly ascending by position.
// Re-hydrate whenever the tempo map changes.
calc.hydrate([480, 1440, 2400], [120, 180, 90], [0, 0, 0]);

calc.pos2Time(960); // seconds at tick 960
calc.time2Pos(1.5); // tick position at 1.5 s
```

`hydrate()` handles the `bendFactor` precomputation and the time-axis anchoring for you — the two
[caller obligations](cpp-api.md#what-the-caller-owns) of the C++ API. It clamps to the shortest of
the three arrays, so a partial or mismatched snapshot can never over-read.

## Bulk conversion, zero copy

For many values at once, skip the per-value marshaling: the input and output views are
`Float64Array`s aliasing the WASM heap directly.

```js
const input = calc.inputView(3);
input.set([0, 480, 960]);           // sorted ascending
calc.convertPos2Time();             // or convertTime2Pos()
const seconds = calc.outputView();  // one result per input
```

The order matters, because both views alias heap memory that can move:

1. `inputView(n)` sizes the shared buffers and returns the input view. It stays valid until the next
   `inputView()` call.
2. Fill it, then call `convertPos2Time()` (ticks → seconds) or `convertTime2Pos()` (seconds → ticks).
3. Read `outputView()` before allocating again on the WASM heap.

## Lifetime

```js
calc.delete(); // release the underlying C++ object
```

The object lives on the WASM heap, so it is not garbage collected for you.
