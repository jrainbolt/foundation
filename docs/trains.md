# Deterministic train consists

Foundation models a train as one locomotive followed by zero or more Cargo
Wagons. The locomotive entity ID is also the stable `FactoryTrainId`; this
preserves the identity of every existing one-vehicle train without a second ID
allocator. The locomotive owns propulsion progress and the first wagon link.
Each wagon owns its stable entity ID, rail node, entry direction, cargo, train
ID, and previous/next membership links. Uncoupled wagons use train ID zero and
remain stationary.

Coupling is explicit and command-driven. Only an uncoupled wagon on the rail
immediately behind the current rear vehicle may be appended. Its traversal must
lead to that rear vehicle, which validates curved as well as straight chains.
Rear-only decoupling leaves the detached wagon on its rail. Middle insertion,
middle removal, multiple locomotives, routing, schedules, signals, reservations,
and station freight are intentionally deferred.

## Movement and conflicts

The front locomotive is the only vehicle that evaluates current switch state.
At the four-tick movement threshold the entire train commits one atomic shift:
the locomotive enters its forward destination and every wagon enters the exact
former rail position of its predecessor. Consequently a switch change beneath
the trailing portion cannot split a train; trailing vehicles follow historical
occupied positions rather than independently choosing a branch.

Trains plan in ascending locomotive entity ID order. A front destination must
be empty at the start of planning and may be won by only one train. Simultaneous
vacate-and-enter chains and swaps remain conservatively blocked. Failure moves
no member and retains ready progress.

## Cargo, construction, and persistence

A Cargo Wagon has a checked single-item inventory of 100 units. Movement and
coupling never modify cargo. Nonempty wagons cannot be demolished; coupled
wagons and locomotives with followers must first be uncoupled. Rail or switches
occupied by any rail vehicle cannot be demolished. Cargo Wagons cost eight
construction units through the normal construction/depot path.

Snapshot version 23 stores locomotive membership heads/counts and wagon rail,
direction, membership, and cargo fields. Positions, networks, activity, and
forward destinations remain derived. Loading emits no movement or coupling
events. Presentation exports consist identity and cargo values; Godot only
renders and submits typed commands.

Long consists protect every derived rail block occupied by any vehicle. An old
block becomes available only after the rear wagon leaves it; see
`train-reservations.md`.
