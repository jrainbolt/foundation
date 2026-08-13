# Deterministic train routing

Foundation owns train destinations and accepted routes. A frontend submits typed
commands and inspects the result; it never chooses track or advances a route.

An accepted route is an ordered array of `(rail_entity_id, entry_direction)`
states. Index zero is the locomotive's state when planning succeeds. The current
index always identifies the locomotive's present route state, and the following
element is its next intended rail. The destination is a station entity, while
the final route state is that station's attached rail.

Routes are computed once by breadth-first search when a destination is set or an
explicit replan is requested. Search nodes include entry direction so curves and
switches cannot be traversed geometrically but illegally. Candidate successors
at each node are ordered by ascending rail entity ID, making equal-length route
selection independent of component storage and allocation history. Every edge
has unit cost, so the first target found is a deterministic shortest path.

Search uses physical switch connectivity: stem-to-either-branch and either-
branch-to-stem are candidates, while branch-to-branch is not. Movement remains
separate and uses the selected switch branch. A routed train waits with
`FACTORY_LOCOMOTIVE_BLOCKED_SWITCH` if the selected branch does not match its
next route state. It never changes a switch or takes another branch implicitly.

Trains with no destination retain the original free-movement behavior. A routed
train stops when its locomotive reaches the station attachment rail, retains its
destination, changes to `FACTORY_TRAIN_ROUTE_ARRIVED`, and emits one arrival
event. Clearing a destination removes the route without changing position or
movement progress.

Each transition is checked against current physical topology before movement.
A missing station, rail, or transition changes the route to
`FACTORY_TRAIN_ROUTE_INVALID`, emits one invalidation event, and stops the train.
There is no automatic replanning. The explicit replan command atomically replaces
the route only when a new path succeeds; failure preserves the old route.

Route planning allocates bounded temporary BFS storage and constructs the full
replacement route before changing authoritative state. Allocation or path-search
failure therefore cannot partially replace a route. The accepted route is owned
by the locomotive and freed on replacement, clear, demolition, reset, or world
destruction.

Snapshot version 24 stores destination, status, route length, current index, and
every directional route step. BFS scratch, topology network IDs, and presentation
data are excluded. Loading validates bounds and active route transitions; it does
not recompute or repair routes.

Current limitations are deliberate: routing does not reserve track, avoid other
trains during search, detect deadlocks, control switches, transfer freight, or
implement schedules. Existing occupancy conflict resolution remains authoritative
when independently planned routes overlap.

Block permission is a separate one-block-ahead layer documented in
`train-reservations.md`; a route remains intended direction rather than ownership.
Ordinary signals do not alter route search or switch selection. They split the
derived exclusive block map, so route execution obtains permission through that
same reservation layer before crossing a signal-defined boundary.
# Chain interlocking

Routing remains independent of interlocking. Chain Signals consume the installed
ordered route and never rerun BFS, choose another exit, change a destination, or
throw a switch. Destination replacement transactionally installs a new route and
releases reservations belonging to the old route before later planning.
