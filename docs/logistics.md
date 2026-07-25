# Deterministic Logistics

Items have exactly one owner: an extractor output buffer, one belt, or storage.
An extractor transfers only to an empty adjacent belt in its output direction.
A ready belt transfers to an empty adjacent belt, a correctly oriented
refinery input, or non-full adjacent storage. Refineries emit only to belts.
Direct producer-to-storage transfer is prohibited.

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
