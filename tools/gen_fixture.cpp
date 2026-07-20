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
#include <utility>
#include <vector>

#include "TempoCurveCore.h"

using namespace tempo_curve;

namespace {

constexpr double kR = 60.0 / RESOLUTION;
constexpr double kEps = 1e-7;

double constTime(double ticks, double bpm) { return ticks * kR / bpm; }

std::vector<TempoPoint> makePoints(std::initializer_list<std::pair<double, double>> posBpm)
{
    std::vector<TempoPoint> pts;
    for (const auto &[pos, bpm] : posBpm) {
        TempoPoint p;
        p.pos = pos;
        p.bpm = bpm;
        p.bend = 0.0;
        p.bendFactor = std::exp(0.0);
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

    // posBpm: the case's control points (each bend is 0). Query methods take `t`, the same points
    // after makePoints() (bendFactor + recomputeTimes), so the emitted golden is the core output.
    void beginCase(const std::string &name, const std::vector<std::pair<double, double>> &posBpm, double tolerance)
    {
        if (!firstCase)
            out += ",\n";
        firstCase = false;
        out += "    {\n";
        out += "      \"name\": \"" + name + "\",\n";
        out += "      \"tolerance\": " + num(tolerance) + ",\n";
        out += "      \"points\": [";
        bool f = true;
        for (const auto &[pos, bpm] : posBpm) {
            out += f ? "" : ", ";
            f = false;
            out += "{\"pos\": " + num(pos) + ", \"bpm\": " + num(bpm) + ", \"bend\": 0}";
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
