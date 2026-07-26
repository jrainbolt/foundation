# Deterministic Simulation

`factory_simulation_tick()` executes this fixed order:

```text
1. Apply queued placement and recipe-selection commands FIFO
2. Update extractor production
3. Plan and commit existing extractor/refinery/assembler outputs into belts
4. Advance belt progress
5. Plan and commit belt-to-belt/refinery/assembler/storage transfers
6. Update refinery processing
7. Update assembler processing
8. Plan and commit inserter drops, then pickups
9. Increment the tick
```

Producer conflicts and belt conflicts use lowest source entity ID. A newly
extractor- or refinery-loaded belt gains progress 1 in that tick. A belt-loaded
refinery begins processing and gains progress 1 in its delivery tick.

Refinery output planning precedes processing, so output completed during step
6 cannot move until the next tick; assembler output completed in step 7 follows
the same rule. Belt destination items receive progress zero.

Recipe selection therefore takes effect before input transfer and processing
on its tick. Commands are FIFO, so placement followed by selection for its
deterministically allocated ID can succeed in one tick; selection before that
placement fails and does not prevent the later placement.

Construction grants, placement spending, and demolition refunds occur during
step 1 in FIFO order. Later commands in a tick observe earlier balance changes.
Production and logistics updates never modify construction units.

Demolition is also applied in step 1. Successfully removed entities cannot
produce, advance, receive, or transfer later in that tick. Rejected demolition
leaves the entity active.

Inserter drop processing precedes pickup processing, so an item acquired during
step 8 remains visibly held until a later tick. Both phases validate current
endpoint state, resolve contention by lowest inserter entity ID, and only then
commit ownership.

Producer, belt, splitter, and inserter planners now record private logistics
endpoints. Endpoint inspection is read-only; commit revalidates the expected
source item and destination acceptance before either ownership field changes.
This changes internal dispatch only, not the fixed update order.

Storage output generation occurs after assembler processing and before inserter
updates. A newly filled buffer may make an idle inserter enter `PICKING_UP`, but
ownership transfers only on a later pickup commit. Pickup occurs after storage
generation, so an emptied buffer refills no earlier than the next tick.

Snapshots may be taken between ticks, including after commands are queued.
They preserve the tick, FIFO queue, last command results, subsystem iteration
order, and next entity ID, allowing the loaded simulation to resume at the same
next command phase.

After commands, power topology and allocation are rebuilt before extraction.
The resulting powered flags govern extractors, refineries, assemblers, and both
inserter phases for that tick. Passive transfers retain their established
relative order and require no power.
