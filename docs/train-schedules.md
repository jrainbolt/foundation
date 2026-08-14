# Deterministic train schedules

Each Locomotive owns an authoritative, fixed-capacity schedule of at most 16
stops. A stop stores only a stable Rail Station entity ID, a wait-condition
enumerator, and its canonical integer value. Schedule storage is part of the
train's simulation state; route topology, block reservations, and presentation
records remain derived observer state.

Schedule edits are FIFO commands. Stops may be added, removed, or cleared only
while automatic scheduling is disabled. Enabling requires at least one stop and
immediately asks the existing directional route planner for stop zero. Manual
destination changes are rejected while scheduling is enabled. Disabling keeps
the current route and destination, allowing the train to finish or be managed
manually without an implicit teleport or route mutation.

## Tick order and waits

The rail tick order is movement, station freight transfer, then schedule
evaluation. An arrival tick never counts toward a wait. On later ticks:

- `NONE` completes immediately;
- `TIME` completes after exactly its positive configured number of evaluation
  ticks (maximum 1,000,000);
- `CARGO_EMPTY` completes when every coupled Cargo Wagon is empty, including a
  zero-wagon train;
- `CARGO_FULL` completes only when the consist has at least one Cargo Wagon and
  every wagon is at capacity.

Cargo conditions inspect the authoritative consist in membership order. Station
freight therefore may satisfy a condition before schedule evaluation in the
same tick, except on the protected arrival tick. Completion emits
`TRAIN_WAIT_COMPLETED`, advances modulo the schedule length, emits
`TRAIN_SCHEDULE_ADVANCED`, and invokes the ordinary route planner for the next
station. Trains are evaluated in ascending train entity ID, so simultaneous
schedule transitions have deterministic event and reservation order.

An unavailable route never skips a stop. The schedule enters
`ROUTE_UNAVAILABLE`, retains its current index, and stops safely. The existing
explicit replan command retries the current destination; schedule stops are not
silently replayed or replaced.

## Ownership, snapshots, and presentation

Snapshot version 29 stores the enabled flag, current stop, wait progress,
schedule status, and all 16 canonical fixed slots. Unused slots must be zero.
Load validates enum bounds, time bounds, indices, and canonical unused storage.
Routes and reservations are rebuilt by the established derived-topology load
path. Loading emits no arrival, wait, or advancement events.

The public inspection API returns schedule summaries on locomotive records and
copies individual stop records by index; no mutable schedule pointer is exposed.
Presentation exports the same compact integer state. Godot reads it and submits
ordinary schedule commands through its inspector; it does not own or advance a
second schedule.

This milestone deliberately does not include station names, multiple schedules,
conditional branching, circuit conditions, refueling policy, timetables, train
priorities, or automatic station selection.
