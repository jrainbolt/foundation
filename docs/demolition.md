# Demolition

`FACTORY_COMMAND_DEMOLISH_ENTITY` removes an extractor, belt, refinery,
assembler, or storage only when doing so cannot destroy material.

- Extractors require an empty output; partial production progress is discarded.
- Belts require no item and zero movement progress.
- Refineries and assemblers require empty inputs/output and no active work.
- Storage requires a total item count of zero.

The simulation validates the entity, subsystem record, empty state, position,
and matching tile occupancy before mutation. Success clears occupancy,
swap-removes the private subsystem record, and invalidates the entity ID.
Resources beneath extractors remain unchanged. There are no refunds or dropped
items.

Commands remain FIFO. Demolish-then-place can reuse a tile in one tick;
place-then-demolish observes the occupied tile and fails placement first.
