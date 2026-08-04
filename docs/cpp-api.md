# C++ API

Everything lives in `namespace tempo_curve`, declared in
[`include/TempoCurveCore.h`](../include/TempoCurveCore.h). See [the tempo model](tempo-model.md) for
what the fields mean, and [building and testing](building.md) for how to compile it.

## Surface

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

## What the caller owns

Two things must hold before you convert. The read-only functions assume both rather than checking
them:

1. **Each point's `bendFactor` is `exp(bend)`,** and `recomputeTimes()` has run since the last edit.
2. **Points are sorted strictly ascending by `pos`** — no two points share a position.

## Notes

- The API is `std::span`-based, so it is container-neutral: keep your points in any contiguous
  buffer.
- The core is **read-only**. Editing, undo, and storage belong to whatever embeds it.
- Nothing allocates on the conversion path.

## Embedding it

The whole core is [`include/TempoCurveCore.h`](../include/TempoCurveCore.h) plus
[`src/TempoCurveCore.cpp`](../src/TempoCurveCore.cpp), with no third-party includes — so dropping
those two files into an existing target works. The bundled CMake also exposes a `tempo_curve_core`
static library target.
