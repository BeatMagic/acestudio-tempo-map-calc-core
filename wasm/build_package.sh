#!/usr/bin/env bash
# Build the @timedomain/acestudio-tempo-map-calc npm artifact from the core sources with stock,
# version-pinned Emscripten. Emits the prebuilt WASM + ES module glue + TypeScript types into
# dist/ (gitignored) — this is what ships inside the npm tarball, so downstream consumers need no
# C++/Emscripten toolchain. Distinct from wasm/build.sh, which builds the internal parity harness.
#
# Usage: bash wasm/build_package.sh [out-dir]   (default out-dir: dist)
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${1:-$here/dist}"
mkdir -p "$out"

emcc -std=c++23 -O3 \
    -I"$here/include" \
    "$here/wasm/tempo_embind.cpp" \
    "$here/src/TempoCurveCore.cpp" \
    -lembind \
    -o "$out/tempo_map_calc.mjs" \
    -sMODULARIZE=1 \
    -sEXPORT_ES6=1 \
    -sEXPORT_NAME=createTempoMapCalc \
    -sENVIRONMENT='web,worker,node' \
    -sALLOW_MEMORY_GROWTH=1 \
    -sFILESYSTEM=0

# Ship the hand-authored typed contract (precise where --emit-tsd would emit `any`).
cp "$here/types/tempo_map_calc.d.ts" "$out/tempo_map_calc.d.ts"

echo "built $out/tempo_map_calc.{mjs,wasm,d.ts}"
