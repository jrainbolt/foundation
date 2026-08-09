# Deterministic procedural world model

Foundation worlds have an explicit 64-bit seed and a bounded, authoritative
row-major terrain grid. `factory_world_create_with_seed` records a caller-owned
seed; the original constructor remains a compatibility wrapper using
`FACTORY_WORLD_DEFAULT_SEED`. This milestone adds no random generator or
terrain-generation algorithm. A later generator can use the stable input tuple
`(seed, width, height)`. Generator version 1 now performs that initialization;
see [world-generation.md](world-generation.md) for the fixed integer algorithm.

## Terrain ownership and setup

Every new cell is Ground. The immutable content layer defines Ground, Water,
and Rock. Ground is buildable and accepts the current iron and copper deposits;
Water and Rock are non-buildable and reject ordinary deposits. Terrain None is
an invalid/sentinel lookup value.

`factory_world_initialize_terrain` is the sanctioned setup path. It accepts
only known terrain and pristine cells. Creating a simulation seals its borrowed
world, after which setup returns `FACTORY_RESULT_INVALID_STATE`. There is no
gameplay terrain-mutation command.

## Construction and resources

Construction centrally validates the entity definition's complete footprint.
Validation order is: known/unlocked entity, bounds, row-major terrain,
row-major occupancy, construction inventory, then subsystem validation and
commit. Blocked terrain returns `FACTORY_RESULT_TERRAIN_BLOCKED` without
spending inventory, creating an entity, or emitting a construction event.
Deposit creation consults the same immutable terrain definition.

Current entity footprints are one cell, but checked footprint validation is
ready for larger immutable definitions.

## Snapshots and presentation

Snapshot version 18 stores the seed and every terrain cell canonically in
row-major order. Load restores stored state and never regenerates it. Validation
rejects unknown terrain and terrain/resource incompatibility. Presentation
exports one read-only terrain record per cell in row-major order. Godot renders
these beneath resources, networks, and entities and uses buildability only for
advisory preview; the engine remains authoritative.

The demo uses generated coherent Water, Rock, and finite resource patches,
while retaining its deterministic systems factory inside the protected core.
Movement costs, biomes, chunks, hidden generation state, and renderer assets
remain deliberately deferred.
