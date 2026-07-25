# Deterministic Simulation

`factory_simulation_tick()` executes this fixed order:

```text
1. Apply queued placement and recipe-selection commands FIFO
2. Update extractor production
3. Plan, resolve, and commit existing extractor/refinery outputs into belts
4. Advance belt progress
5. Plan, resolve, and commit belt-to-belt/refinery/storage transfers
6. Update refinery processing
7. Increment the tick
```

Producer conflicts and belt conflicts use lowest source entity ID. A newly
extractor- or refinery-loaded belt gains progress 1 in that tick. A belt-loaded
refinery begins processing and gains progress 1 in its delivery tick.

Refinery output planning precedes processing, so output completed during step
6 cannot move until the next tick. Belt destination items receive progress
zero and cannot cross another belt boundary during the same tick.

Recipe selection therefore takes effect before input transfer and processing
on its tick. A placement result's entity ID must be inspected before submitting
a dependent selection command on a later tick.
