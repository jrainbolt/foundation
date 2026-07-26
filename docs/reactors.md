# Deterministic reactor cores and heat storage

`FactoryHeatStorage` is a reusable authoritative integer resource containing
stored heat and capacity. Addition is checked and never exceeds capacity.
Heat has no consumer in this milestone and does not decay.

A reactor core owns a stable entity ID, grid position, one-rod input
inventory, active fuel ID, remaining fuel heat yield, heat storage, and
latest-tick activity. It owns no burner, electrical generator, fluid storage,
steam, water, or hidden energy buffer.

The immutable Basic Nuclear Fuel Rod definition has stable ID 1, total yield
10,000 heat, nominal duration 100 producing ticks, and maximum output 100 heat
per tick. Reactor heat-storage capacity is 10,000 and the input inventory holds
one queued rod. A queued rod moves into active state without changing its
yield.

Once per tick after power dispatch, each reactor resets transient activity,
activates a queued rod if needed, and computes:

```text
output = min(maximum output, remaining fuel yield, remaining heat capacity)
```

Positive output is atomically added to heat storage and subtracted from
remaining fuel yield. Remaining burn ticks are the number of full-rate or
partial-rate producing ticks needed for the remaining yield. A partial
capacity fill therefore conserves fuel exactly rather than rounding it away.
At zero remaining yield, the active fuel clears and one exhaustion event is
emitted.

When storage is full, output is zero, activity is `BLOCKED_HEAT_FULL`, and
active fuel ID, remaining yield, and remaining burn ticks are unchanged.
Removing heat in a future system—or direct fixture manipulation in current
tests—allows production to resume on the next tick.

Fuel insertion is a FIFO command. It validates the reactor, immutable fuel
definition, and inventory capacity before mutation. Success queues one rod and
emits one fueled event; failure leaves state unchanged. Demolition deliberately
discards queued fuel, active fuel, remaining yield, and stored heat under the
milestone's removal policy, then applies the normal construction refund.

Snapshot version 11 serializes reactor identity, position, queued fuel, active
fuel, remaining yield, and stored heat. Capacity comes from the immutable
definition. Activity, latest output, events, and presentation are transient
and reset after load. Reactor component order is preserved, so continuation
uses the same deterministic update and event order.

This milestone intentionally excludes steam production, turbines, electrical
generation, cooling, heat exchangers, radiation, waste, enrichment, breeding,
control rods, pressure vessels, meltdowns, chemistry, and gameplay balancing.
