// ACE Studio Tempo Map Calculation Core — the canonical reference for ACE Studio's tick<->seconds
// tempo-map conversion. Container-neutral (std::span) and read-only: this is pure calculation, so
// editing, undo, and storage belong to whatever embeds it.
//
// Keep this file dependency-free and event-loop-free: no third-party headers, no framework, no
// async. It must compile as-is under a plain toolchain and under Emscripten, so it can be built
// to WebAssembly (or ported) without change. The standalone tst_TempoCurveCore target builds it
// with nothing extra on its include path, so a stray dependency fails the build.
//
// See README.md for the tempo-curve model and the public API; src/TempoCurveCore.cpp for the math.

#ifndef TEMPO_CURVE_CORE_H
#define TEMPO_CURVE_CORE_H

#include <cstddef>
#include <span>

namespace tempo_curve {

inline constexpr int RESOLUTION = 480; // ticks per quarter note
inline constexpr double DEFAULT_TEMPO = 120.0;

// A tempo control point. `time` is the cached cumulative time, filled by recomputeTimes();
// bendFactor is exp(bend), precomputed by the caller.
//
// PRECONDITION for every function below: `points` is sorted by `pos` **strictly
// increasing** — no two points share a `pos`. Equal adjacent positions divide by zero in
// the segment integrators. The embedder enforces this on edit; the read-only calculation
// core assumes it.
struct TempoPoint
{
    double pos = 0.0;
    double bpm = 0.0;
    double bend = 0.0;
    double bendFactor = 1.0;
    double time = 0.0;
};

// The per-segment integrators (bend/exp/log ramp math) that back these functions are
// file-local to TempoCurveCore.cpp — this surface is exactly what consumers call.

// Re-anchor the cached time axis so tick 0 maps to time 0. Mutates points[].time.
// Points must be sorted ascending by pos.
void recomputeTimes(std::span<TempoPoint> points);

// Bulk conversion. positions/times must be sorted ascending; out.size() >= in.size().
void pos2TimeRange(std::span<const TempoPoint> points, std::span<const double> positions, std::span<double> times);
void time2PosRange(std::span<const TempoPoint> points, std::span<const double> times, std::span<double> positions);

// Non-allocating single-value convenience (the per-frame hot-path shape).
double pos2Time(std::span<const TempoPoint> points, double pos);
double time2Pos(std::span<const TempoPoint> points, double time);

} // namespace tempo_curve

#endif // TEMPO_CURVE_CORE_H
