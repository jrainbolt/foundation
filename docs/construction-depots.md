# Construction depots

Construction material is `FACTORY_ITEM_CONSTRUCTION_MATERIAL` and uses the
ordinary Storage, Belt, Inserter, and logistics-endpoint ownership path. A
Construction Depot accepts only that item, stores at most 500 units, has no
output, and requires no power.

The simulation's former global construction inventory is a finite bootstrap
reserve only until the first depot commits. The first depot's cost is paid
from that reserve and every remaining unit transfers into the depot. The
transition is rejected atomically if the remainder would exceed the depot's
500-unit capacity. After the transition the reserve is zero permanently and
all placement supply comes from physical depots.

Coverage uses Manhattan distance from the depot tile. A one-tile footprint at
`abs(x-depot_x)+abs(y-depot_y) <= 8` is covered. Future multi-cell footprints
must have every cell covered. Among covering depots that can independently pay
the complete immutable content cost, the lowest stable entity ID supplies the
placement. Costs are never split. Terrain, occupancy, and unlock checks occur
before source selection, and deduction occurs only after entity placement
commits.

Demolition refunds the complete cost to the lowest-ID covering depot with
capacity. Refunds are not split. Without an eligible destination demolition
fails. A depot must be empty before demolition, cannot refund into itself, and
the final depot cannot be demolished. Bootstrap ownership is never resurrected
and no material is discarded.

Snapshot version 19 stores depot position, ID, material quantity, construction
material held by Storage, command source/refund depot IDs, the bootstrap
reserve, and a permanent bootstrap-completed bit in the existing metadata
word. A pre-bootstrap snapshot must contain no depots; a completed-bootstrap
snapshot must contain a zero reserve. Coverage overlays and telemetry remain
derived and transient.

This establishes physical remote-outpost supply without train-specific state.
Future rail logistics can transport the same ordinary construction item to
distant depots.

Static rails and Rail Stations use this physical supply path. Each tile is an
independent transaction: a rail costs 2 units and a station costs 30, selected
from the lowest-ID eligible depot. There is no free or bulk-build discount and
failed validation consumes nothing.

Rail Switches cost 5 units and use the same lowest-ID covering-depot rule.
Invalid terrain, occupancy, geometry, branch configuration, insufficient
supply, and out-of-coverage attempts never partially deduct material.
