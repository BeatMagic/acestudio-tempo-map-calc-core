# Porting to another language

The core is small and dependency-free on purpose: it reads as a specification. A port is the two
conversion functions plus a fixture run to prove it.

## 1. Reimplement the two conversions

Follow [`src/TempoCurveCore.cpp`](../src/TempoCurveCore.cpp): `pos2Time` and `time2Pos`, plus the
range variants if you want the batch path. The per-segment integrators (`getTimeRelativeTo` and
`getPosRelativeTo`) are the whole algorithm, and [the tempo model](tempo-model.md) explains which case
applies when.

## 2. Validate against the shared fixture

[`fixtures/tempo_curve_cases.json`](../fixtures/tempo_curve_cases.json) is language-neutral: build
each case's points, convert, and assert your outputs match the expected values within that case's
`tolerance`. The numbers are the core's own full-precision outputs, so matching them means agreeing
with ACE Studio's tempo math.

[`wasm/run_fixture.mjs`](../wasm/run_fixture.mjs) is a working example of a non-C++ consumer driving
the fixture.

## 3. Don't drop `bend`

The `bend*` cases exist to catch a port that loses the field or flips its sign — the easiest mistake
to make here, and the hardest to notice.

`bend` changes the *integral* over a bent segment, not merely its shape. So dropping it moves the
interior of that segment **and** the cached time at the control point closing it — and therefore
every control point downstream. Only tick 0, the origin `recomputeTimes()` anchors, is invariant.

What still looks healthy without `bend`, which is why the gap is easy to miss: every constant-tempo
case, and every segment whose **leading** point carries `bend == 0` (bend shapes the ramp *leaving* a
point, so that is the field consulted for the segment). A smoke test over a simple map can pass
while bend is entirely unimplemented.

`bendEqualBpmStaysConstant` pins the other half of the rule: when both endpoints carry the same BPM
there is no ramp to shape, so `bend` must not curve anything. Check for equal endpoints *before*
applying bend, or you will curve a segment that has to stay straight.
