# Deterministic rail blocks and train reservations

Rail blocks are derived from physical rail topology. A switch, a station
attachment rail, a dead end, or any node whose physical connection count is
not two is a one-node boundary block. Remaining two-connection rails form
maximal connected blocks. A block ID is its lowest member rail entity ID and
members are inspected in ascending rail-ID order. Blocks are never serialized.

Ordinary directional Rail Signals additionally cut their protected physical
transition, allowing players to subdivide corridors. Topology mutation releases
and deterministically replans future reservations; see `rail-signals.md`.

Routing, reservation, and occupancy are distinct. Each routed train owns an
ordered bounded list of authoritative future block IDs; `reserved_block_id`
remains the compatibility view of its first entry. Before movement, trains are
processed in ascending locomotive/train ID. A still-needed reservation is
retained; otherwise the train requests its immediate next route block. The
lowest-ID requester wins an unowned block that no other consist occupies.
Every block occupied by any vehicle remains protected until the rear vehicle
leaves it. Same-block movement needs no reservation.

Ordinary boundaries retain immediate one-block behavior. Chain boundaries
atomically request every routed block through the next ordinary-signal exit,
including that exit block. Nested chains do not terminate lookahead. Entered
blocks remain protected by physical occupancy while future entries remain held.
Route replacement, clearing, invalidation, arrival, demolition,
or removal of the derived block releases obsolete ownership. Reservation does
not throw switches: a permitted train can still wait for manual alignment.

Free-moving trains continue within their current block but do not request a
future block and therefore stop at boundaries. Reserve/release events occur
only on ownership transitions; `entity_id` is the train and `quantity` is the
block ID. Waiting emits no event.

Snapshot version 27 stores the ordered reservation sequence. Derived chain
status/count, membership, occupancy protection, and blockers are reconstructed
without events. Invalid, duplicate, cross-owned, unordered, or route-incompatible
ownership is rejected.

This system provides exclusive permission and collision safety, not liveness.
Fairness, priority aging, deadlock recovery, and automatic switch throwing are
deliberately deferred.
