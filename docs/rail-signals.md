# Deterministic ordinary rail signals

A Rail Signal is authoritative static infrastructure containing only its stable
entity ID, ground-cell position, and controlled travel direction. It occupies
the trackside cell to the right of its attached rail when viewed in the
opposite direction: equivalently, the attached rail is one cell to the signal's
left while looking along the controlled direction. For an eastbound signal,
the signal is south of the attached rail. The protected boundary is the
transition from the attached rail's west neighbor into that rail.

Placement requires ground, an empty signal cell, a rail-capable attached node,
and a physical upstream connection opposite the controlled direction. Two
signals may protect opposite directions using opposite trackside cells, while
duplicate attachment/direction pairs are rejected. Signals cost four
construction units and use normal depot selection, FIFO placement, demolition,
refund, occupancy, and construction events.

Signals cut their physical transition during derived block component building.
The boundary is non-directional for conservative exclusive block membership,
but the signal retains its direction for movement and future chain signaling.
Structural switch, station, and endpoint boundaries remain. Block IDs continue
to be the lowest member rail ID, so placing or removing a signal may change IDs.

Aspect is derived. `GREEN` means the downstream block is available, `RESERVED`
means it has a future reservation, and `RED` means it is disconnected or
physically occupied. A reserved aspect is not a universal stop: the owning
train may cross because movement authorization remains the reservation layer's
responsibility. Other routed trains wait, and free trains stop at the resulting
block boundary.

Any rail-topology mutation releases future reservations in stable train order
before rebuilding. Reservation planning then reacquires the new immediate block
in ascending train-ID order during the same tick. This conservative policy
prevents stale IDs and duplicate owners when signal removal merges blocks.
Long consists protect every newly derived block they physically span.

Snapshot version 26 stores signal ID, position, and orientation. Attachment,
boundary mapping, blocks, aspect, occupancy, and reservation display are
reconstructed. Invalid orientation, terrain, attachment, connection, or
duplicate direction/attachment is rejected without load-time events.

Ordinary signals do not throw switches, reserve routes, provide fairness, or
guarantee liveness. Chain signals, interlocking, multi-block lookahead,
deadlock handling, freight, and schedules remain deferred.
