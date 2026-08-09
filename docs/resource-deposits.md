# Finite resource deposits

Procedural generation creates multi-cell Iron and Copper patches directly from
ordinary finite deposit cells. A patch has no runtime object or shared quantity;
all depletion, occupancy, conservation, and snapshot rules below remain
unchanged.

Iron and copper deposits are authoritative world-tile state. Each deposit has
an integer resource type, position, remaining `FactoryResourceQuantity`, and
ordinary tile occupancy. Valid initial quantities are 1 through
`FACTORY_RESOURCE_QUANTITY_MAX`; zero is the derived depleted state reached by
extraction. There is no separate depletion flag in persistent state.

An Extractor converts exactly one deposit unit into one matching raw item per
completed 20-tick cycle. It first requires power, an empty output, a compatible
deposit, and at least one remaining unit. Incomplete, unpowered, blocked, or
depleted cycles consume nothing. The final complete cycle reaches zero
exactly; progress then remains stopped at zero until conditions can again
permit work—which depletion never does without a different world-creation
operation.

The output commit and one-unit world consumption form one simulation
transition. `PRODUCTION_COMPLETED` describes the created raw item. If the same
transition changes quantity from one to zero, `RESOURCE_DEPLETED` follows it
and identifies the extractor, resource type, and deposit coordinates. It is
not repeated on later ticks.

Extractor placement is allowed on a typed zero-quantity deposit. Demolition
clears only occupancy and never restores or removes the deposit. This keeps
world structure independent from machines and permits future deterministic
generation to create distant deposits from `(type, position, quantity)`.

World snapshots already serialize every tile's resource type, exact quantity,
and occupancy, so this milestone does not change snapshot version 17. Loading
does not extract, regenerate, or emit events. Presentation retains depleted
deposit records, exposes their derived state, and lets renderers distinguish
rich, low, and empty tiles without becoming authoritative.

Finite independent tiles are the basis for future procedural patches, remote
mining outposts, and long-distance rail logistics. No shared patch quantity,
terrain generation, or train-specific state is included here.
