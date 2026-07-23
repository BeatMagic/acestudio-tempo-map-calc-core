// Frozen public API for @timedomain/acestudio-tempo-map-calc (ADR 0086 addendum).
//
// Hand-authored typed contract shipped alongside the prebuilt WASM module. The .wasm and .mjs are
// build artifacts (regenerated in release CI, never committed); this contract is source, kept in
// step with wasm/tempo_embind.cpp — the surface is frozen, so it does not track the binding.

/** A hydrated tempo map. Convert between tick positions and wall-clock seconds against it. */
export interface TempoMapCalc {
  /**
   * Rebuild the tempo map from a revision-stamped snapshot's three parallel arrays: control-point
   * positions (ticks), tempos (BPM), and ramp-shaping bend values. Clamps to the shortest array,
   * so a partial or mismatched snapshot can never over-read. Points must be sorted strictly
   * ascending by position. Call before converting; call again to re-hydrate after a tempo change.
   */
  hydrate(pos: ArrayLike<number>, bpm: ArrayLike<number>, bend: ArrayLike<number>): void;

  /** Convert a single tick position to wall-clock seconds. Non-allocating. */
  pos2Time(pos: number): number;
  /** Convert a single wall-clock time (seconds) to a tick position. Non-allocating. */
  time2Pos(time: number): number;

  /**
   * Size the shared bulk buffers to `n` and return a Float64Array aliasing the WASM heap for the
   * caller to fill with inputs (sorted ascending). The view is valid until the next `inputView`
   * call; fill it, call a `convert*`, then read `outputView` before allocating on the WASM heap.
   */
  inputView(n: number): Float64Array;
  /** Convert the filled input view (tick positions) to seconds, in place into the output buffer. */
  convertPos2Time(): void;
  /** Convert the filled input view (seconds) to tick positions, in place into the output buffer. */
  convertTime2Pos(): void;
  /** Float64Array aliasing the WASM heap with the results of the last `convert*` call. */
  outputView(): Float64Array;

  /** Release the underlying C++ object. Call when done to free WASM heap memory. */
  delete(): void;
}

/** The instantiated module: construct `TempoMapCalc` instances from it. */
export interface TempoMapCalcModule {
  TempoMapCalc: { new (): TempoMapCalc };
}

/** Instantiate the WASM module. Resolves once the module is ready to use. */
export default function createTempoMapCalc(options?: unknown): Promise<TempoMapCalcModule>;
