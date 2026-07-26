# Deterministic heat networks and heat exchangers

Heat remains authoritative only in entity-owned `FactoryHeatStorage`
components. Reactor cores own the only heat storage in this milestone.
`FactoryHeatPort` records expose reactor sources and exchanger consumers but
own no heat. Derived heat networks contain topology and latest-tick accounting
only; they never contain heat, pending transfers, residuals, or fractions.

Heat Conductors are passive cardinal grid entities. Their connection masks and
network IDs are rebuilt from placement. Adjacent conductors and ports form
connected components. A component's stable network ID is its minimum conductor
entity ID. An isolated conductor is a valid one-conductor network using its own
ID. Ports without an adjacent conductor are disconnected.

Topology rebuild allocates a complete temporary derived state, sorts
conductors and ports by stable IDs, repeatedly propagates minimum component
IDs across conductor adjacency and ports, then atomically replaces the prior
derived state. Placement, demolition, snapshot load, merges, and splits use
the same reconstruction.

A Heat Exchanger owns two generic fluid storages and ports:

- `HEAT_EXCHANGER_INPUT`: water-only, capacity 1,000, west-facing;
- `HEAT_EXCHANGER_OUTPUT`: steam-only, capacity 1,000, east-facing.

It owns a heat-input port but no heat storage, burner, reactor, or generator.
The single atomic recipe consumes 100 heat and 100 water and produces 100
steam. One cycle may complete per tick.

After reactor generation, exchangers are processed by ascending entity ID.
For each connected exchanger, reactor source ports on the same network are
processed by ascending reactor ID. Sufficient heat is preflighted before any
mutation. Water identity/quantity and complete steam output capacity are also
validated. Only then is exactly 100 heat withdrawn across sources, 100 water
removed, and 100 steam committed. Failed attempts mutate nothing and emit no
transfer or completion event.

Newly generated reactor heat is eligible in the same tick. Because conversion
follows reactor generation, a reactor that began the tick full remains blocked
for that generation phase; exchanger withdrawal creates capacity that allows
fuel burn to resume on the following tick.

Activity precedence is: disconnected heat, insufficient heat, insufficient
water, insufficient steam capacity, working, then idle. Latest transfer and
conversion fields reset each tick. Fluid arrival and departure use the generic
fluid network; there is no exchanger-specific fluid transport.

Snapshot version 12 stores conductor/exchanger identity and position plus
generic water/steam storage state. Heat ports, fluid ports, topology, masks,
network IDs, latest quantities, activity, events, and presentation are rebuilt
or reset. Loading never transports heat or fluid and never performs conversion.

Demolition discards entity-owned exchanger water and steam under the existing
removal policy and marks both derived topologies dirty. Conductors own no
resource. This milestone excludes turbines, nuclear electrical generation,
cooling, condensers, pressure, temperature, gradients, heat loss, radiation,
waste, enrichment, breeding, reactor controls, meltdowns, and balancing.
