# Deterministic rail infrastructure

Rails are authoritative one-cell entities. Each stores only its stable entity
ID, integer grid position, and one explicit geometry: horizontal, vertical, or
one of the four cardinal curves. Geometry derives exactly two ports using the
public N/E/S/W bit mask; it is never inferred from neighboring cells.

Two rails connect only when they are cardinally adjacent and expose opposing
ports. Derived neighbors are stored in North, East, South, West order. A rail
network is a connected component whose ID is the lowest rail entity ID in that
component. Isolated rails are valid one-member networks, loops are supported,
and no junction or switch geometry exists yet.

Rail and station topology is rebuilt after commands using storage reserved by
global tick preflight. Once preflight commits, rebuilding allocates no heap
memory and replaces the previous derived topology only after the new topology
is complete. Component store order does not define graph order or IDs.

Rail Stations are separate one-cell entities on Ground. Their orientation is
the cardinal direction of the rail they expect: a station facing East requires
a rail in its east neighbor at placement time. Only station ID, position, and
orientation are authoritative. Attached rail ID, connected state, and network
ID are derived. Removing the rail leaves the station present but disconnected;
placing a rail in the expected cell reconnects it.

Both entities use ordinary occupancy, terrain, FIFO command, event, demolition,
and physical Construction Depot rules. Rails cost 2 construction material per
tile and stations cost 30. Ground permits placement; Water and Rock return the
generic terrain-blocked result. Rails may be isolated, but initial station
placement requires an adjacent rail.

Snapshot version 20 serializes rail ID/position/geometry and station
ID/position/orientation. Port masks, adjacency, network membership, and station
attachment are excluded and rebuilt without events after load. Presentation
exports the same read-only derived graph facts for renderers. Construction and
demolition use the existing events; no rail-specific topology events are added
in this milestone.

This graph is the future train traversal substrate: consumers can inspect a
rail's stable geometry, compatible cardinal neighbors, and component without
reconstructing topology from renderer coordinates. Switches, rolling stock,
movement, occupancy, routing, reservations, signals, freight endpoints, and
schedules remain deferred.

## Rail Switches

Rail Switches are authoritative one-cell, three-port graph nodes. Four stable
geometries use North, South, East, or West as the stem; Branch A and Branch B
are the remaining two ports in the explicit immutable geometry definition.
Branch A is the deterministic construction default. Switches cost 5
construction material and otherwise use ordinary terrain, occupancy, depot,
demolition, and construction-event rules.

Physical topology and traversal state are intentionally separate. All three
compatible physical neighbors always participate in adjacency and connected
components, and switch IDs participate in the lowest-node-ID network rule.
Changing branch selection never adds/removes edges or rebuilds network
membership. Traversal permits stem ↔ selected branch only; the unselected
branch is blocked and branch-to-branch traversal is never allowed. Both entry
and exit neighbors must physically exist. Ordinary two-port rails traverse
from one physically connected port to the other.

`factory_simulation_get_rail_traversal` is a read-only, train-neutral query.
`FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH` changes authoritative selection in
FIFO order. Same-state requests are successful no-ops; actual transitions emit
one `FACTORY_EVENT_RAIL_SWITCH_CHANGED` carrying previous and new branch IDs.

Snapshot version 21 serializes switch ID, position, geometry, and selected
branch. Connection masks, neighbors, networks, and traversal results remain
derived. Switches add branching topology only: rolling stock, automatic
switching, occupancy, routing, reservations, signals, and schedules remain
deferred.

## Locomotives and rail-vehicle occupancy

A Locomotive is a simulation entity but not a topology node or ordinary tile
occupant. Its authoritative state is its stable ID, occupied rail-capable
entity ID, entry direction, and integer movement progress. Position, network,
next rail, travel direction, occupancy, and activity are derived.

Progress advances once per tick and a locomotive attempts one transition at
`FACTORY_LOCOMOTIVE_MOVE_TICKS` (four). A blocked locomotive retains completed
progress and retries next tick. Movement calls the generic traversal API;
switch logic is not duplicated and switches are never thrown automatically.
Commands and topology rebuilding precede movement, so a switch command affects
movement in the same tick.

Planning uses start-of-phase occupancy and ascending locomotive IDs. A target
must initially be empty, the lowest ID wins a shared target, and swaps or
movement chains cannot pass through occupied nodes. Occupied rail and switches
cannot be demolished. Removing a locomotive preserves its underlying rail.

Snapshot version 22 stores only locomotive ID, occupied rail ID, entry
direction, and progress. Wagons, consists, routing, signals, reservations,
schedules, freight, fuel, and station stopping remain deferred.
