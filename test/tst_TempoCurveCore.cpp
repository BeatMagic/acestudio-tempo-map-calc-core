// Analytic unit test for TempoCurveCore. It drives the std::span conversion API directly against
// an independent closed-form oracle (elapsed time at a constant tempo), covering the lead-in,
// straddling, all-negative, empty, curved, bent and corrupt-data cases. Bent segments have no
// closed form, so they are pinned by the properties that do not need one: the equal-endpoint case
// where bend must NOT curve anything (and the oracle still applies), monotonicity and invertibility
// across both signs, and each sign deviating from the bend-free curve in its own direction. Because this target compiles
// TempoCurveCore.cpp with nothing extra on its include path, it also enforces the core's
// dependency-free constraint: a stray include fails the build.
//
// Deliberately plain int main() + a manual check() (not <cassert>), so a Release/NDEBUG build
// cannot pass the assertions vacuously.

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <utility>
#include <vector>

#include "TempoCurveCore.h"

using namespace tempo_curve;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool cond, const char *what)
{
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::printf("  FAIL: %s\n", what);
    }
}

constexpr double kR = 60.0 / RESOLUTION;
constexpr double kEps = 1e-7;

// time elapsed over `ticks` at a constant `bpm` — the independent oracle
double constTime(double ticks, double bpm)
{
    return ticks * kR / bpm;
}

// One control point, with `bend` defaulting to 0 so the bend-free cases read unchanged.
struct PointSpec
{
    double pos = 0.0;
    double bpm = 0.0;
    double bend = 0.0;
};

// Build points the way the core expects: bendFactor = exp(bend), then recomputeTimes().
// An empty list stays empty (the no-control-points fallback).
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

void firstPointAtZero()
{
    auto t = makePoints({{0.0, 120.0}});
    check(std::abs(pos2Time(t, 0.0) - 0.0) < kEps, "firstPointAtZero pos2Time(0)=0");
    check(std::abs(pos2Time(t, 480.0) - 0.5) < kEps, "firstPointAtZero pos2Time(480)=0.5");
    check(std::abs(pos2Time(t, 960.0) - 1.0) < kEps, "firstPointAtZero pos2Time(960)=1.0");
    check(std::abs(time2Pos(t, 0.5) - 480.0) < kEps, "firstPointAtZero time2Pos(0.5)=480");
    check(std::abs(time2Pos(t, 0.0) - 0.0) < kEps, "firstPointAtZero time2Pos(0)=0");
}

void firstPointPositive()
{
    auto t = makePoints({{480.0, 120.0}});
    check(std::abs(pos2Time(t, 0.0) - 0.0) < kEps, "firstPointPositive anchor");
    check(std::abs(pos2Time(t, 240.0) - 0.25) < kEps, "firstPointPositive lead-in 240=0.25");
    check(std::abs(pos2Time(t, 480.0) - 0.5) < kEps, "firstPointPositive at-point 480=0.5");
    check(std::abs(pos2Time(t, 960.0) - 1.0) < kEps, "firstPointPositive after 960=1.0");
    check(pos2Time(t, 479.0) < pos2Time(t, 480.0), "firstPointPositive continuity <");
    check(pos2Time(t, 480.0) < pos2Time(t, 481.0), "firstPointPositive continuity >");
    check(std::abs(time2Pos(t, 0.0) - 0.0) < kEps, "firstPointPositive inv anchor");
    check(std::abs(time2Pos(t, 0.5) - 480.0) < kEps, "firstPointPositive inv 0.5=480");
}

void firstPointNegativeStraddling()
{
    auto t = makePoints({{-480.0, 120.0}, {480.0, 120.0}});
    check(std::abs(pos2Time(t, 0.0) - 0.0) < kEps, "straddling anchor");
    check(std::abs(pos2Time(t, 480.0) - constTime(480.0, 120.0)) < kEps, "straddling +0.5");
    check(std::abs(pos2Time(t, -480.0) - constTime(-480.0, 120.0)) < kEps, "straddling -0.5");
    check(std::abs(pos2Time(t, -960.0) - constTime(-960.0, 120.0)) < kEps, "straddling -1.0");
    check(std::abs(time2Pos(t, 0.0) - 0.0) < kEps, "straddling inv anchor");
}

