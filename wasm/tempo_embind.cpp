// Production embind binding for @timedomain/acestudio-tempo-map-calc — the npm package's public
// surface over the same core sources. This is the real binding shipped to JS/TS consumers, as
// opposed to tempo_wasm.cpp, which is the tiny extern-"C" bridge used only by this repo's internal
// WASM<->C++ parity harness (wasm/run_fixture.mjs).
//
// The frozen API (see ADR 0086 addendum in ACE Studio):
//   - hydrate(pos, bpm, bend) from three parallel arrays of a revision-stamped tempo snapshot,
//     clamped to the shortest array so a partial/mismatched snapshot can never over-read.
//   - non-allocating single-value pos2Time / time2Pos for the occasional one-off conversion.
//   - a zero-copy typed-array bulk path: inputView(n) hands back a Float64Array aliasing the WASM
//     heap; the caller fills it, calls convertPos2Time()/convertTime2Pos(), then reads outputView()
//     (also heap-aliased). No per-value JS<->WASM marshaling on the hot path.
//
// Contract for the bulk path: fill the input view and read the output view within one
// inputView -> convert -> outputView cycle, without allocating on the WASM heap in between. The
// returned views alias WASM memory, so a heap growth (or the next inputView call) invalidates
// them. convert*/outputView never grow the heap; only inputView(n) may, and it does so before
// returning the fresh view.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "TempoCurveCore.h"

namespace {

using tempo_curve::TempoPoint;

class TempoMapCalc
{
public:
    // Rebuild the tempo map from a revision-stamped snapshot's three parallel arrays. Clamps to the
    // shortest array so a partial/mismatched snapshot can't over-read; precomputes bendFactor and
    // re-anchors the time axis, as the core expects before any conversion.
    void hydrate(const emscripten::val &posArr, const emscripten::val &bpmArr, const emscripten::val &bendArr)
    {
        const auto pos = emscripten::convertJSArrayToNumberVector<double>(posArr);
        const auto bpm = emscripten::convertJSArrayToNumberVector<double>(bpmArr);
        const auto bend = emscripten::convertJSArrayToNumberVector<double>(bendArr);

        const std::size_t n = std::min({pos.size(), bpm.size(), bend.size()});
        m_points.clear();
        m_points.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            TempoPoint p;
            p.pos = pos[i];
            p.bpm = bpm[i];
            p.bend = bend[i];
            p.bendFactor = std::exp(bend[i]);
            m_points.push_back(p);
        }
        tempo_curve::recomputeTimes(m_points);
    }

    // Single-value convenience — the non-allocating per-frame hot-path shape.
    double pos2Time(double pos) const { return tempo_curve::pos2Time(m_points, pos); }
    double time2Pos(double time) const { return tempo_curve::time2Pos(m_points, time); }

    // Zero-copy bulk path. Size the shared input/output buffers to n and hand back a Float64Array
    // aliasing the input buffer for the caller to fill (inputs must be sorted ascending). resize
    // (not assign) avoids re-zeroing on every call: the caller overwrites the whole input view and
    // convert* writes the whole output buffer, so a same-size call is a no-op here.
    emscripten::val inputView(int n)
    {
        const std::size_t count = n > 0 ? static_cast<std::size_t>(n) : 0;
        m_in.resize(count);
        m_out.resize(count);
        return emscripten::val(emscripten::typed_memory_view(m_in.size(), m_in.data()));
    }

    void convertPos2Time()
    {
        tempo_curve::pos2TimeRange(m_points, std::span<const double>(m_in), std::span<double>(m_out));
    }

    void convertTime2Pos()
    {
        tempo_curve::time2PosRange(m_points, std::span<const double>(m_in), std::span<double>(m_out));
    }

    // Float64Array aliasing the output buffer written by the last convert*() call.
    emscripten::val outputView()
    {
        return emscripten::val(emscripten::typed_memory_view(m_out.size(), m_out.data()));
    }

private:
    std::vector<TempoPoint> m_points;
    std::vector<double> m_in;
    std::vector<double> m_out;
};

} // namespace

EMSCRIPTEN_BINDINGS(tempo_map_calc)
{
    emscripten::class_<TempoMapCalc>("TempoMapCalc")
        .constructor<>()
        .function("hydrate", &TempoMapCalc::hydrate)
        .function("pos2Time", &TempoMapCalc::pos2Time)
        .function("time2Pos", &TempoMapCalc::time2Pos)
        .function("inputView", &TempoMapCalc::inputView)
        .function("convertPos2Time", &TempoMapCalc::convertPos2Time)
        .function("convertTime2Pos", &TempoMapCalc::convertTime2Pos)
        .function("outputView", &TempoMapCalc::outputView);
}
