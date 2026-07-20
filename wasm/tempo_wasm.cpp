// Minimal Emscripten bridge over the core, used only by this repo's WASM<->C++ parity harness
// (wasm/run_fixture.mjs). It is deliberately a tiny C API — hydrate a tempo map, then convert
// single values and whole ranges — a parity harness, not a production binding. The point is to
// prove the WASM build of the same core sources agrees with the native build over the shared
// fixture, on both the scalar and the bulk-range paths. A real binding (embind, zero-copy typed
// arrays, etc.) is left to whoever ships the core to a given platform.

#include <cmath>
#include <span>
#include <vector>

#include <emscripten.h>

#include "TempoCurveCore.h"

using namespace tempo_curve;

namespace {
std::vector<TempoPoint> g_points;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void tc_reset()
{
    g_points.clear();
}

EMSCRIPTEN_KEEPALIVE void tc_add_point(double pos, double bpm, double bend)
{
    TempoPoint p;
    p.pos = pos;
    p.bpm = bpm;
    p.bend = bend;
    p.bendFactor = std::exp(bend); // the caller precomputes bendFactor, as the core expects
    g_points.push_back(p);
}

EMSCRIPTEN_KEEPALIVE void tc_recompute()
{
    recomputeTimes(g_points);
}

EMSCRIPTEN_KEEPALIVE double tc_pos2time(double pos)
{
    return pos2Time(g_points, pos);
}

EMSCRIPTEN_KEEPALIVE double tc_time2pos(double time)
{
    return time2Pos(g_points, time);
}

// Bulk range path (heap pointers): inputs sorted ascending, out has room for n values.
EMSCRIPTEN_KEEPALIVE void tc_pos2time_range(const double *positions, double *times, int n)
{
    pos2TimeRange(g_points, std::span<const double>(positions, n), std::span<double>(times, n));
}

EMSCRIPTEN_KEEPALIVE void tc_time2pos_range(const double *times, double *positions, int n)
{
    time2PosRange(g_points, std::span<const double>(times, n), std::span<double>(positions, n));
}

} // extern "C"
