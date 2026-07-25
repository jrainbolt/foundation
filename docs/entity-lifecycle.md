# Entity Lifecycle

Entity IDs are monotonic and never reused. A replacement placed after
demolition always receives a newer ID, while unrelated IDs remain unchanged.

Private subsystem arrays use swap-remove. Simulation outcomes do not depend on
their order: transfer contention uses source entity IDs, and inspection finds
records by ID.

After successful demolition, entity and type-specific lookup fail and the tile
contains occupying entity ID zero. Any previously obtained internal record
pointer is invalid after removal or later store growth; public inspection APIs
copy records to caller-owned values.
