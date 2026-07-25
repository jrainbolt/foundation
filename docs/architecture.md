# Architecture

The runtime data flow is:

```text
Frontend
    → Command queue
    → Simulation tick
    → Gameplay systems
    → Read-only state inspection
```

The simulation owns its command queue, entity manager, extractor, belt,
refinery, assembler, and storage records, transfer plans, and command results. It
borrows a caller-owned world for its lifetime.
Frontends submit value-type commands and inspect tiles or copied extractor
records; they do not receive mutable gameplay arrays.

The simulation remains authoritative and independent of rendering. Render
frame timing, platform dependencies, and presentation state cannot influence
simulation outcomes. Console, Godot, SDL, and Raylib frontends can therefore
drive the same core without becoming engine dependencies.

Update order is centralized and explicit rather than dynamically registered.
Belt movement separates read-only planning from conflict resolution and atomic
ownership commits, keeping outcomes independent of storage order.

Resources, items, and recipes are shared data definitions. Transfers carry
item values generically; extractors map deposits to ore, refineries validate
their selected recipe, and storage validates supported item types.

All public headers live under `include/foundation/`. Existing C symbols retain
the historical `Factory` and `factory_` prefixes.

Lifecycle mutation is command-driven. Demolition validates all ownership and
empty-state invariants before clearing the tile, removing the subsystem record,
and invalidating the monotonic entity ID.

Splitters participate in the existing transfer plan as one-item consumers and
producers. Their routing decision is local deterministic state rather than
pathfinding.
