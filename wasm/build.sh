#!/usr/bin/env bash
# Build the WASM parity module from the same core sources, with stock Emscripten.
# Output: <out-dir>/tempo_wasm.mjs (+ .wasm), an ES6 module loaded by wasm/run_fixture.mjs.
# This is the repo's own parity build, not a production artifact.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${1:-$here/build-wasm}"
mkdir -p "$out"

emcc -std=c++23 -O2 \
    -I"$here/include" \
    "$here/wasm/tempo_wasm.cpp" \
    "$here/src/TempoCurveCore.cpp" \
    -o "$out/tempo_wasm.mjs" \
    -sMODULARIZE=1 \
    -sEXPORT_ES6=1 \
    -sENVIRONMENT=node \
    -sEXPORTED_FUNCTIONS='["_tc_reset","_tc_add_point","_tc_recompute","_tc_pos2time","_tc_time2pos","_tc_pos2time_range","_tc_time2pos_range","_malloc","_free"]' \
    -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPF64"]'

echo "built $out/tempo_wasm.mjs"
