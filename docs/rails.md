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
