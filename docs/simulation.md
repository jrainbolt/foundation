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
8. Increment the tick
```

Producer conflicts and belt conflicts use lowest source entity ID. A newly
extractor- or refinery-loaded belt gains progress 1 in that tick. A belt-loaded
refinery begins processing and gains progress 1 in its delivery tick.

Refinery output planning precedes processing, so output completed during step
6 cannot move until the next tick; assembler output completed in step 7 follows
the same rule. Belt destination items receive progress zero.

Recipe selection therefore takes effect before input transfer and processing
on its tick. A placement result's entity ID must be inspected before submitting
a dependent selection command on a later tick.

Demolition is also applied in step 1. Successfully removed entities cannot
produce, advance, receive, or transfer later in that tick. Rejected demolition
leaves the entity active.
