# Deterministic electrical accumulators

An accumulator is a simulation-owned electrical node with a stable entity ID,
grid position, and explicit integer stored energy. The immutable definition is:

- capacity: 10,000 energy;
- maximum charge: 100 energy per tick;
- maximum discharge: 100 energy per tick;
- initial storage: zero.

The power topology derives its pole attachment and network ID. A disconnected
accumulator retains storage but cannot charge or discharge.

Power dispatch first discovers ordinary generator availability and allocates
it to indivisible consumers in ascending entity ID. For each still-unpowered
consumer, connected accumulators are considered in ascending accumulator ID.
Storage changes only if their combined remaining per-tick discharge and stored
energy can fully satisfy that consumer. No partial consumer allocation consumes
storage.

After discharge allocation, each network's remaining ordinary generation is
offered to accumulators in ascending ID. Charge is bounded by unused ordinary
generation, remaining capacity, and the per-tick rate. An accumulator that
discharged is skipped during charging, explicitly enforcing one activity state
per tick. Discharged energy is never considered chargeable generation, so
energy cannot circulate between accumulators.

Per-network accounting is:

```text
ordinary available = ordinary consumer delivery + charge + unused
consumer delivery = ordinary consumer delivery + accumulator discharge
ending storage = starting storage + charge - discharge
```

Latest charge/discharge fields reset at the beginning of each simulation tick.
One aggregate event is emitted per accumulator for positive charge or
discharge, in ascending accumulator ID. Event quantity is transferred energy
and related quantity is resulting stored energy.

Demolition discards stored energy, removes the component, clears its occupied
tile, and applies the normal construction refund. No energy is transferred.

Snapshot version 12 serializes entity ID, position, and stored energy.
Attachment, network ID, latest-tick activity, events, and presentation are
derived or transient. Snapshot topology reconstruction never charges or
discharges storage.

This milestone excludes chemistry, degradation, self-discharge, efficiency
loss, voltage, temperature, charge curves, transformers, weather, research,
and smart-grid optimization.
