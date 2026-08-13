# Deterministic rail blocks and train reservations

Rail blocks are derived from physical rail topology. A switch, a station
attachment rail, a dead end, or any node whose physical connection count is
not two is a one-node boundary block. Remaining two-connection rails form
maximal connected blocks. A block ID is its lowest member rail entity ID and
members are inspected in ascending rail-ID order. Blocks are never serialized.

Routing, reservation, and occupancy are distinct. Each routed train may own
one authoritative future `reserved_block_id`. Before movement, trains are
processed in ascending locomotive/train ID. A still-needed reservation is
retained; otherwise the train requests its immediate next route block. The
lowest-ID requester wins an unowned block that no other consist occupies.
Every block occupied by any vehicle remains protected until the rear vehicle
leaves it. Same-block movement needs no reservation.

Entering the reserved block releases the future slot because occupancy then
protects it. Route replacement, clearing, invalidation, arrival, demolition,
or removal of the derived block releases obsolete ownership. Reservation does
not throw switches: a permitted train can still wait for manual alignment.

Free-moving trains continue within their current block but do not request a
future block and therefore stop at boundaries. Reserve/release events occur
only on ownership transitions; `entity_id` is the train and `quantity` is the
block ID. Waiting emits no event.

Snapshot version 25 adds the train-owned reserved block ID. Derived membership,
occupancy protection, waiting status, and blocker are reconstructed after load
without events. Invalid, duplicate, or route-incompatible ownership is rejected.

This system provides exclusive permission and collision safety, not liveness.
Fairness, priority aging, signals, deadlock recovery, automatic switch throwing,
and multi-block interlocking are deliberately deferred.
