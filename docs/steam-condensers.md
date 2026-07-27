# Steam condensers

`FACTORY_ENTITY_TYPE_STEAM_CONDENSER` is a simulation-owned, purely
fluid-processing machine: it is never a power generator. Its immutable basic
definition stores at most 2,000 exhaust steam and 2,000 water and converts
one complete cycle of 100 exhaust steam into 100 water, consuming 50
electrical power per cycle, at most once per tick. It closes the real
thermal loop the Steam Turbine opens: `Water -> Heat Exchanger -> Live Steam
-> Steam Turbine -> Exhaust Steam -> Steam Condenser -> Water`. Steam
Turbines convert live steam into electricity and exhaust steam; Steam
Condensers spend electricity to recover that turbine exhaust back into
water. The condenser's input is `FACTORY_FLUID_EXHAUST_STEAM`, a fluid type
distinct from the turbine's `FACTORY_FLUID_STEAM` input -- see "Interaction
with turbines" below and `docs/steam-turbines.md`. A condenser has no way to
consume live steam even if some were physically routed to it: the recipe's
fluid-type check is exact-identity, not merely vapor-class.

## Ownership

The condenser owns exactly:

- an immutable definition reference (`FactorySteamCondenserDefinitionId`)
- authoritative steam input and water output storage, in the generic fluid
  system (`FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT` /
  `..._OUTPUT`)
- one west-facing steam input port and one east-facing water output port
- transient last-tick activity, consumed steam, produced water, and
  completed-cycle counters

It does not own heat, electricity storage, pressure, network state, or any
buffer beyond the two generic fluid slots above. Steam and water are always
authoritative inside generic fluid storage, exactly like every other
fluid-owning machine in this codebase (boilers, heat exchangers, steam
engines, steam turbines).

## Recipe

One complete cycle requires all three of:

