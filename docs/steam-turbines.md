# Steam turbines

`FACTORY_ENTITY_TYPE_STEAM_TURBINE` is a simulation-owned ordinary power
generator. Its immutable basic definition stores at most 2,000 steam and
converts one atomic cycle of 100 steam into 200 electrical energy, at most
once per tick. This remains distinct from the Steam Engine's one-to-one,
100-output definition and 1,000-unit storage.

The turbine owns one west-facing generic vapor input port and the dedicated
`FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT` slot. Availability is derived
without mutation. The generic power dispatcher retains ascending stable
entity-ID order across every generator, continuous or atomic; there is no
global rule preferring the turbine over an ordinary generator or vice versa.
A complete cycle may fire when only part of its 200-energy output ends up
committed to consumers and accumulator charging combined -- a full cycle
is indivisible, but its output is not: once fired, the 200 energy is a
normal divisible pool for whichever consumers and accumulator draw on it,
in the same tick, the same way a continuous generator's output is. Steam is
committed only once any part of a cycle is needed, exactly 100 units for
that cycle, in full, regardless of how much of the resulting 200 energy is
ultimately claimed; with no usable demand or accumulator headroom at all,
steam is unchanged. Whatever part of a fired cycle's output is not claimed
is recorded as unused generation, never re-attempted or refunded.

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
