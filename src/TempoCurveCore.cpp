// See the banner in TempoCurveCore.h. Keep this file dependency-free and framework-free.

#include "TempoCurveCore.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace tempo_curve {

namespace {

// Per-segment integrators (bend/exp/log ramp math) — reads only. File-local: only the
// functions in this translation unit call them; consumers use the public span API below.
double getTimeRelativeTo(std::span<const TempoPoint> points, std::ptrdiff_t index, double tickOffset)
{
    if (tickOffset == 0.0) {
        return 0.0;
    }

    static constexpr double r = 60.0 / RESOLUTION;
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(points.size());
    const bool isLast = (index == n - 1);

    // Defense-in-depth against corrupt data: a non-positive bpm in the relevant segment would
    // make the formulas below divide by zero / take log of a non-positive value, yielding NaN
    // that recomputeTimes() then propagates to every cached time. Treat such a segment as
    // zero-duration. Valid tempo is clamped well above 0, so this never triggers for real data.
    if (points[index].bpm <= 0.0 || (!isLast && points[index + 1].bpm <= 0.0)) {
        return 0.0;
    }

    if (isLast) {
        return tickOffset / points[index].bpm * r;
    }

    const auto &point = points[index];
    const auto &nextPoint = points[index + 1];

    if (point.bpm == nextPoint.bpm) {
        return tickOffset / point.bpm * r;
    }

    if (point.bend == 0.0) {
        double A = (nextPoint.bpm - point.bpm) / (nextPoint.pos - point.pos);
        return r / A * std::log((A * tickOffset + point.bpm) / point.bpm);
    }

    double K = (nextPoint.bpm - point.bpm) / (point.bendFactor - 1);
    double C = point.bpm - K;
    double d = nextPoint.pos - point.pos;
    if (C == 0.0) {
        return r * d / (point.bend * point.bpm) * (1 - std::exp(-point.bend * tickOffset / d));
    }

    double T = std::pow(point.bendFactor, tickOffset / d);
    return r * d / (point.bend * C) * std::log((T * (C + K)) / (C + K * T));
}

double getPosRelativeTo(std::span<const TempoPoint> points, std::ptrdiff_t index, double timeOffset)
{
    if (timeOffset == 0.0) {
        return 0.0;
    }

    static constexpr double r = 60.0 / RESOLUTION;
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(points.size());
    const bool isLast = (index == n - 1);

    // Same corrupt-bpm guard as getTimeRelativeTo: a non-positive bpm would produce NaN/inf;
    // treat the segment as zero-duration.
    if (points[index].bpm <= 0.0 || (!isLast && points[index + 1].bpm <= 0.0)) {
        return 0.0;
    }

    if (isLast) {
        return timeOffset * points[index].bpm / r;
    }

    const auto &point = points[index];
    const auto &nextPoint = points[index + 1];

    if (point.bpm == nextPoint.bpm) {
        return timeOffset * point.bpm / r;
    }

    if (point.bend == 0.0) {
        double A = (nextPoint.bpm - point.bpm) / (nextPoint.pos - point.pos);
        return (std::exp((A / r) * timeOffset) * point.bpm - point.bpm) / A;
    }

    double K = (nextPoint.bpm - point.bpm) / (point.bendFactor - 1);
    double C = point.bpm - K;
    double d = nextPoint.pos - point.pos;
    if (C == 0.0) {
        return -d / point.bend * std::log(1 - timeOffset / d / r * (point.bend * point.bpm));
    }

    double S = point.bend / d / r * timeOffset * C - std::log(point.bpm);
    return d / point.bend * std::log(C * std::exp(S) / (1 - K * std::exp(S)));
}

} // namespace

void recomputeTimes(std::span<TempoPoint> points)
{
    static constexpr double r = 60.0 / RESOLUTION;
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(points.size());
    if (n == 0) {
        // No control points to anchor (e.g. all removed, or corrupt data). Nothing to
        // recompute; pos2Time/time2Pos fall back to a constant DEFAULT_TEMPO.
        return;
    }

    // Pass 1: cumulative times using the first point as a provisional time origin.
    points[0].time = 0.0;
    for (std::ptrdiff_t i = 1; i < n; i++) {
        points[i].time = points[i - 1].time + getTimeRelativeTo(points, i - 1, points[i].pos - points[i - 1].pos);
    }

    // Pass 2: shift the whole axis so that tick 0 maps to time 0. We don't assume the first
    // point is at tick 0 (it may be at a positive or negative tick); we only assume the points
    // are sorted ascending. Find the provisional time at tick 0, then subtract it from every
    // point.
    double timeAtZero;
    if (points.front().pos >= 0.0) {
        // tick 0 is at or before the first point, i.e. in the leading constant-tempo region
        // (= first point's bpm), so extrapolate backward from the first point. Guard bpm > 0 to
        // avoid a divide-by-zero from a malformed point.
        const double bpm = points.front().bpm;
        timeAtZero = bpm > 0.0 ? -points.front().pos * r / bpm : 0.0;
    } else {
        // tick 0 lies within (or after) the curve: walk to the segment that contains it and
        // integrate forward from that segment's left point.
        std::ptrdiff_t i = 0;
        while (i + 1 < n && points[i + 1].pos <= 0.0) {
            i++;
        }
        timeAtZero = points[i].time + getTimeRelativeTo(points, i, 0.0 - points[i].pos);
    }

    if (timeAtZero != 0.0) {
        for (std::ptrdiff_t i = 0; i < n; i++) {
            points[i].time -= timeAtZero;
        }
    }
}