- 100 exhaust steam in the input slot
- 50 electrical power (the network's ordinary fixed-demand allocation for
  this tick, already decided by the time the condenser's recipe runs)
- room for 100 water in the output slot

and produces exactly 100 water. As with every other generic-recipe machine in
this codebase (boilers, heat exchangers), all validation precedes the atomic,
allocation-free commit: if any one of the three conditions is not met, the
recipe does not execute at all this tick -- there is no partial or fractional
conversion, and nothing is deducted or produced. `maximum_cycles_per_tick` is
`1` for the basic definition; the field exists (mirroring the Steam Turbine's
own definition shape) so a future definition could raise it without an
architecture change.

## Power behavior

The condenser is an ordinary, indivisible power consumer with a fixed demand
of 50 -- the same mechanism that already powers extractors, refineries,
assemblers, and inserters. It participates in power topology (pole coverage,
network membership), the generic deterministic allocation described in
`power.md`, and ascending-entity-ID consumer scheduling. It introduces no
turbine-specific or condenser-specific power logic: the dispatcher cannot
distinguish a condenser from any other fixed-demand consumer, and adding this
milestone required only one new demand constant
(`FACTORY_POWER_DEMAND_STEAM_CONDENSER`) and one more store in the consumer
loop, not any change to the allocation algorithm itself.

## Fluid behavior

The condenser integrates into the existing generic fluid network exactly like
a boiler or heat exchanger: its steam input and water output are ordinary
fluid ports (west/east, matching the turbine's west-facing convention),
transported by the single fluid-transport pass that runs before power
allocation each tick. Exhaust steam produced by a Steam Turbine in tick N
first moves and becomes condenser-usable in tick N+1 -- there is no second
transport pass. Water produced during recipe execution becomes transportable
on the following tick's transport phase, the same rule the Steam Turbine's
steam consumption follows in reverse.

The condenser does not know a turbine exists upstream. Its input storage
only ever accepts `FACTORY_FLUID_EXHAUST_STEAM` (enforced by the generic
fluid-type-identity check on non-empty storage); whatever pipe network is
physically connected to its input port is the network its supply comes
from. It is the level designer's or player's responsibility to route a
turbine's exhaust output to a condenser's input via pipes -- exactly as
routing a Heat Exchanger's steam output to a turbine's input already is.

## Activity states

```text
Working                Recipe executed this tick.
No Power                Connected to a power network, but not allocated
                         power this tick.
No Steam                Fewer than 100 exhaust steam in the input slot.
Output Full             Fewer than 100 units of free capacity in the water
                         output slot, or it holds an incompatible fluid.
Disconnected Fluid       The steam input port has no fluid network.
Disconnected Power       Not covered by any power pole.
```

Precedence when a cycle does not execute is disconnected fluid, disconnected
power, no power, no steam, output full, then working -- matching the Steam
Turbine's own disconnected-first precedence. `IDLE` is the default state
before any tick has evaluated the condenser (construction tick only).

## Deterministic rules

The condenser:

- never creates water from nothing -- every unit of water produced is paired
  with exactly one unit of steam consumed, at the fixed 100:100 ratio
- never destroys steam without producing water in the same commit
- never executes a partial recipe -- all three conditions (steam, power,
  output room) are checked before any storage is mutated
- never depends on floating point -- every quantity is an unsigned integer
- never executes twice in one tick -- `maximum_cycles_per_tick` is `1`
- never bypasses the fluid network -- steam arrives and water leaves only
  through the generic fluid-transport pass

## Public API

```c
FactoryResult factory_simulation_get_steam_condenser(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactorySteamCondenserInspection *out_condenser
);
```

The inspection exposes definition ID, steam/water quantities and capacities,
input/output fluid network IDs, power network ID, fluid connection state,
power connection/powered state, power demand per cycle, last-tick consumed
steam and produced water, completed cycles, and activity -- all
renderer-neutral, matching the presentation-neutral shape every other machine
inspection in this codebase already uses.

## Commands and events

`FACTORY_COMMAND_PLACE_STEAM_CONDENSER` costs 75 construction units and
transactionally creates the entity, both fluid storages, both fluid ports,
and registers it as an ordinary power consumer -- no power-generator record
is created. Demolition requires both the steam input and water output
storage to be empty first (matching the Steam Turbine's and Boiler's own
empty-storage demolition requirement), removes the owned ports and storages,
dirties fluid topology, and uses the normal construction-unit refund path.

`FACTORY_EVENT_STEAM_CONDENSER_CYCLE_COMPLETED` is emitted once per
successful recipe cycle and never on a failed attempt; `fluid_type` /
`quantity` carry the consumed steam and `related_fluid_type` /
`related_quantity` carry the produced water, mirroring the Boiler's own
conversion-event shape.

## Snapshots

Snapshot version 14 adds an authoritative steam-condenser component section
(entity ID, grid position, and stable definition ID -- 16 bytes, the same
shape as the steam-turbine section). Steam and water remain in generic fluid
storage. Activity, last-tick output, cycle counters, network attachments, and
topology IDs are not persisted; they are recomputed deterministically after
load, exactly like every other transient machine field in this codebase. The
condenser's own persisted section is unchanged by the later turbine-exhaust
correction (version 15, see `docs/steam-turbines.md`): only the turbine's
storage/port pair count grew, not the condenser's.

## Interaction with turbines

Steam Turbines and Steam Condensers are decoupled through fluid-type
identity, not through any direct reference to each other. A turbine produces
`FACTORY_FLUID_EXHAUST_STEAM` into its own east-facing output storage; a
condenser consumes that same fluid type from its own west-facing input
storage. Both are ordinary generic-fluid-system participants -- neither
holds an entity ID or pointer to the other -- so removing one never
invalidates the other's placement, ports, or recipe logic. What connects
them in a given layout is exactly what connects any two fluid machines:
physical pipe adjacency forming a shared fluid network, followed by the
generic per-tick pressure equalization that moves fluid between any two
same-network storages holding (or able to accept) the same fluid type.

This is deliberately the same mechanism that already links a Heat Exchanger's
live steam output to a Steam Turbine's live steam input -- extended, not
special-cased, to a second fluid identity. A condenser's input port can be
wired to any network; it will only ever accept and process exhaust steam,
never live steam, because exhaust steam and live steam are different
`FactoryFluidType` values and the generic storage/port/network machinery
already treats different fluid types as mutually incompatible once a slot is
non-empty. A condenser wired to a network that never carries exhaust steam
(for example, one carrying only live steam, or nothing) simply sits in
`No Steam`/`Disconnected Fluid` forever -- there is no turbine-condenser-
specific pairing code anywhere in the engine.
