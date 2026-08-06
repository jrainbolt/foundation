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

| Type | `entity_id` | `related_entity_id` | Payload |
|---|---|---|---|
| `ENTITY_CONSTRUCTED` | constructed entity | unused | `entity_type` |
| `ENTITY_DEMOLISHED` | demolished entity | unused | `entity_type` |
| `PRODUCTION_COMPLETED` | producer | unused | `item_type`, `quantity` |
| `ITEM_TRANSFERRED` | source owner | destination owner | `item_type`, `quantity` |
| `POWER_GAINED` | consumer | unused | none |
| `POWER_LOST` | consumer | unused | none |
| `FUEL_IGNITED` | burner owner | unused | `item_type`, quantity one |
| `FUEL_EXHAUSTED` | burner owner | unused | `item_type` |
| `FLUID_INSERTED` | destination | unused | `fluid_type`, `quantity` |
| `FLUID_REMOVED` | source | unused | `fluid_type`, `quantity` |
| `FLUID_TRANSFERRED` | source | destination | `fluid_type`, `quantity` |
| `FLUID_NETWORK_CREATED` | deterministic network ID | unused | none |
| `FLUID_NETWORK_SPLIT` | first resulting network ID | unused | none |
| `FLUID_NETWORK_MERGED` | resulting network ID | unused | none |
| `PIPE_CONNECTED` | unused | unused | number of new connections |
| `PIPE_DISCONNECTED` | unused | unused | number of removed connections |
| `WATER_PRODUCED` | water extractor | unused | water in `fluid_type`, produced `quantity` |
| `BOILER_CONVERSION_COMPLETED` | boiler | unused | consumed `fluid_type`/`quantity`, produced `related_fluid_type`/`related_quantity` |
| `STEAM_ENGINE_GENERATION_COMPLETED` | steam engine | unused | consumed steam in `fluid_type`/`quantity`, generated energy in `related_quantity` |
| `SUNRISE` | unused | unused | none |
| `SUNSET` | unused | unused | none |
| `ACCUMULATOR_CHARGED` | accumulator | unused | charged energy in `quantity`, resulting stored energy in `related_quantity` |
| `ACCUMULATOR_DISCHARGED` | accumulator | unused | discharged energy in `quantity`, resulting stored energy in `related_quantity` |
| `REACTOR_FUELED` | reactor core | unused | `nuclear_fuel_id`, quantity one |
| `REACTOR_HEAT_GENERATED` | reactor core | unused | generated heat in `quantity`, resulting stored heat in `related_quantity` |
| `REACTOR_FUEL_EXHAUSTED` | reactor core | unused | `nuclear_fuel_id`, quantity one |
| `HEAT_NETWORK_CREATED` | unused | unused | network-count change in `quantity` |
| `HEAT_NETWORK_SPLIT` | unused | unused | network-count change in `quantity` |
| `HEAT_NETWORK_MERGED` | unused | unused | network-count change in `quantity` |
| `HEAT_PORT_CONNECTED` | port owner | unused | stable heat-port slot in `quantity` |
| `HEAT_PORT_DISCONNECTED` | port owner | unused | stable heat-port slot in `quantity` |
| `HEAT_TRANSFERRED` | reactor source | exchanger destination | transferred heat in `quantity` |
| `HEAT_EXCHANGER_CYCLE_COMPLETED` | exchanger | unused | heat in `quantity`, water in `related_quantity`, steam in `third_quantity` |
| `RESEARCH_SELECTED` | unused | unused | `technology_id` |
| `RESEARCH_UNIT_COMPLETED` | unused | unused | `technology_id`, science `item_type`/`quantity`, completed units in `related_quantity`, required units in `third_quantity` |
| `TECHNOLOGY_COMPLETED` | unused | unused | `technology_id`, final units in `quantity` |

Construction and demolition events appear only after successful transactional
commands. Production events appear only when output is committed. Transfer
events appear once at the atomic logistics ownership commit. Power events
compare consecutive derived allocations, omit newly created or loaded
consumers, and are emitted in ascending consumer entity-ID order. Fuel events
occur only on ignition and final burn-tick completion, never for steady-state
burning or released-buffer depletion.

Within a step, command and topology events follow deterministic FIFO/topology
order. Fluid-network transport then runs, burner ignition follows, and water
extraction and boiler conversion run in ascending stable machine-store order.
Power allocation follows: accumulator charge/discharge events are emitted in
ascending accumulator entity-ID order, then consumer power transitions are
emitted in ascending consumer entity-ID order. Fuel-completion events follow.
Reactor cores then update in deterministic component-store order, emitting one
aggregate heat event per producing core and an exhaustion event immediately
after the final heat commit. Heat transfers follow ascending exchanger then
reactor ID, with each cycle event following its contributing transfer events.
Extractor
production and producer transfers, belt transfers, refinery completion,
assembler completion, and inserter drop/pickup transfers follow according to
the normal authoritative update phases. Fluid command events occupy their FIFO
command positions. There is no event sorting pass.

Events and their allocation capacity are observer state. They are not encoded
in version 16 snapshots and do not affect canonical bytes. A loaded simulation
starts with an empty batch. Failed loads create no simulation and cannot alter
an existing simulation or its visible batch.
