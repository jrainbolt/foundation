# Demolition

`FACTORY_COMMAND_DEMOLISH_ENTITY` removes a placed entity only when doing so
cannot destroy material or active work.

- Extractors require an empty output; partial production progress is discarded.
- Belts require no item and zero movement progress.
- Refineries and assemblers require empty inputs/output and no active work.
- Storage requires a total item count of zero.

The simulation validates the entity, subsystem record, empty state, position,
matching tile occupancy, fixed refund, and refund overflow before mutation.
Success clears occupancy, swap-removes the private subsystem record,
invalidates the entity ID, and credits the full construction cost. Resources
beneath extractors remain unchanged. There are no dropped items.

Commands remain FIFO. Demolish-then-place can reuse a tile and refund in one
tick; place-then-demolish observes the occupied tile and fails placement first.

For assemblers, every counted input and output must be zero, processing must be
false, and progress must be zero. An empty configured assembler and an empty
recipe-`NONE` assembler both receive the same fixed refund.
