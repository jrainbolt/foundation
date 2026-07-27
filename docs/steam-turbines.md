# Steam turbines

`FACTORY_ENTITY_TYPE_STEAM_TURBINE` is a simulation-owned ordinary power
generator. Its immutable basic definition stores at most 2,000 steam and
converts one atomic cycle of 100 steam into 200 electrical energy plus 100
exhaust steam, at most once per tick. This remains distinct from the Steam
Engine's one-to-one, 100-output definition and 1,000-unit storage.

## Live steam in, exhaust steam out

A turbine cycle consumes `FACTORY_FLUID_STEAM` (live steam) and produces
`FACTORY_FLUID_EXHAUST_STEAM` -- a distinct `FactoryFluidType`, not merely a
different quantity of the same fluid. Both belong to the vapor fluid class,
but exact fluid-type identity, not class, is what a storage slot enforces
once it holds anything: the generic mismatch check
(`storage->quantity != 0 && storage->fluid_type != fluid_type`) that already
keeps every other fluid pair apart applies here with no turbine- or
exhaust-specific code. Live steam and exhaust steam can never occupy the
same storage slot or merge across a network once that slot has committed to
one identity.

The turbine owns two generic vapor ports on its own dedicated slots: a
west-facing input (`FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT`) for live
steam, and an east-facing output (`FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT`)
for exhaust steam. A Steam Condenser recovers that exhaust into water; see
`docs/steam-condensers.md`. The turbine does not know a condenser exists on
the other end -- it just produces exhaust into its own output storage, the
same way any generator produces into its own state, and generic pipe-network
pressure equalization (`factory_fluid_network_transfer`) moves it onward.

## Generation gating

Availability is derived without mutation. The generic power dispatcher
retains ascending stable entity-ID order across every generator, continuous
or atomic; there is no global rule preferring the turbine over an ordinary
generator or vice versa. A complete cycle may fire when only part of its
200-energy output ends up committed to consumers and accumulator charging
combined -- a full cycle is indivisible, but its output is not: once fired,
the 200 energy is a normal divisible pool for whichever consumers and
accumulator draw on it, in the same tick, the same way a continuous
generator's output is.

A cycle requires both ends to have room: at least 100 live steam in the
input slot, *and* at least 100 units of headroom in the output slot for the
exhaust that same cycle produces (empty, or already holding exhaust steam
below capacity). Plentiful steam does not matter if the exhaust output has
nowhere to go -- the cycle simply does not fire, exactly as it would not
fire on insufficient input steam. Consumption and exhaust production happen
atomically together: a cycle that cannot place its exhaust is not committed
at all, so it never partially consumes steam without producing the matching
exhaust. Steam is committed only once any part of a cycle is needed, exactly
100 units for that cycle, in full, regardless of how much of the resulting
200 energy is ultimately claimed; with no usable demand or accumulator
headroom at all, steam and exhaust storage are both unchanged. Whatever part
of a fired cycle's output is not claimed is recorded as unused generation,
never re-attempted or refunded.

Fluid transport runs before power allocation. Heat Exchangers run afterward,
so steam produced in tick N first moves and becomes turbine-usable in tick
N+1.

## Activity precedence

In order: disconnected live-steam input, disconnected power, no live steam,
insufficient live steam, exhaust output full, no usable demand, then
working (`t->actual_output != 0` short-circuits every other check). There is
deliberately no separate "disconnected exhaust output" state distinct from
`BLOCKED_EXHAUST_FULL`: a disconnected exhaust port and a connected-but-full
downstream network are observationally identical from the turbine's own
point of view -- both mean "my local exhaust storage has no room," which is
exactly what `BLOCKED_EXHAUST_FULL` already reports, checked purely against
local storage room, never against exhaust port connectivity. A disconnected-
but-not-yet-full exhaust output does not block anything: the turbine keeps
firing and accumulating exhaust locally regardless of whether a pipe is
attached, exactly as it would with a pipe attached to a network that simply
never drains it. Splitting this into two activity values would not change
turbine behavior in any case, so the existing model is kept rather than
adding a state with no distinct effect.

## Inspection and events

`FactorySteamTurbineInspection` exposes, without calculating: live-steam
stored/capacity/network-id/connected, exhaust-steam stored/capacity/
network-id/connected, `steam_consumed_last_tick`, `exhaust_produced_last_tick`,
`actual_output`, `available_output`, and `activity`.

Physical generation is exposed as `actual_output`: whenever a cycle fires,
`factory_steam_turbine_consume_for_generation` is always called with the
generator's full atomic quantum (`energy_per_cycle`, 200) as its `generated`
argument -- never a lesser amount -- so `actual_output` is always the
complete, indivisible physical output of that cycle. `completed_cycles_
last_tick * energy_per_cycle` is definitionally the same value, not an
independent quantity.

Committed electrical output is a *different* number, exposed generically on
every generator as `FactoryPowerGeneratorInspection.committed_output`: the
portion of that same fired cycle's 200 that `factory_power_rebuild`'s
preflight actually attributed to specific consumers and accumulator
charging, walked in the same ascending-ID order as everywhere else. A single
100-demand consumer with no accumulator yields `committed_output == 100`
while `actual_output` is still 200 -- the cycle fires in full regardless of
how much of it found a taker, exactly as the generation-gating section
above describes. `committed_output` is never threaded back into the
generation call; it is bookkeeping the dispatcher updates independently of
what the turbine itself was told to (fully) produce.

"Unused electrical generation" is a network-level aggregate, not a stored
per-generator field, though it is not fully opaque per generator either: for
a single generator alone on a network, `available_output - committed_output`
on that generator reconstructs exactly what the network attributes to it.
With several generators (of any type) jointly oversupplying a network, the
leftover cannot be attributed to any one of them without an arbitrary
tie-breaking rule, which is why the engine tracks it only as a network total
(`FactoryPowerNetworkInspection.unused_generation`; see `docs/power.md`),
computed as that network's total generation minus the sum of every
generator's `committed_output` on it -- never re-derived from `actual_output`.

A successful cycle emits exactly one `FACTORY_EVENT_STEAM_TURBINE_
CYCLE_COMPLETED` event containing definition ID, completed cycles, consumed
live steam, produced exhaust (fixed 1:1 with consumed steam), and generated
energy (`third_quantity`, the full physical `actual_output`, not
`committed_output`). No event is emitted on a blocked attempt of any kind,
including one blocked specifically by full exhaust output: the event
represents a fired cycle's physical outcome, and a blocked attempt has no
outcome to report. `committed_output` is visible only through the generic
power-generator inspection, not through this event.

## Construction, snapshots, and presentation

Placement costs 75 construction units and transactionally creates the
entity, both storage/port pairs, and the generic generator. Demolition
requires both the steam and exhaust storages to be empty, removes those
components, dirties fluid topology, and uses normal refunds.

Snapshot version 15 adds the second (exhaust) fluid storage/port pair to the
turbine's dedicated validation and reconstruction paths; both remain in
generic fluid storage, and network attachment and latest-tick fields stay
transient. This is an incompatible structural change from version 14 (which
validated exactly one turbine-owned storage/port pair), so older snapshots
are rejected rather than silently misread. Presentation copies steam,
exhaust, topology, definition, availability, actual output, consumption,
exhaust production, cycles, and activity without calculating them.

Cooling towers, recycling, temperature, pressure, heat loss, inertia, rotor
speed, startup delay, frequency, multi-stage turbines, damage, nuclear
controls, and gameplay balancing remain deliberately excluded.
