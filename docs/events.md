# Deterministic simulation events

Each `factory_simulation_tick` produces one transient, simulation-owned event
batch. Before any authoritative mutation, the simulation reserves enough
storage for the maximum events that step can produce. If reservation fails,
the function returns `FACTORY_RESULT_OUT_OF_MEMORY` without advancing,
applying commands, clearing the previous batch, or changing authoritative
state.

After successful reservation, the previous batch is cleared and committed
transitions append events in the existing simulation update order. The
completed batch remains available until the next successful tick, an explicit
`factory_simulation_clear_events`, or simulation destruction. Event pointers
are read-only and have the same lifetime; out-of-range lookup returns `NULL`.

Events are observational. No simulation system reads them, and clearing or
ignoring them cannot affect later behavior. Events use the authoritative tick
value at the start of the step. Thus the events inspected after a successful
step from tick 4 to tick 5 carry tick 4.

## Event fields

Every `FactoryEvent` is fully initialized. Unused fields are zero or their
`FACTORY_*_NONE` value.

| Type | `entity_id` | `related_entity_id` | `entity_type` | `item_type` / `quantity` |
|---|---|---|---|---|
| `ENTITY_CONSTRUCTED` | constructed entity | unused | constructed type | unused |
| `ENTITY_DEMOLISHED` | demolished entity | unused | demolished type | unused |
| `PRODUCTION_COMPLETED` | producer | unused | unused | committed output stack |
| `ITEM_TRANSFERRED` | source owner | destination owner | unused | transferred item and count |
| `POWER_GAINED` | consumer | unused | unused | unused |
| `POWER_LOST` | consumer | unused | unused | unused |
| `FUEL_IGNITED` | burner owner | unused | unused | one consumed fuel item |
| `FUEL_EXHAUSTED` | burner owner | unused | unused | exhausted fuel item |

Construction and demolition events appear only after successful transactional
commands. Production events appear only when output is committed. Transfer
events appear once at the atomic logistics ownership commit. Power events
compare consecutive derived allocations, omit newly created or loaded
consumers, and are emitted in ascending consumer entity-ID order. Fuel events
occur only on ignition and final burn-tick completion, never for steady-state
burning or released-buffer depletion.

Within a step, command events follow command FIFO order. Burner ignition events
follow next, then power transitions, then fuel-completion events. Extractor
production and producer transfers, belt transfers, refinery completion,
assembler completion, and inserter drop/pickup transfers follow according to
the normal authoritative update phases. There is no event sorting pass.

Events and their allocation capacity are observer state. They are not encoded
in version 4 snapshots and do not affect canonical bytes. A loaded simulation
starts with an empty batch. Failed loads create no simulation and cannot alter
an existing simulation or its visible batch.
