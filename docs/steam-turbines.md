# Steam turbines

`FACTORY_ENTITY_TYPE_STEAM_TURBINE` is a simulation-owned ordinary power
generator. Its immutable basic definition stores at most 2,000 steam and
converts one atomic cycle of 100 steam into 200 electrical energy, at most
once per tick. This remains distinct from the Steam Engine's one-to-one,
100-output definition and 1,000-unit storage.

The turbine owns one west-facing generic vapor input port and the dedicated
`FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT` slot. Availability is derived
without mutation. The generic power dispatcher retains ascending stable
entity-ID order and preflights the turbine's 200-energy quantum against
indivisible demand and accumulator charge capacity. Steam is committed only
after allocation, exactly 100 units per assigned cycle; with no usable demand
steam is unchanged.

Fluid transport runs before power allocation. Heat Exchangers run afterward,
so steam produced in tick N first moves and becomes turbine-usable in tick
N+1. Activity precedence is disconnected fluid, disconnected power, no
steam, insufficient steam, no usable demand, then working. A successful cycle
emits one aggregate event containing definition ID, cycles, steam, energy,
and the authoritative start-of-step tick.

Placement costs 75 construction units and transactionally creates the entity,
storage/port, and generic generator. Demolition requires empty steam storage,
removes those components, dirties fluid topology, and uses normal refunds.

Snapshot version 13 adds an authoritative turbine component section; steam
remains in generic fluid storage. Network attachment and latest-tick fields
are transient. Presentation copies steam, topology, definition, availability,
actual output, consumption, cycles, and activity without calculating them.

Condensers, cooling towers, recycling, temperature, pressure, heat loss,
inertia, rotor speed, startup delay, frequency, multi-stage turbines, damage,
nuclear controls, and gameplay balancing remain deliberately excluded.
