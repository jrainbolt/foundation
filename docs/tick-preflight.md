# Simulation tick allocation preflight

Every public simulation tick performs an allocation preflight before clearing
events or applying FIFO commands. The preflight computes conservative upper
bounds from current component counts plus every queued command. A placement is
counted even when it will later fail validation; demolitions never reduce the
bound.

The preflight grows authoritative component stores and the entity manager to
the maximum batch size, then allocates transferable blocks for the complete
power, fluid, and heat rebuilds. Bounds include power poles, all possible pole
edges, generators, consumers, accumulators, networks, allocation scratch,
pipes, fluid ports, fluid networks, Heat Conductors, heat ports, and heat
networks. Addition, multiplication, and byte-size conversions are checked.

Topology rebuilds obtain their arrays from that successful preflight plan.
They do not call the heap allocator after command mutation. Each rebuild still
constructs a separate derived state, validates/populates it, then replaces the
active state. Scratch arrays are released after use; active topology arrays
remain simulation-owned until their next replacement or destruction.

If any reservation fails, the tick returns `FACTORY_RESULT_OUT_OF_MEMORY`
before commitment. Commands remain queued in their original order, the clock
does not advance, the previous event batch remains visible, and authoritative
state plus active derived topologies remain unchanged. Retrying the tick after
memory becomes available processes the same commands normally.

This differs from an ordinary command validation failure: rejected commands
produce their normal per-command result during a successfully preflighted
tick. An invariant or arithmetic failure after preflight is an engine error,
not a command rejection and not an allocation-recovery mechanism.

Capacities, unused reservation blocks, and allocator-test controls are
transient implementation details. They are excluded from canonical snapshots
and presentation. Snapshot loading may rebuild topology directly before a
public tick; those direct reconstruction paths retain their local transactional
allocation behavior.