void allPointsNegative()
{
    auto t = makePoints({{-960.0, 120.0}, {-480.0, 120.0}});
    check(std::abs(pos2Time(t, 0.0) - 0.0) < kEps, "allNeg anchor");
    check(std::abs(pos2Time(t, 480.0) - constTime(480.0, 120.0)) < kEps, "allNeg +0.5");
    check(std::abs(pos2Time(t, -480.0) - constTime(-480.0, 120.0)) < kEps, "allNeg -0.5");
}

void roundTripAndMonotonic()
{
    auto t = makePoints({{480.0, 120.0}, {1440.0, 180.0}, {2400.0, 90.0}});
    check(std::abs(pos2Time(t, 0.0) - 0.0) < kEps, "roundTrip anchor");
    double prev = pos2Time(t, -600.0);
    for (double pos = -600.0; pos <= 3000.0; pos += 37.0) {
        const double time = pos2Time(t, pos);
        check(time >= prev - kEps, "roundTrip monotonic");
        prev = time;
        check(std::abs(time2Pos(t, time) - pos) < 1e-4, "roundTrip pos->time->pos");
    }
}

void emptyListFallback()
{
    std::vector<TempoPoint> t; // empty: no control points
    check(std::abs(pos2Time(t, 0.0) - 0.0) < kEps, "empty anchor");
    check(std::abs(pos2Time(t, 480.0) - constTime(480.0, DEFAULT_TEMPO)) < kEps, "empty default 480");
    check(std::abs(time2Pos(t, constTime(480.0, DEFAULT_TEMPO)) - 480.0) < kEps, "empty inv");
}

void nonPositiveBpmStaysFinite()
{
    auto t = makePoints({{0.0, 120.0}, {480.0, 0.0}});
    for (double pos = -240.0; pos <= 1440.0; pos += 60.0) {
        const double time = pos2Time(t, pos);
        check(std::isfinite(time), "corrupt-bpm pos2Time finite");
        check(std::isfinite(time2Pos(t, time)), "corrupt-bpm time2Pos finite");
    }
    check(std::abs(pos2Time(t, 0.0) - 0.0) < kEps, "corrupt-bpm anchor");

    auto tf = makePoints({{480.0, 0.0}, {960.0, 120.0}});
    double prevPos = time2Pos(tf, -1.0);
    for (double time = -1.0; time <= 1.0; time += 0.1) {
        const double pos = time2Pos(tf, time);
        check(std::isfinite(pos), "corrupt-first time2Pos finite");
        check(std::isfinite(pos2Time(tf, pos)), "corrupt-first pos2Time finite");
        check(pos >= prevPos - kEps, "corrupt-first non-decreasing");
        prevPos = pos;
    }
}

// `bend` shapes the ramp leaving a point (bendFactor = exp(bend)). Equal bpm at both ends means
// there is no ramp to shape, so the constant-tempo closed form must still hold — the one bend case
// with an independent oracle. It also pins the precedence: checking for equal endpoints has to come
// before applying bend, or a segment that must stay straight gets curved.
void bendWithEqualBpmStaysConstant()
{
    for (const double bend : {-2.0, -0.5, 0.5, 2.0}) {
        auto t = makePoints({{0.0, 120.0, bend}, {960.0, 120.0, 0.0}});
        check(std::abs(pos2Time(t, 0.0) - 0.0) < kEps, "bend equal-bpm anchor");
        check(std::abs(pos2Time(t, 240.0) - constTime(240.0, 120.0)) < kEps, "bend equal-bpm 240");
        check(std::abs(pos2Time(t, 480.0) - constTime(480.0, 120.0)) < kEps, "bend equal-bpm 480");
        check(std::abs(pos2Time(t, 1440.0) - constTime(1440.0, 120.0)) < kEps, "bend equal-bpm tail");
        check(std::abs(time2Pos(t, constTime(480.0, 120.0)) - 480.0) < kEps, "bend equal-bpm inverse");
    }
}

