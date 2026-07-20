// WASM<->C++ parity harness. Loads the Emscripten build of the SAME core sources, runs it over
// the language-neutral shared fixture on BOTH the scalar and the bulk-range paths, and asserts
// the WASM build agrees with the fixture's canonical expected values AND with the native build's
// actual outputs — within each case's tolerance (not bit-for-bit: cross-arch/WASM last-ULP
// differences are acceptable; a real behavioral drift is not). Exits non-zero on any failure.
//
// Usage: node run_fixture.mjs <module.mjs> <fixture.json> <native.json>

import { readFileSync } from "node:fs";
import { pathToFileURL } from "node:url";

const [, , modulePath, fixturePath, nativePath] = process.argv;
if (!modulePath || !fixturePath || !nativePath) {
  console.error("usage: node run_fixture.mjs <module.mjs> <fixture.json> <native.json>");
  process.exit(2);
}

const factory = (await import(pathToFileURL(modulePath).href)).default;
const wasm = await factory();

const tc = {
  reset: wasm.cwrap("tc_reset", null, []),
  addPoint: wasm.cwrap("tc_add_point", null, ["number", "number", "number"]),
  recompute: wasm.cwrap("tc_recompute", null, []),
  pos2time: wasm.cwrap("tc_pos2time", "number", ["number"]),
  time2pos: wasm.cwrap("tc_time2pos", "number", ["number"]),
  pos2timeRange: wasm.cwrap("tc_pos2time_range", null, ["number", "number", "number"]),
  time2posRange: wasm.cwrap("tc_time2pos_range", null, ["number", "number", "number"]),
};

// Run a bulk-range conversion over the WASM heap: copy inputs in, convert, read outputs back.
function runRange(fn, inputs) {
  const n = inputs.length;
  if (n === 0) return [];
  const inPtr = wasm._malloc(n * 8);
  const outPtr = wasm._malloc(n * 8);
  wasm.HEAPF64.set(Float64Array.from(inputs), inPtr / 8);
  fn(inPtr, outPtr, n);
  const out = Array.from(wasm.HEAPF64.subarray(outPtr / 8, outPtr / 8 + n));
  wasm._free(inPtr);
  wasm._free(outPtr);
  return out;
}

const fixture = JSON.parse(readFileSync(fixturePath, "utf8"));
const native = JSON.parse(readFileSync(nativePath, "utf8"));
const nativeByName = new Map(native.cases.map((c) => [c.name, c]));

let checks = 0;
let failures = 0;
const fail = (msg) => {
  failures++;
  console.log(`  FAIL: ${msg}`);
};
const near = (a, b, tol, msg) => {
  checks++;
  if (Math.abs(a - b) > tol) fail(`${msg}: ${a} vs ${b} (tol ${tol})`);
};

for (const c of fixture.cases) {
  const tol = c.tolerance;
  const nat = nativeByName.get(c.name);
  if (!nat) {
    fail(`${c.name}: missing from native outputs`);
    continue;
  }

  tc.reset();
  for (const p of c.points) tc.addPoint(p.pos, p.bpm, p.bend);
  tc.recompute();

  // Scalar path: wasm vs expected AND wasm vs native.
  const p2t = c.pos2time ?? [];
  for (let i = 0; i < p2t.length; i++) {
    const got = tc.pos2time(p2t[i].pos);
    near(got, p2t[i].time, tol, `${c.name} pos2time(${p2t[i].pos}) wasm/expected`);
    near(got, nat.pos2time[i], tol, `${c.name} pos2time(${p2t[i].pos}) wasm/native`);
  }
  const t2p = c.time2pos ?? [];
  for (let i = 0; i < t2p.length; i++) {
    const got = tc.time2pos(t2p[i].time);
    near(got, t2p[i].pos, tol, `${c.name} time2pos(${t2p[i].time}) wasm/expected`);
    near(got, nat.time2pos[i], tol, `${c.name} time2pos(${t2p[i].time}) wasm/native`);
  }

  // Bulk range path: feed the exact inputs native used (already sorted ascending) and assert the
  // WASM range output agrees with the native range output element-for-element.
  const bp = runRange(tc.pos2timeRange, nat.bulk_pos2time_in);
  for (let i = 0; i < bp.length; i++)
    near(bp[i], nat.bulk_pos2time_out[i], tol, `${c.name} bulk pos2TimeRange[${i}] wasm/native`);
  const bt = runRange(tc.time2posRange, nat.bulk_time2pos_in);
  for (let i = 0; i < bt.length; i++)
    near(bt[i], nat.bulk_time2pos_out[i], tol, `${c.name} bulk time2PosRange[${i}] wasm/native`);
}

console.log(`== run_fixture (wasm) ==`);
console.log(`${checks - failures}/${checks} checks passed (scalar + bulk range; wasm vs expected AND wasm vs native)`);
if (failures) {
  console.log(`RESULT: FAIL (${failures})`);
  process.exit(1);
}
console.log("RESULT: PASS");
