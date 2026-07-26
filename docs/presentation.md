# Renderer-neutral presentation snapshots

A presentation snapshot is an externally owned, transient copy of the current
visible simulation state. It answers “what exists now,” while the simulation
event batch answers “what happened during the latest successful step.” Neither
presentation records nor events are authoritative.

Create a snapshot with `factory_presentation_snapshot_create`, rebuild it from
a simulation, and release it with `factory_presentation_snapshot_destroy`.
Entity, resource, and power-edge pointers returned by inspection belong to the
snapshot. They remain valid while the simulation continues advancing and until
that presentation snapshot is rebuilt, cleared, or destroyed.

Rebuild is transactional. It counts and validates the required records, builds
three temporary owned arrays, and replaces the previous contents only after
the complete rebuild succeeds. Allocation, size, or invariant failure leaves
the previous tick and every previous record unchanged and does not mutate the
simulation.

The three collections are:

- real simulation entities, ordered by ascending stable entity ID;
- finite resource deposits, ordered by grid row and then column;
- canonical power edges, ordered lexicographically as `(lower pole ID, higher
  pole ID)`.

Deposits are separate because they are world tiles, not entities, and have no
stable entity IDs. No synthetic IDs are invented.

Every entity record contains its ID, type, grid position, integer direction,
derived machine status, and powered flag. Its tagged union contains the active
entity type's integer-only state. Storage uses seven fixed item quantities in
`FactoryItemType` order from iron ore through copper wire. Progress is always a
tick numerator and duration denominator; no percentage or interpolation value
is calculated.

Machine-status precedence is:

```text
unpowered
→ blocked output
→ actively processing or otherwise capable of work
→ blocked input
→ idle
```

`UNPOWERED` therefore explains the immediate inability to advance even if an
output is also occupied. Recipe-less refineries and assemblers are `IDLE`.
Passive entities use `NONE`. Status is derived during rebuild and is never
stored back into the simulation.

The snapshot tick is the authoritative number of completed simulation steps.
After a successful step from tick 4 to tick 5, a rebuild represents post-step
state and records tick 5.

A safe frontend sequence is:

```text
result = factory_simulation_tick(simulation)
if result is OK:
    inspect the completed event batch
    rebuild the presentation snapshot
    apply the complete snapshot to the frontend
    use events for one-shot effects
```

Presentation rebuild does not clear or copy events. Event clearing does not
alter an existing presentation snapshot. If a tick or rebuild fails, a caller
may continue displaying its previous presentation snapshot.

Presentation data is excluded from canonical version 2 simulation snapshots.
After simulation load, callers rebuild a new presentation snapshot. Equivalent
authoritative simulations produce field-wise equivalent presentation records.

This milestone intentionally performs a complete rebuild. That is simpler and
safer than maintaining a second mutable mirror, at the cost of allocation and
copying proportional to the current world and entity counts. A future
milestone may add deltas, but no dirty tracking or delta contract exists now.
