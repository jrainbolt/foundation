# Entity Lifecycle

Placement requires the entity's full fixed construction cost. Insufficient
funds fail before ID allocation; every other placement failure also leaves the
balance unchanged. Successful placement deducts only after the entity,
subsystem record, and tile occupancy have been established.

Entity IDs are monotonic and never reused. A replacement placed after
demolition always receives a newer ID, while unrelated IDs remain unchanged.

Successful demolition refunds the immutable cost. Demolish-then-replace can
therefore reuse both a tile and its refund in one FIFO command-processing tick.

Private subsystem arrays use swap-remove. Simulation outcomes do not depend on
their order: transfer contention uses source entity IDs, and inspection finds
records by ID.

After successful demolition, entity and type-specific lookup fail and the tile
contains occupying entity ID zero. Any previously obtained internal record
pointer is invalid after removal or later store growth; public inspection APIs
copy records to caller-owned values.

Changing or clearing an assembler recipe leaves its entity ID, orientation,
tile occupancy, and construction accounting unchanged. It is permitted only
when empty and idle, preventing existing material from being reinterpreted.

Snapshots preserve live IDs, their internal order, tombstone effects through
the monotonic next ID, subsystem membership, and tile occupancy. Loading never
renumbers entities; the next placement receives the same ID as uninterrupted
execution.
