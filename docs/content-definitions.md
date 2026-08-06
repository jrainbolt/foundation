# Immutable content definitions

Foundation separates immutable content from authoritative simulation state.
`include/foundation/content.h` is the renderer-neutral public boundary and
`src/content.c` is the single storage location for compile-time gameplay
definitions. Definitions have process lifetime, use stable integer IDs, never
allocate, and cannot be registered or modified at runtime.

The content view contains deterministic ordered tables for:

- entities and construction costs;
- refinery and assembler recipes;
- technologies, prerequisites, and unlock flags;
- burner fuels;
- fluids;
- nuclear fuels;
- steam-generation recipes;
- fluid-conversion and heat-exchange recipes;
- Steam Turbine and Steam Condenser definitions.

Every category provides count, indexed lookup, and stable-ID lookup. The main
categories also expose category validation, while `factory_content_validate`
and `factory_content_validate_view` perform the complete cross-category pass.
Indexed order is source order and is deterministic, but public identity is
always the explicit ID field rather than the index.

The validator rejects missing or empty tables, duplicate IDs, invalid items,
fluids and referenced definitions, zero costs and work quantities, invalid
entity roles, unknown unlocks, duplicate technology unlock flags, invalid or
cyclic prerequisites, and invalid construction/entity relationships. Default
content is validated whenever a simulation is created. A failed validation
prevents initialization; it cannot partially initialize simulation state.

Entity definitions describe stable entity type, construction cost, one-tile
footprint, optional orientation, required unlock flag, recipe family, and
power/fluid/heat roles. These records describe content only. They neither
replace components nor enforce unlocks. Unlock enforcement remains explicitly
deferred to the next milestone.

Historical APIs such as `factory_recipe_get`,
`factory_fuel_definition_get`, `factory_fluid_definition_get`,
`factory_technology_definition_get`, and
`factory_entity_construction_cost` remain available. They are compatibility
wrappers over the unified tables and return the same immutable records or
values. There is no second copy of their data.

Simulation components continue to store only stable IDs and authoritative
runtime quantities. They never copy prerequisite arrays, burn duration,
fluid metadata, recipe content, or construction metadata. Presentation also
exports IDs and runtime state only; frontends may independently query content
when they need descriptive metadata.

Definitions are excluded from canonical snapshots. This milestone does not
change snapshot version 16, section sizes, bytes, or load behavior. Querying or
validating content cannot mutate simulation state, snapshots, event batches,
or presentation snapshots.

Future content additions extend the fixed tables and their validators. Runtime
registration, dynamic strings, plugins, balancing, unlock enforcement, and UI
remain outside this layer.
