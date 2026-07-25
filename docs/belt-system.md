# Belt System

Each belt is an entity occupying one tile. It has a cardinal direction, a
single item slot, and integer movement progress. Empty belts contain
`FACTORY_ITEM_NONE` and have progress zero.

An occupied belt gains one progress per update, capped at
`FACTORY_BELT_TRANSFER_TICKS` (currently 5). At 5 it is ready to transfer to
the adjacent tile in its direction. A blocked belt retains both its item and
ready progress. A successful source becomes empty with progress zero; a belt
destination receives the item with progress zero.

Movement uses plan, resolve, and commit phases. Planning reads the state before
any belt ownership changes. If multiple ready belts target one destination,
the lowest source entity ID wins. Losers remain ready. This prevents an item
from crossing multiple belt boundaries in one tick.

Current belts have one slot, one lane, one speed, and transport only iron ore.
There are no curves, splitters, mergers, underground segments, or demolition.
