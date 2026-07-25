# Extractor System

Extractors are the first gameplay system in Foundation. Each extractor owns an
entity ID and stores its tile position, output direction, production progress,
and one-item output buffer. The simulation owns extractor storage; inspection
copies records into caller-provided memory.

## Placement

Placement occurs only through a queued command. The target must be in bounds,
contain iron, and be unoccupied. Output direction is retained for future item
transfer but currently has no effect.

Placement is transactional. The simulation validates the tile and reserves
extractor storage before creating an entity. It then marks the tile occupied
and adds the already-reserved extractor record. If marking occupancy fails,
the newly created entity is destroyed. Resource state is never changed by
placement.

## Production timing

Commands are applied before extractors update. Therefore, an extractor placed
during a tick receives progress 1 during that same tick. After exactly 20
extractor updates, it consumes one deposit unit, produces one iron ore, and
resets progress to zero.

## Output and conservation

The output buffer holds exactly one item. While full, progress does not advance
and the deposit does not decrease. Extraction also stops at a depleted
deposit. With no transfer system, conservation is:

```text
initial deposit = remaining deposit + buffered ore
```

## Deferred work and limitations

Extractor output direction now selects an adjacent belt. Output transfers only
when that tile contains an empty belt; direct storage transfer is prohibited.
Extractors cannot be destroyed, only iron is extractable, and production rates
are fixed.