void pos2TimeRange(std::span<const TempoPoint> points, std::span<const double> positions, std::span<double> times)
{
    // Contract: times.size() >= positions.size(). The assert catches a violation in debug
    // builds; the min() keeps a malformed caller from writing out of bounds in Release — this
    // core is shared with the WASM build, whose consumers we don't control. Extra inputs (if the
    // output is undersized) are dropped rather than trusted. For valid callers min is a no-op.
    assert(times.size() >= positions.size());
    const std::ptrdiff_t count = static_cast<std::ptrdiff_t>(std::min(positions.size(), times.size()));
    std::ptrdiff_t index = count - 1;
    for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(points.size()) - 1; i >= 0; i--) {
        const auto &point = points[i];
        while (index >= 0 && positions[index] >= point.pos) {
            times[index] = point.time + getTimeRelativeTo(points, i, positions[index] - point.pos);
            index--;
        }
        if (index < 0) {
            break;
        }
    }
    if (index >= 0) {
        // Positions before the first control point: tempo is constant (= the first point's
        // bpm), extrapolated linearly from the first point. Anchoring on the first point
        // (rather than on tick 0) keeps pos2Time continuous at the first point for any
        // first-point position; recomputeTimes() has already set first().time so that tick 0
        // maps to time 0.
        static constexpr double r = 60.0 / RESOLUTION;
        if (points.empty()) {
            // No control points: constant DEFAULT_TEMPO anchored at tick 0.
            while (index >= 0) {
                times[index] = positions[index] * r / DEFAULT_TEMPO;
                index--;
            }
        } else {
            const auto &first = points.front();
            // Guard bpm > 0 (hoisted so the loop stays branch-free); a malformed zero bpm
            // collapses the lead-in to a flat segment instead of producing inf/nan.
            const double rate = first.bpm > 0.0 ? r / first.bpm : 0.0;
            while (index >= 0) {
                times[index] = first.time + (positions[index] - first.pos) * rate;
                index--;
            }
        }
    }
}

void time2PosRange(std::span<const TempoPoint> points, std::span<const double> times, std::span<double> positions)
{
    // Contract mirror of pos2TimeRange: the assert flags an undersized output in debug, the
    // min() bounds the Release loop so a malformed shared-core caller can't write out of bounds.
    assert(positions.size() >= times.size());
    const std::ptrdiff_t count = static_cast<std::ptrdiff_t>(std::min(times.size(), positions.size()));
    std::ptrdiff_t index = count - 1;
    for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(points.size()) - 1; i >= 0; i--) {
        const auto &point = points[i];
        while (index >= 0 && times[index] >= point.time) {
            positions[index] = point.pos + getPosRelativeTo(points, i, times[index] - point.time);
            index--;
        }
        if (index < 0) {
            break;
        }
    }
    if (index >= 0) {
        // Times before the first control point: inverse of the constant-tempo extrapolation in
        // pos2TimeRange, anchored on the first point so time2Pos stays continuous there for any
        // first-point position.
        static constexpr double r = 60.0 / RESOLUTION;
        if (points.empty()) {
            // No control points: inverse of the constant DEFAULT_TEMPO fallback.
            while (index >= 0) {
                positions[index] = times[index] * DEFAULT_TEMPO / r;
                index--;
            }
        } else {
            const auto &first = points.front();
            // Mirror the pos2TimeRange lead-in guard: a non-positive first bpm collapses the
            // pre-first-point region to a flat segment instead of a sign-flipped, non-monotonic
            // inverse. No-op for valid tempo.
            const double rate = first.bpm > 0.0 ? first.bpm / r : 0.0;
            while (index >= 0) {
                positions[index] = first.pos + (times[index] - first.time) * rate;
                index--;
            }
        }
    }
}

double pos2Time(std::span<const TempoPoint> points, double pos)
{
    double time;
    pos2TimeRange(points, std::span<const double>(&pos, 1), std::span<double>(&time, 1));
    return time;
}

double time2Pos(std::span<const TempoPoint> points, double time)
{
    double pos;
    time2PosRange(points, std::span<const double>(&time, 1), std::span<double>(&pos, 1));
    return pos;
}

} // namespace tempo_curve
