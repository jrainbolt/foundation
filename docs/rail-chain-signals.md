# Rail Chain Signals and Interlocking

Chain Signals are immutable-content construction entities with stable IDs,
position, and controlled direction. They use the ordinary signal attachment
convention, cost six construction units, split the physical block graph, and
derive attachment, blocks, aspect, occupancy, and reservation ownership.

An ordinary signal authorizes only the immediate downstream block. A Chain
Signal instead follows the train's already-authoritative route from the first
downstream block through nested Chain Signal boundaries to the first ordinary
signal exit, destination, or route end. The exit block is included so a train
cannot enter protected territory without somewhere safe to clear it.

The train owns an ordered, bounded list of future block IDs. Requests are
evaluated in ascending train ID. Every required block must be unowned and free
of another consist before the complete sequence commits; failure adds none of
the new blocks. Existing compatible ownership counts toward the request.
Reservation ownership never changes routing or switch state.

Switch alignment remains authoritative and manual. A train may hold its full
chain route while stopped at a misaligned switch. Physical occupancy protects
all blocks occupied by the consist, and future locks are released only when no
vehicle occupies them and they are no longer required. Topology mutations
conservatively release future locks before rebuilding derived blocks.

The signal aspect is conservative and route-agnostic: green means its immediate
downstream block is available, reserved identifies unique reservation ownership,
and red means occupied or disconnected. Route-specific permission is exposed on
train inspection as chain status, required count, reserved count, blocking block,
and blocking train.

Snapshot version 27 stores Chain Signal infrastructure and each train's ordered
reservation sequence. Derived chain status/count, topology, aspect, occupancy,
and blockers are rebuilt without load-time events. Duplicate, nonexistent,
unordered, cross-owned, or route-incompatible reservations are rejected.

This system prioritizes safety over liveness. Automatic switch throwing,
alternative-route search, fairness, priority aging, and deadlock recovery remain
deferred.
