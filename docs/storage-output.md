# Storage output

Storage inventory remains a fixed set of counted item fields. Each storage also
has one configured output item and a separate one-item output buffer. New
storage entities default to configuration `NONE` and an empty buffer.

`FACTORY_COMMAND_SET_STORAGE_OUTPUT` selects `NONE` or any valid item type.
Commands apply FIFO. Invalid or non-storage entities and invalid item values are
rejected. Inventory may contain arbitrary items during a change, but the output
buffer must be empty. Selecting the current item is a state-preserving success
even while the buffer is occupied. Selecting `NONE` disables export without
altering inventory and requires an empty buffer when it changes configuration.

During each tick, storage updates after assemblers and before inserters. If a
configured item is available and the buffer is empty, exactly one inventory
item moves into the buffer. An idle inserter can begin its pickup state that
tick, but the item remains in storage until the later pickup commit. A buffer
emptied by pickup cannot refill until the next tick.

Only `FACTORY_LOGISTICS_SLOT_STORAGE_OUTPUT` supports source `peek` and
`remove`. Inventory is never a logistics endpoint. The existing storage input
slot supports insertion only. Two inserters competing for one output endpoint
retain the existing lowest-entity-ID winner rule.

The buffer cannot hold more than one item. While it is occupied, inventory does
not export another item. Missing configured inventory leaves the configuration
unchanged and the buffer empty. All seven item types use identical behavior.

Demolition requires both inventory and output buffer to be empty. Conservation
counts inventory, the output buffer, and any later inserter-held item as
separate ownership locations.

Known limitations are intentional: there is one configured output, no automatic
selection or priorities, no direct inventory pickup, no belt extraction, and no
direct storage-to-belt transfer.
