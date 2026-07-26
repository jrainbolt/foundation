# Deterministic Logistics

Items have exactly one owner: a deposit, machine buffer, belt, splitter,
inserter, or storage.
An extractor transfers only to an empty adjacent belt in its output direction.
A ready belt transfers to an empty adjacent belt, a correctly oriented
refinery input, or non-full adjacent storage. Refineries emit only to belts.
Direct producer-to-storage transfer is prohibited.

Assemblers accept required plates from any adjacent belt pointing inward and
emit components only to belts. Transfer destination identity includes both
entity and logical endpoint. Assembler iron and copper slots are different
endpoints, allowing simultaneous different inputs while preventing duplicate
commits to either slot.

Extractor transfers happen before belt progress, so a newly inserted item gains
progress 1 in that tick. Belt-to-belt destinations receive progress zero
because progress has already run. Plan-and-commit movement ensures one item
crosses at most one belt ownership boundary per tick.

Conflicts are resolved by lowest source entity ID, independent of store
iteration order. Failed sources retain their item and readiness.

Iron and copper are accounted independently across ore and one-to-one plates:

```text
initial iron
= remaining deposit
+ extractor-buffered ore
+ belt-carried ore
+ refinery input
+ in-process refinery units
+ refinery output plates
+ belt-carried plates
+ stored ore and plates
```

Tests check this invariant during movement, blocking, full storage, and
conflicting transfers.

Demolition cannot remove an entity that owns an item or active in-process
material, so successful lifecycle changes preserve both elemental totals.

Splitters accept only from the tile opposite their facing direction and route
to left/right output belts. Round-robin state advances only after successful
ownership transfer; blocked splitters retain both item and state.

Inserters pick up from the tile opposite facing and drop onto the tile along
facing. Supported sources are belts, splitter outputs, and refinery/assembler
outputs. Supported destinations are belts, splitter inputs, storage, and
compatible refinery/assembler inputs. Storage is drop-only. Pickup and drop
commit on separate ticks; blocked delivery remains owned by the inserter.

Internally, planners identify both the entity and logical slot for every source
and destination. Read-only peek/acceptance precedes conflict resolution.
Winning transfers revalidate and atomically move exactly one item through the
shared endpoint layer. Assembler inputs are generic slots 0 and 1; the selected
recipe supplies their item types and capacities. The lowest compatible slot is
chosen, different slots remain independent, and same-slot conflicts remain.
Counted assembler output still transfers one item per successful commit.

Storage inventory is not a logistics source. Its subsystem moves one configured
item into a bounded output endpoint, after which inserters use the ordinary
transactional transfer and contention rules. Belts cannot extract from storage.

All persistent ownership locations and fairness state are serialized:
machine buffers, belt progress, splitter next-output choice, inserter state and
held item, storage inventory, and storage output. Transfer intents are
tick-local and are rebuilt because none survive a completed update.