// A curved segment must still behave like a tempo map: anchored at tick 0, monotonic, and
// invertible. Asserted across both signs of bend, since a sign error is the easiest way to get a
// curve that looks plausible and inverts wrongly.
void bendStaysMonotonicAndInvertible()
{
    for (const double bend : {-2.0, -1.5, -0.5, 0.5, 1.5, 2.0}) {
        auto t = makePoints({{0.0, 120.0, bend}, {1920.0, 180.0, -bend}, {3840.0, 90.0, 0.0}});
        check(std::abs(pos2Time(t, 0.0) - 0.0) < kEps, "bend anchor");
        double prev = pos2Time(t, -600.0);
        for (double pos = -600.0; pos <= 4800.0; pos += 41.0) {
            const double time = pos2Time(t, pos);
            check(std::isfinite(time), "bend pos2Time finite");
            check(time >= prev - kEps, "bend monotonic");
            prev = time;
            check(std::abs(time2Pos(t, time) - pos) < 1e-4, "bend pos->time->pos");
        }
    }
}

// bend must actually do something, and the two signs must do opposite things — otherwise a consumer
// could drop the field, or flip its sign, and still match. Over a rising segment the bend=0 log ramp
// sits strictly between the two exponential shapes.
void bendChangesTheCurveBySign()
{
    auto straight = makePoints({{0.0, 120.0, 0.0}, {1920.0, 180.0, 0.0}});
    auto positive = makePoints({{0.0, 120.0, 2.0}, {1920.0, 180.0, 0.0}});
    auto negative = makePoints({{0.0, 120.0, -2.0}, {1920.0, 180.0, 0.0}});

    // Interior samples only: tick 0 is pinned for every curve, so it can never show a difference.
    for (const double pos : {480.0, 960.0, 1440.0, 1920.0}) {
        const double s = pos2Time(straight, pos);
        const double p = pos2Time(positive, pos);
        const double n = pos2Time(negative, pos);
        check(std::abs(p - s) > kEps, "positive bend changes the curve");
        check(std::abs(n - s) > kEps, "negative bend changes the curve");
        check((p - s) * (n - s) < 0.0, "the two bend signs deviate in opposite directions");
    }
}

// Bulk conversion over the std::span range API: assert the bulk path agrees with the
// single-value path and round-trips, on a realistic varying-tempo curve at a 100k-point scale.
void bulkRangeMatchesSingleValue()
{
    auto pts = makePoints({{0.0, 120.0}, {19200.0, 140.0}, {38400.0, 90.0}, {57600.0, 160.0}, {76800.0, 120.0}});

    constexpr int N = 100000;
    std::vector<double> positions(N), times(N), back(N);
    for (int i = 0; i < N; ++i) {
        positions[i] = static_cast<double>(i) * (96000.0 / N);
    }

    pos2TimeRange(pts, positions, times);
    time2PosRange(pts, times, back);

    bool bulkMatchesSingle = true;
    bool roundTrips = true;
    for (int i = 0; i < N; ++i) {
        if (std::abs(times[i] - pos2Time(pts, positions[i])) > kEps) {
            bulkMatchesSingle = false;
        }
        if (std::abs(back[i] - positions[i]) > 1e-4) {
            roundTrips = false;
        }
    }
    check(bulkMatchesSingle, "bulk pos2TimeRange matches single-value pos2Time");
    check(roundTrips, "bulk pos->time->pos round-trips");
}

} // namespace

int main()
{
    std::printf("== tst_TempoCurveCore ==\n");
    firstPointAtZero();
    firstPointPositive();
    firstPointNegativeStraddling();
    allPointsNegative();
    roundTripAndMonotonic();
    emptyListFallback();
    nonPositiveBpmStaysFinite();
    bendWithEqualBpmStaysConstant();
    bendStaysMonotonicAndInvertible();
    bendChangesTheCurveBySign();
    bulkRangeMatchesSingleValue();

    std::printf("%d/%d checks passed\n", g_checks - g_failures, g_checks);
    if (g_failures) {
        std::printf("RESULT: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::printf("RESULT: PASS\n");
    return 0;
}
