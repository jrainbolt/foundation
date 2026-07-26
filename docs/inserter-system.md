# Inserter System

Inserters are one-tile, one-item active logistics entities. Their facing is
fixed at placement. The pickup tile is one tile opposite facing and the drop
tile is one tile along facing. For an east-facing inserter, the source is west
and the destination is east.

## State and timing

The only states are:

```text
Idle → Picking Up → Holding → Dropping → Idle
```

Each arm action requires `FACTORY_INSERTER_ACTION_TICKS` updates. Discovery,
pickup commit, holding, drop start, and drop commit occur on distinct update
boundaries. An item can never cross both ownership boundaries in one tick.
Progress is exposed for inspection.

## Ownership and transactions

Pickup and drop use read-only validation followed by deterministic conflict
resolution and atomic commit. Ownership changes only on a winning commit:

```text
source → inserter → destination
```

A failed pickup leaves the source unchanged and returns the inserter to idle.
A failed drop leaves the inserter in `Dropping`, holding the item for another
attempt. No failure duplicates or discards material.

When multiple inserters target the same source or destination endpoint in one
commit phase, the lowest inserter entity ID wins. Store iteration order does
not affect the result.

## Interactions

Pickup sources are belts, valid splitter outputs, refinery output slots, and
assembler output slots. Storage is never a source.

Drop destinations are empty belts, correctly oriented splitter inputs,
non-full storage, compatible refinery inputs, and compatible assembler input
slots. Refinery input/output orientation is enforced. A splitter remains
responsible for routing an item after input.

## Backpressure and demolition

Occupied or incompatible destinations reject delivery without changing
ownership. The inserter continues holding and retries deterministically.

Demolition succeeds only when the inserter is idle, has no held item, and has
zero progress. Pickup/drop activity is busy; held material cannot be deleted.

## Current limitations

There are no long-handed, stack, filter, fast, or burner variants. Inserters
have fixed one-tile reach, one-item capacity, fixed orientation, fixed timing,
and no burner-powered variant, circuits, animation, or dynamic rotation.
