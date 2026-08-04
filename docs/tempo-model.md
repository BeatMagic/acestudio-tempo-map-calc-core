# The tempo model

Defines what a tempo map is in this core and what each value means. Both the
[C++](cpp-api.md) and [JavaScript](javascript-api.md) API references build on the terms established
here.

## Control points

A tempo map is a list of **control points**, sorted strictly ascending by position. Each point
carries:

| field | meaning |
| --- | --- |
| `pos` | position, in **ticks** |
| `bpm` | tempo at this point |
| `bend` | shapes the ramp *leaving* this point |
| `bendFactor` | `exp(bend)`, precomputed by the caller |
| `time` | cached cumulative time, filled in by `recomputeTimes()` |

## Ticks

Position is measured in ticks. `RESOLUTION` (480) is ACE Studio's ticks per quarter note (TPQN). At a
constant tempo `B`:

```
seconds = ticks * (60 / RESOLUTION) / B
```

## Between two adjacent points

Elapsed time over a segment is the integral of `1 / tempo`. Which interpolation applies depends on
the segment's **leading** point:

- **Equal BPM at both ends** → constant tempo, so time is linear in ticks. This case takes
  precedence over `bend`: there is no ramp to shape, so a non-zero `bend` must not curve anything.
- **`bend == 0`** → BPM ramps **linearly** in ticks, which makes elapsed time a **logarithmic**
  function of position.
- **`bend != 0`** → BPM follows an **exponential** ramp shaped by `bend` (`bendFactor = exp(bend)`).

`bend` changes the *integral* over the segment, not merely its shape, so it also changes how long
the segment takes, and therefore the cached time of every control point after it. That has direct
consequences for anyone reimplementing this; see [porting](porting.md#3-dont-drop-bend).

## Outside the control points

Before the first point and after the last, tempo is constant at that end point's BPM, extrapolated
linearly. With no control points at all, the map is a constant `DEFAULT_TEMPO` of 120 BPM.

## Anchoring the time axis

`recomputeTimes()` caches the cumulative time at each control point, then shifts the whole axis so
**tick 0 maps to time 0**. The first control point does not have to sit at tick 0. That anchor is the
one position no curve shape can move.

## Where the math lives

`pos2Time` and `time2Pos` are analytic inverses of each other, per segment. The closed forms for
every case are in [`src/TempoCurveCore.cpp`](../src/TempoCurveCore.cpp), in `getTimeRelativeTo` and
`getPosRelativeTo`. That file is the one to read if you are porting.
