# Construction depots

Construction material is `FACTORY_ITEM_CONSTRUCTION_MATERIAL` and uses the
ordinary Storage, Belt, Inserter, and logistics-endpoint ownership path. A
Construction Depot accepts only that item, stores at most 500 units, has no
output, and requires no power.

The simulation's former global construction inventory is now a finite
bootstrap reserve. While no depot exists, placement spends that reserve. When
the first depot is constructed, up to its 500-unit capacity is transferred
from the remaining reserve into the depot; any excess remains authoritative
but inactive. While any depot exists, placements never fall back to the
bootstrap reserve.

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
the last depot's own refund returns to the bootstrap reserve so construction
can be bootstrapped again. No material is discarded.

Snapshot version 19 stores depot position, ID, material quantity, construction
material held by Storage, command source/refund depot IDs, and the bootstrap
reserve. Coverage overlays and telemetry remain derived and transient.

This establishes physical remote-outpost supply without train-specific state.
Future rail logistics can transport the same ordinary construction item to
distant depots.
