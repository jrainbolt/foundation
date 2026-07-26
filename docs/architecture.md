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
refinery, assembler, splitter, inserter, and storage records, transfer plans,
and command results. It borrows a caller-owned world for its lifetime.
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

Inserters add an active state machine above the same ownership model. Pickup
and drop have independent plan, contention-resolution, and commit phases.

Shared logistics operations flow from simulation planning into a private
endpoint layer and then into entity-specific stores. An endpoint combines an
entity ID with a logical slot, avoiding duplicated type dispatch while keeping
routing, geometry, scheduling, and processing in their owning systems.

The simulation directly owns a small construction inventory. Commands are its
only runtime mutation path: grants credit it, successful placement spends it,
and successful demolition refunds it. Fixed costs use one immutable lookup.

Assembler processing and logistics share one private immutable recipe table.
Public queries copy recipe definitions; internal consumers use stable table
entries without exposing mutable pointers. Assembler input endpoints are
generic logical slots whose item and capacity derive from the selected recipe.

Storage keeps inventory ownership separate from its one-item output buffer.
The storage subsystem performs the inventory-to-buffer transition; the endpoint
layer exposes only that buffer to inserters.

The snapshot module depends on simulation internals but subsystems do not depend
on serialization. Its central explicit encoder/decoder rebuilds pointer-backed
arrays in a temporary simulation, then runs cross-subsystem validation before
publishing the result.

Power discovery is a derived layer between command application and active
machine updates. Authoritative pole and generator stores feed a canonical edge
list, connected components, attachment lookup, and whole-consumer allocation.
Machines query the allocation instead of searching for poles independently.
Network aggregation identifies connected source entities and asks
`factory_power_source_available_generation` for each source's current output.
It does not calculate or depend on the basic generator's production mechanism.
