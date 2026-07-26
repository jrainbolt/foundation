# Deterministic save and load

The public memory API is declared in `foundation/snapshot.h`.

- `factory_simulation_snapshot_size` validates state and reports exact size.
- `factory_simulation_save_snapshot` writes into caller-owned memory.
- `factory_simulation_create_snapshot` allocates a library-owned buffer.
- `factory_snapshot_buffer_destroy` releases that buffer.
- `factory_simulation_load_snapshot` creates a new simulation transactionally.

Saving never mutates simulation state. Repeated saves of the same state are
byte-identical. The allocated snapshot buffer belongs to the caller until
destroyed.

Loading sets the output simulation to `NULL` before decoding. It validates the
header and section boundaries, allocates a temporary simulation, decodes every
field, validates cross-subsystem state, and returns it only after complete
success. Every failure destroys temporary allocations. A loaded simulation owns
its reconstructed world, which is released by `factory_simulation_destroy`.
`factory_simulation_get_world` provides read-only world inspection.

Validation covers magic, version, sizes, reserved fields, fixed section order,
canonical booleans, enum ranges, command payloads, entity ID uniqueness and
next-ID monotonicity, world occupancy, subsystem membership, positions,
machine progress, recipes, counted buffers, inserter ownership, storage output,
queue limits, and command results. Invalid live simulation state is rejected
before saving as `FACTORY_RESULT_SNAPSHOT_CORRUPT`.

Pending FIFO commands and the most recently observable command results are
durable state. Allocation capacities and pointers are rebuilt because they do
not affect successful deterministic behavior; live ID and subsystem record
orders are preserved.

The simulation event batch is transient observer state. It is excluded from
canonical snapshot bytes, and a successfully loaded simulation begins with an
empty batch. Failed loads do not mutate an existing simulation or its events.

Externally owned presentation snapshots are also excluded. They remain valid
independently of simulation save/load and may be transactionally rebuilt from
a newly loaded simulation.

The continuation invariant is:

```text
run N → save/load → run M
==
run N + M uninterrupted
```

Tests also require snapshot-chain identity: saving a newly loaded simulation
without advancing it produces the original bytes.

There are currently no file wrappers, compression, encryption, save slots,
autosaving, background I/O, partial loads, or migration from other versions.

Snapshot version 11 includes authoritative power-pole, generator, burner,
biomass-storage, slotted fluid-storage, water-extractor, and boiler records,
steam-engine recipe records, solar generators, accumulators, and reactor-core
heat/fuel records, plus queued placement, fluid, and reactor-fuel commands,
pipe positions, and results. Pole
edges, network IDs, attachments, allocation, and inspection summaries are
derived and rebuilt immediately after load. Earlier snapshots are deliberately
unsupported.
