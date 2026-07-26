# Generic burner fuel

Burners are reusable authoritative components owned by machines. A burner
accepts fuel by class, keeps a single-item-type input inventory, tracks one
active item, and buffers released integer energy. The initial fuel table
contains a solid biomass pellet with a 1,000-tick duration and 100,000 energy.

An idle burner ignites one compatible inventory item only when the finite
released-energy buffer has room for that item's entire energy. Its capacity is
`FACTORY_BURNER_RELEASED_ENERGY_CAPACITY` (100,000). This simple rule is not
demand-aware. It prevents unbounded idle accumulation: another item cannot
ignite until the previous burn completes and enough released energy has been
consumed.

For total energy `E`, duration `D`, and zero-based elapsed tick `t`, release is
`E / D + 1` when `t < E % D`, otherwise `E / D`. Remainder units are released
on the earliest ticks. This quotient-and-remainder rule uses no wide
multiplication or floating point and releases exactly `E` units over exactly
`D` ticks, including when `E < D`.

The simulation phases are:

1. apply commands;
2. ignite eligible burners and release the active items' current-tick energy;
3. rebuild power topology and allocate whole-machine demand;
4. consume only actual allocated generator output;
5. complete the active burn tick and emit completion when appropriate;
6. run production and logistics.

Energy released in a tick is immediately available to that tick's power
allocation. A basic generator offers the lesser of its 100-unit per-tick
maximum and its released energy. Unallocated released energy remains buffered;
unreleased active-fuel energy cannot be consumed. Fuel energy is an item reserve
across a burn, while generator output is a per-tick electrical rate.

Fuel reaches a generator through its burner-input logistics endpoint. Belts and
inserters use the normal deterministic transfer-intent and commit rules.
Incompatible items are rejected without mutation.

The world owns burner state. Public inspection returns a value copy. Fuel
definitions are immutable process-lifetime data. Ignition emits only after the
inventory decrement and active-state initialization commit.
`FACTORY_EVENT_FUEL_EXHAUSTED` means the active item completed its final burn
tick and has no unreleased energy; released energy may remain buffered. Each
event occurs exactly once per active item.

Burner inventory, active fuel, remaining ticks, accepted classes, and released
energy are canonical version 4 snapshot state. Duration and unreleased energy
are derived exactly from the immutable fuel definition and remaining ticks.
Power topology and allocation remain derived. Presentation exposes inventory,
active fuel, duration, remaining ticks, unreleased energy, released energy, and
activity without granting mutation.

Demolition rejects any generator with unignited inventory, an active item,
unreleased energy, or released energy using
`FACTORY_RESULT_ENTITY_HAS_MATERIAL`. This deliberate nonempty policy avoids a
generator-specific refund path and prevents silent energy or item loss.
