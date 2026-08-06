# Deterministic research

Research is a bounded authoritative subsystem owned by `FactorySimulation`.
This milestone uses the allowed global-controller model rather than a Research
Lab entity: the controller owns one science-item inventory, one selected
technology, completed-ID bits, and fixed progress records. It has no renderer,
wall-clock, or heap-owned state.

`FactoryTechnologyId` is a stable `uint32_t`. Zero means none; IDs 1 and 2 are
Basic Automation and Fluid Handling. Immutable definitions expose bounded
prerequisites, `FACTORY_ITEM_BASIC_SCIENCE`, integer science cost, required
units, work ticks, and stable unlock flags. Validation rejects zero or duplicate
IDs, bad costs/items/flags, duplicate, self, unknown, and cyclic prerequisites.

Selection is a FIFO transactional command. A valid selection replaces the
active selection while preserving each technology's progress. Completed or
prerequisite-blocked selections return research-specific results. Unlock flags
are inspection-only; existing construction remains unlocked.

The controller accepts Basic Science through the explicit
`INSERT_RESEARCH_SCIENCE` command. Basic Science is also an ordinary item:
storage, belts, inserters, and generic logistics accept and snapshot it.

Science is consumed atomically when a unit starts. One work tick is committed
per simulation tick after powered production and before storage/inserter
logistics. The authoritative committed marker preserves an in-progress unit
across selection changes and save/load. No partial quantity is consumed.
Unit completion emits one unit event. Final completion then sets the completed
bit, clears active selection, exposes unlocks, and emits one technology event.
Commands already processed in that tick are unaffected.

Snapshot version 16 stores active ID, completed bits, science inventory, and
both fixed progress records. Definitions, derived unlocks, events, and
presentation are excluded. Load validates IDs, bits, prerequisites, progress
bounds, completion consistency, and the committed marker.

Research storage is fixed-size and never allocates. Tick preflight reserves
space for selection, unit, and completion events, so progress cannot fail after
mutation. Presentation and Godot copy active research, science quantity,
completed count, and active progress without deriving simulation state.

Deferred: lab entities and power endpoints, large trees, science recipes,
construction-lock enforcement, interactive UI, research queues, parallel or
repeatable research, modifiers, balancing, factions, scripting, and networking.
