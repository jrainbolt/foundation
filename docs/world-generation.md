# Deterministic world generation

`factory_world_generate` is an initialization-only, transactional population
step. It receives an allocated seeded world and a compact
`FactoryWorldGenerationConfig`, builds terrain and deposits in temporary tile
storage, validates the result, then swaps it into the world. Invalid settings,
unsatisfiable small maps, and allocation failures leave the original world
unchanged. A world attached to a simulation is sealed and rejects generation.

## Integer spatial field

Generator version 1 uses the SplitMix64 finalizer with explicitly wrapped
`uint64_t` arithmetic. Coordinates and independent channel constants are mixed
with the seed. Terrain therefore does not depend on traversal order and adding
a later channel need not shift existing channels.

Water and Rock each use a separate coordinate field. Four hashed 16-bit values
at the corners of a configurable coarse grid are interpolated with integer
weights. Thresholding these smoothly changing fields produces coherent bodies
instead of independent-cell noise. Water takes precedence, then Rock, with
Ground dominant under the defaults. No floating point or external noise
library is used.

## Start and resource guarantees

The start is the integer center `(width / 2, height / 2)`. A configured circular
core is forced to Ground. Starter Iron and Copper centers are selected from
seeded directions on a ring outside that core. Their access corridors and
irregular patch cells are Ground, patches cannot overlap, and every deposit has
a positive finite quantity. Starter quantity defaults to 500 units plus bounded
per-cell variation.

Additional bounded-attempt remote patches alternate Iron and Copper. Their
base quantity is 900 units plus `Manhattan distance / (start radius + 1)` times
the configured distance bonus, with checked saturation at `UINT32_MAX`.
Patches are generation concepts only: runtime ownership remains one finite
quantity per deposit tile. No patch list or PRNG state is retained.

## Authority and versions

The public FNV-1a checksum helper hashes seed, dimensions, and every authoritative
tile field using explicit little-endian bytes. It is intended for deterministic
regression diagnostics, not simulation decisions. Generator version 1 is
distinct from snapshot version 18. Snapshots store the resulting terrain and
deposits and never rerun generation, so changing future generation algorithms
does not change saved worlds.

This geography establishes finite nearby resources followed by richer remote
patches. It prepares expansion pressure for later outposts and rail logistics;
it adds no train, rail, station, chunk, or routing state.
