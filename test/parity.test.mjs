// Node parity test for the @timedomain/acestudio-tempo-map-calc package. Loads the prebuilt WASM
// module (dist/, built by wasm/build_package.sh) and drives its frozen public API — hydrate, the
// scalar pos2Time/time2Pos path, and the zero-copy typed-array bulk path — over the shared fixture
// (fixtures/tempo_curve_cases.json, the canonical full-precision core outputs). Every result is
// asserted within each case's declared tolerance, matching the fixture's design and the repo's
// WASM<->C++ parity harness: cross-arch/cross-libm last-ULP differences (the curved exp/log cases)
// are acceptable; behavioral drift is not.

import { readFileSync } from "node:fs";
import { fileURLToPath, pathToFileURL } from "node:url";
import { dirname, join } from "node:path";
import { test, before } from "node:test";
import assert from "node:assert/strict";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..");

const fixture = JSON.parse(
  readFileSync(join(root, "fixtures", "tempo_curve_cases.json"), "utf8"),
);

let createTempoMapCalc;
before(async () => {
  const modulePath = join(root, "dist", "tempo_map_calc.mjs");
  createTempoMapCalc = (await import(pathToFileURL(modulePath).href)).default;
});

// A hydrated calc for one fixture case, from its parallel control-point arrays.
async function hydrate(points) {
  const mod = await createTempoMapCalc();
  const calc = new mod.TempoMapCalc();
  calc.hydrate(
    points.map((p) => p.pos),
    points.map((p) => p.bpm),
    points.map((p) => p.bend),
  );
  return calc;
}

// Run one direction of the zero-copy bulk path: fill the input view, convert, return the output as
// a plain array. Inputs must be sorted ascending (core precondition).
function runBulk(calc, inputs, convert) {
  const input = calc.inputView(inputs.length);
  input.set(inputs);
  calc[convert]();
  return Array.from(calc.outputView());
}

// One direction of a fixture case: scalar and bulk each asserted against the expected values.
function checkDirection(calc, tol, entries, inKey, outKey, scalarFn, convert, label) {
  const sorted = [...entries].sort((a, b) => a[inKey] - b[inKey]);
  const bulk = sorted.length > 0 ? runBulk(calc, sorted.map((e) => e[inKey]), convert) : [];
  sorted.forEach((e, i) => {
    const scalar = calc[scalarFn](e[inKey]);
    assert.ok(
      Math.abs(scalar - e[outKey]) <= tol,
      `${label}(${e[inKey]}) scalar = ${scalar}, expected ${e[outKey]} (tol ${tol})`,
    );
    assert.ok(
      Math.abs(bulk[i] - e[outKey]) <= tol,
      `${label}[${i}] bulk = ${bulk[i]}, expected ${e[outKey]} (tol ${tol})`,
    );
  });
}

for (const c of fixture.cases) {
  test(`${c.name}: scalar + zero-copy bulk paths within tolerance`, async () => {
    const calc = await hydrate(c.points);
    try {
      checkDirection(calc, c.tolerance, c.pos2time ?? [], "pos", "time", "pos2Time", "convertPos2Time", "pos2Time");
      checkDirection(calc, c.tolerance, c.time2pos ?? [], "time", "pos", "time2Pos", "convertTime2Pos", "time2Pos");
    } finally {
      calc.delete();
    }
  });
}

// Large-volume bulk run: the zero-copy path exists for curves thousands of points long (ADR 0085's
// 100k-frame resampling), a scale the small fixture cases never reach. Drive the bulk path over a
// curved map and assert it agrees with the scalar path *exactly* — both call the same core in the
// same WASM binary, so there is no cross-libm slack here; a mismatch means the bulk plumbing (heap
// views, growth, indexing) is broken. A second inputView() call then re-fetches the views (the
// documented "get a fresh view each cycle" contract), and a round-trip recovers the inputs within
// tolerance.
test("large-volume bulk path matches the scalar path exactly and round-trips", async () => {
  const calc = await hydrate([
    { pos: 480, bpm: 120, bend: 0 },
    { pos: 1440, bpm: 180, bend: 0 },
    { pos: 2400, bpm: 90, bend: 0.7 },
  ]);
  try {
    const n = 100_000;
    const positions = Array.from({ length: n }, (_, i) => i * 3); // sorted ascending, spans the curve

    const times = runBulk(calc, positions, "convertPos2Time");
    for (let i = 0; i < n; i++) {
      assert.equal(times[i], calc.pos2Time(positions[i]), `bulk vs scalar diverged at pos ${positions[i]}`);
    }

    // Re-fetch the views at a larger size (may move the heap) and convert back; round-trip recovers
    // the positions within a tick's slack.
    const back = runBulk(calc, times, "convertTime2Pos");
    for (let i = 0; i < n; i++) {
      assert.ok(
        Math.abs(back[i] - positions[i]) <= 1e-6,
        `round-trip at index ${i}: ${back[i]} vs ${positions[i]}`,
      );
    }
  } finally {
    calc.delete();
  }
});

test("hydrate clamps to the shortest of the three arrays (no over-read)", async () => {
  const mod = await createTempoMapCalc();
  const calc = new mod.TempoMapCalc();
  try {
    // Two full points' worth of pos, but only one bpm/bend: only the first point is usable. A
    // single point at 120 BPM makes tick 480 land at 0.5s; an over-read would corrupt that.
    calc.hydrate([0, 480], [120], [0]);
    assert.ok(Math.abs(calc.pos2Time(480) - 0.5) <= 1e-7);
  } finally {
    calc.delete();
  }
});
