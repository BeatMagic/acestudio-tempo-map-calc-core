// gen_fixture — emits the language-neutral shared fixture (fixtures/tempo_curve_cases.json)
// that BOTH the native fixture test (test/run_fixture.cpp) and the WASM/Node parity harness
// (wasm/run_fixture.mjs) assert against. This is a dev tool, not part of CI: run it once when
// the oracle set changes, then commit the regenerated JSON.
//
// Expected values are the canonical engine's own outputs, so the fixture pins the numbers a port
// (e.g. the WASM build) must reproduce. For every constant-tempo query — where
// an independent closed form exists — this tool ALSO computes the analytic value and refuses to
// emit unless the core matches it within kEps, so the golden is oracle-validated at birth. The
// separate analytic unit test (test/tst_TempoCurveCore.cpp) is the standing correctness net.
//
// Usage: gen_fixture > fixtures/tempo_curve_cases.json

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "TempoCurveCore.h"

using namespace tempo_curve;

namespace {

constexpr double kR = 60.0 / RESOLUTION;
constexpr double kEps = 1e-7;

double constTime(double ticks, double bpm) { return ticks * kR / bpm; }

// One control point as a case declares it. `bend` defaults to 0, so the constant-tempo and
// bend-free cases below read exactly as they did before it existed.
struct PointSpec
{
    double pos = 0.0;
    double bpm = 0.0;
    double bend = 0.0;
};

std::vector<TempoPoint> makePoints(std::initializer_list<PointSpec> spec)
{
    std::vector<TempoPoint> pts;
    for (const auto &s : spec) {
        TempoPoint p;
        p.pos = s.pos;
        p.bpm = s.bpm;
        p.bend = s.bend;
        p.bendFactor = std::exp(s.bend);
        pts.push_back(p);
    }
    recomputeTimes(pts);
    return pts;
}

// full-precision double so a port reproduces the exact bits the canonical engine emits
std::string num(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

int g_die = 0;
void mustMatchAnalytic(double core, double analytic, const char *what)
{
    if (std::abs(core - analytic) > kEps) {
        std::fprintf(stderr, "ORACLE MISMATCH in %s: core=%.17g analytic=%.17g\n", what, core, analytic);
        g_die = 1;
    }
}

struct Fixture
{
    std::string out;
    bool firstCase = true;

    // spec: the case's control points, exactly as a consumer must feed them back in — `bend`
    // included, since a consumer that ignores it reproduces a straight ramp where the core draws
    // a curved one. Query methods take `t`, the same points after makePoints() (bendFactor +
    // recomputeTimes), so the emitted golden is the core output.
    void beginCase(const std::string &name, const std::vector<PointSpec> &spec, double tolerance)
    {
        if (!firstCase)
            out += ",\n";
        firstCase = false;
        out += "    {\n";
        out += "      \"name\": \"" + name + "\",\n";
        out += "      \"tolerance\": " + num(tolerance) + ",\n";
        out += "      \"points\": [";
        bool f = true;
        for (const auto &s : spec) {
            out += f ? "" : ", ";
            f = false;
            out += "{\"pos\": " + num(s.pos) + ", \"bpm\": " + num(s.bpm) + ", \"bend\": " + num(s.bend) + "}";
        }
        out += "],\n";
    }

    // pos2Time queries: each pair is {pos, analytic}; expected = core output, cross-checked against
    // `analytic` when it is finite (NA means "no closed form — the core output is the golden").
    void pos2time(const std::vector<TempoPoint> &pts, const std::vector<std::pair<double, double>> &posAnalytic)
    {
        out += "      \"pos2time\": [";
        bool f = true;
        for (const auto &[pos, analytic] : posAnalytic) {
            const double t = pos2Time(pts, pos);
            if (std::isfinite(analytic))
                mustMatchAnalytic(t, analytic, "pos2time");
            out += f ? "" : ", ";
            f = false;
            out += "{\"pos\": " + num(pos) + ", \"time\": " + num(t) + "}";
        }
        out += "],\n";
    }

    void time2pos(const std::vector<TempoPoint> &pts, const std::vector<std::pair<double, double>> &timeAnalytic)
    {
        out += "      \"time2pos\": [";
        bool f = true;
        for (const auto &[time, analytic] : timeAnalytic) {
            const double p = time2Pos(pts, time);
            if (std::isfinite(analytic))
                mustMatchAnalytic(p, analytic, "time2pos");
            out += f ? "" : ", ";
            f = false;
            out += "{\"time\": " + num(time) + ", \"pos\": " + num(p) + "}";
        }
        out += "]\n";
    }

    void endCase() { out += "    }"; }
};

const double NA = std::nan(""); // "no analytic form — golden is the core's output"

} // namespace

int main()
{
    Fixture fx;

    // --- firstPointAtZero: single point at tick 0, 120 bpm (constant tempo everywhere) ---
    {
        auto t = makePoints({{0.0, 120.0}});
        fx.beginCase("firstPointAtZero", {{0.0, 120.0}}, kEps);
        fx.pos2time(t, {{0.0, 0.0}, {480.0, 0.5}, {960.0, 1.0}});
        fx.time2pos(t, {{0.0, 0.0}, {0.5, 480.0}});
        fx.endCase();
    }

    // --- firstPointPositive: single point at tick 480; constant-tempo lead-in before it ---
    {
        auto t = makePoints({{480.0, 120.0}});
        fx.beginCase("firstPointPositive", {{480.0, 120.0}}, kEps);
        fx.pos2time(t, {{0.0, 0.0}, {240.0, 0.25}, {480.0, 0.5}, {960.0, 1.0}});
        fx.time2pos(t, {{0.0, 0.0}, {0.5, 480.0}});
        fx.endCase();
    }

    // --- firstPointNegativeStraddling: two points straddling tick 0, same bpm => constant ---
    {
        auto t = makePoints({{-480.0, 120.0}, {480.0, 120.0}});
        fx.beginCase("firstPointNegativeStraddling", {{-480.0, 120.0}, {480.0, 120.0}}, kEps);
        fx.pos2time(t, {{0.0, 0.0}, {480.0, constTime(480.0, 120.0)}, {-480.0, constTime(-480.0, 120.0)}, {-960.0, constTime(-960.0, 120.0)}});
        fx.time2pos(t, {{0.0, 0.0}});
        fx.endCase();
    }

    // --- allPointsNegative: both points before tick 0, same bpm => constant everywhere ---
    {
        auto t = makePoints({{-960.0, 120.0}, {-480.0, 120.0}});
        fx.beginCase("allPointsNegative", {{-960.0, 120.0}, {-480.0, 120.0}}, kEps);
        fx.pos2time(t, {{0.0, 0.0}, {480.0, constTime(480.0, 120.0)}, {-480.0, constTime(-480.0, 120.0)}});
        fx.time2pos(t, {{0.0, 0.0}});
        fx.endCase();
    }

    // --- emptyList: no control points => constant DEFAULT_TEMPO anchored at tick 0 ---
    {
        std::vector<TempoPoint> t;
        fx.beginCase("emptyList", {}, kEps);
        fx.pos2time(t, {{0.0, 0.0}, {480.0, constTime(480.0, DEFAULT_TEMPO)}});
        fx.time2pos(t, {{constTime(480.0, DEFAULT_TEMPO), 480.0}});
        fx.endCase();
    }

    // --- curvedMultiPoint: varying-tempo curve; no closed form, golden = canonical core output.
    // A port that reproduces these within tolerance agrees with the C++ engine on the curved math. ---
    {
        auto t = makePoints({{480.0, 120.0}, {1440.0, 180.0}, {2400.0, 90.0}});
        fx.beginCase("curvedMultiPoint", {{480.0, 120.0}, {1440.0, 180.0}, {2400.0, 90.0}}, 1e-9);
        fx.pos2time(t, {{0.0, NA}, {480.0, NA}, {960.0, NA}, {1440.0, NA}, {1920.0, NA}, {2400.0, NA}, {3000.0, NA}});
        // time2pos probed at the times the curve maps those positions to, so the queries land on
        // the curve; expected is NA (the core output is the golden, cross-checked by round-trip).
        std::vector<std::pair<double, double>> timeQueries;
        for (double pos : {0.0, 480.0, 960.0, 1440.0, 1920.0, 2400.0, 3000.0})
            timeQueries.emplace_back(pos2Time(t, pos), NA);
        fx.time2pos(t, timeQueries);
        fx.endCase();
    }

    // --- bendEqualBpmStaysConstant: bend != 0 but the SAME bpm at both ends. The model says equal
    // endpoints mean constant tempo, so bend has nothing to shape and the closed form still
    // applies — which makes this the one bend case with an independent oracle. It also pins the
    // precedence: a consumer that applied bend before checking for equal endpoints would curve a
    // segment that must stay straight, and would fail here rather than silently disagreeing. ---
    {
        auto t = makePoints({{0.0, 120.0, 1.5}, {960.0, 120.0, 0.0}});
        fx.beginCase("bendEqualBpmStaysConstant", {{0.0, 120.0, 1.5}, {960.0, 120.0, 0.0}}, kEps);
        fx.pos2time(t, {{0.0, 0.0},
                        {240.0, constTime(240.0, 120.0)},
                        {480.0, constTime(480.0, 120.0)},
                        {960.0, constTime(960.0, 120.0)},
                        {1440.0, constTime(1440.0, 120.0)}});
        fx.time2pos(t, {{0.0, 0.0}, {constTime(480.0, 120.0), 480.0}});
        fx.endCase();
    }

    // --- bendPositiveRamp / bendNegativeRamp: a single curved segment each, one accelerating and
    // one decelerating. bend shapes the ramp exponentially (bendFactor = exp(bend)). The core has a
    // closed form for that, but nothing independent to check the form against, so the canonical core
    // output is the golden. Sampled at both control points, one tick either side of each, and
    // interior quarters. A port that ignored bend, or applied it with the wrong sign, diverges over
    // all of it: bend changes the segment's integral, not just its shape, so it moves the interior
    // AND the time at the control point closing the segment. Only tick 0 — the origin
    // recomputeTimes anchors — is invariant. ---
    for (const auto &[name, from, to, bend] : {std::tuple {"bendPositiveRamp", 120.0, 180.0, 2.0},
                                               std::tuple {"bendNegativeRamp", 180.0, 90.0, -1.5}}) {
        const double end = 1920.0;
        auto t = makePoints({{0.0, from, bend}, {end, to, 0.0}});
        fx.beginCase(name, {{0.0, from, bend}, {end, to, 0.0}}, 1e-9);
        std::vector<std::pair<double, double>> posQueries;
        for (double pos : {0.0, 1.0, end * 0.25, end * 0.5, end * 0.75, end - 1.0, end, end + 480.0})
            posQueries.emplace_back(pos, NA);
        fx.pos2time(t, posQueries);
        std::vector<std::pair<double, double>> timeQueries;
        for (const auto &q : posQueries)
            timeQueries.emplace_back(pos2Time(t, q.first), NA);
        fx.time2pos(t, timeQueries);
        fx.endCase();
    }

    // --- bendMultiPointBothSigns: the shape a real project produces — a constant lead, a strongly
    // curved climb, a curved fall, and a constant tail — with bend of both signs so neither sign
    // can be dropped without a diverging sample. Every position is >= 0 and strictly ascending, so
    // an embedder whose edit surface refuses negative or unordered points can still push this case
    // through unchanged. ---
    {
        const std::vector<PointSpec> spec {
            {0.0, 120.0, 0.0}, {960.0, 180.0, 2.0}, {2880.0, 90.0, -1.5}, {3840.0, 90.0, 0.0}};
        auto t = makePoints({{0.0, 120.0, 0.0}, {960.0, 180.0, 2.0}, {2880.0, 90.0, -1.5}, {3840.0, 90.0, 0.0}});
        fx.beginCase("bendMultiPointBothSigns", spec, 1e-9);
        std::vector<std::pair<double, double>> posQueries;
        for (double pos : {0.0, 480.0, 959.0, 960.0, 961.0, 1440.0, 1920.0, 2400.0, 2880.0, 3360.0, 3840.0, 4800.0})
            posQueries.emplace_back(pos, NA);
        fx.pos2time(t, posQueries);
        std::vector<std::pair<double, double>> timeQueries;
        for (const auto &q : posQueries)
            timeQueries.emplace_back(pos2Time(t, q.first), NA);
        fx.time2pos(t, timeQueries);
        fx.endCase();
    }

    if (g_die) {
        std::fprintf(stderr, "gen_fixture: refusing to emit — core disagrees with the analytic oracle.\n");
        return 1;
    }

    std::printf("{\n");
    std::printf("  \"_comment\": \"Language-neutral shared fixture for acestudio-tempo-map-calc-core. Generated by tools/gen_fixture.cpp from the CANONICAL C++ core. Expected values are full-precision engine outputs (constant-tempo cases also oracle-validated). Both the native test and the WASM/Node parity harness assert against this within each case's tolerance. Do not hand-edit — regenerate.\",\n");
    std::printf("  \"resolution\": %d,\n", RESOLUTION);
    std::printf("  \"defaultTempo\": %s,\n", num(DEFAULT_TEMPO).c_str());
    std::printf("  \"cases\": [\n%s\n  ]\n}\n", fx.out.c_str());
    return 0;
}
