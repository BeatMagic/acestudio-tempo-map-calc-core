// run_fixture (native) — drives the canonical C++ core over the shared fixture (embedded via
// the generated header) and:
//   1. asserts every scalar query matches the fixture's expected value within the case tolerance
//      (the golden is the canonical engine's own output, so this pins native behavior);
//   2. runs the same inputs (sorted) through the bulk range path and asserts it agrees with the
//      scalar path; and
//   3. emits both the scalar and the bulk actual outputs as JSON to argv[1], so the WASM/Node
//      harness can assert the WASM build agrees with this native build on both paths.
//
// Plain int main() + manual check() (no <cassert>), so a Release/NDEBUG build cannot pass
// vacuously. Fixture data arrives through the codegen header — no runtime JSON parser here.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "TempoCurveCore.h"
#include "fixture_generated.h"

using namespace tempo_curve;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool cond, const std::string &what)
{
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}

std::vector<TempoPoint> build(const fixture::Case &c)
{
    std::vector<TempoPoint> pts;
    for (const auto &p : c.points) {
        TempoPoint tp;
        tp.pos = p.pos;
        tp.bpm = p.bpm;
        tp.bend = p.bend;
        tp.bendFactor = std::exp(p.bend);
        pts.push_back(tp);
    }
    recomputeTimes(pts);
    return pts;
}

std::string num(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

std::string jsonArray(const std::vector<double> &xs)
{
    std::string s = "[";
    for (std::size_t i = 0; i < xs.size(); ++i) {
        s += i ? ", " : "";
        s += num(xs[i]);
    }
    return s + "]";
}

} // namespace

int main(int argc, char **argv)
{
    std::printf("== run_fixture (native) ==\n");

    std::string actual = "{\n  \"cases\": [\n";
    bool firstCase = true;

    for (const auto &c : fixture::cases()) {
        const auto pts = build(c);

        // Scalar path: assert vs golden.
        std::vector<double> scalarP2T, scalarT2P;
        for (const auto &q : c.pos2time) {
            const double got = pos2Time(pts, q.in);
            check(std::abs(got - q.expected) <= c.tolerance, std::string(c.name) + " pos2Time(" + num(q.in) + ")");
            scalarP2T.push_back(got);
        }
        for (const auto &q : c.time2pos) {
            const double got = time2Pos(pts, q.in);
            check(std::abs(got - q.expected) <= c.tolerance, std::string(c.name) + " time2Pos(" + num(q.in) + ")");
            scalarT2P.push_back(got);
        }

        // Bulk range path: the range API requires inputs sorted ascending, so sort first. Assert
        // the bulk result agrees with the scalar path for each input, then emit it for parity.
        std::vector<double> bulkPosIn;
        for (const auto &q : c.pos2time)
            bulkPosIn.push_back(q.in);
        std::sort(bulkPosIn.begin(), bulkPosIn.end());
        std::vector<double> bulkPosOut(bulkPosIn.size());
        pos2TimeRange(pts, bulkPosIn, bulkPosOut);
        for (std::size_t i = 0; i < bulkPosIn.size(); ++i)
            check(std::abs(bulkPosOut[i] - pos2Time(pts, bulkPosIn[i])) <= c.tolerance,
                  std::string(c.name) + " bulk pos2TimeRange == scalar");

        std::vector<double> bulkTimeIn;
        for (const auto &q : c.time2pos)
            bulkTimeIn.push_back(q.in);
        std::sort(bulkTimeIn.begin(), bulkTimeIn.end());
        std::vector<double> bulkTimeOut(bulkTimeIn.size());
        time2PosRange(pts, bulkTimeIn, bulkTimeOut);
        for (std::size_t i = 0; i < bulkTimeIn.size(); ++i)
            check(std::abs(bulkTimeOut[i] - time2Pos(pts, bulkTimeIn[i])) <= c.tolerance,
                  std::string(c.name) + " bulk time2PosRange == scalar");

        actual += firstCase ? "" : ",\n";
        firstCase = false;
        actual += "    {\"name\": \"" + std::string(c.name) + "\"";
        actual += ", \"pos2time\": " + jsonArray(scalarP2T);
        actual += ", \"time2pos\": " + jsonArray(scalarT2P);
        actual += ", \"bulk_pos2time_in\": " + jsonArray(bulkPosIn);
        actual += ", \"bulk_pos2time_out\": " + jsonArray(bulkPosOut);
        actual += ", \"bulk_time2pos_in\": " + jsonArray(bulkTimeIn);
        actual += ", \"bulk_time2pos_out\": " + jsonArray(bulkTimeOut);
        actual += "}";
    }
    actual += "\n  ]\n}\n";

    if (argc > 1) {
        if (FILE *out = std::fopen(argv[1], "w")) {
            std::fputs(actual.c_str(), out);
            std::fclose(out);
            std::printf("wrote actual outputs to %s\n", argv[1]);
        } else {
            std::printf("  WARN: could not open %s for writing\n", argv[1]);
        }
    }

    std::printf("%d/%d checks passed\n", g_checks - g_failures, g_checks);
    if (g_failures) {
        std::printf("RESULT: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::printf("RESULT: PASS\n");
    return 0;
}
