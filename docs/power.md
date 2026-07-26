# Deterministic power networks

Generators are considered in ascending stable entity-ID order regardless of
type; there is no global rule preferring atomic generators (Steam Turbines)
over continuous ones (basic generators, Steam Engines, solar) or vice versa.
For each consumer, in that same ascending order, a continuous generator may
contribute any bounded amount up to what remains of its availability, while
an atomic generator not yet fired contributes only its complete output
quantum. If a later atomic generator's quantum alone would cover the
consumer's complete demand, any partial continuous amount tentatively drawn
ahead of it is discarded first: a lower-ID continuous generator's partial
contribution never strands a self-sufficient atomic generator behind it, and
is never spent once a self-sufficient atomic generator is found. A plan
commits only once a consumer's complete demand is met; accumulator charging
is attributed to specific generators' leftover capacity the same way,
ascending generator ID, after consumer allocation.

A complete atomic cycle may fire when only part of its output ends up
committed to consumers and accumulator charging combined. The full input
resource for that cycle is always consumed in full the moment any part of it
is needed, and any output beyond what was committed to a consumer or an
accumulator is recorded as unused generation. Generation consumption
executes this exact per-generator plan -- a continuous generator produces
exactly its committed output, and a generator that fired produces its
complete quantum -- never re-deriving an amount from a network total.

Power uses integer units and Factorio-style pole coverage. A pole covers
machines within Chebyshev distance 3 and connects automatically to every pole
within Chebyshev distance 6. Connections are undirected and derived each tick.
The renderer-facing edge list stores `pole_a < pole_b` and is ordered by
ascending `pole_a`, then `pole_b`.

Connected pole components form networks. Discovery begins with poles in
ascending entity-ID order, and a component's network ID is its lowest pole ID.
A consumer or generator attaches to the lowest-ID covering pole, even if
another pole is nearer. It belongs only to that pole's component.

Basic generators can provide up to 100 units per tick when covered by a pole.
Their available generation is the lesser of this maximum and their burner's
released energy. Network
aggregation asks the internal power-source boundary for each connected
source's available output; topology and allocation do not calculate a basic
generator's production themselves. Fixed consumer demands are:

| Consumer | Demand |
|---|---:|
| Extractor | 10 |
| Refinery | 20 |
| Assembler | 25 |
| Inserter | 5 |

Every connected consumer reserves its full demand regardless of activity.
Within each network, consumers are considered by ascending entity ID. A
consumer receives either its complete demand or zero. A consumer too large for
the remaining generation is skipped, allowing a smaller later consumer to use
the leftover power.

Commands apply before power discovery, so pole, generator, and demolition
changes affect allocation in the same tick. Powered machines advance normally.
Unpowered extractors, refineries, assemblers, and inserters preserve their exact
progress, buffers, state, and ownership. Belts, splitters, storage, and storage
output remain passive and consume no power.

There is no implicit or compatibility power source. A consumer is unpowered
when the world has no poles or generators, when no pole covers it, or when its
connected network cannot allocate its full demand.

Power poles cost 3 construction units and basic generators cost 30. Generators
own a generic solid-fuel burner; see `burners.md`. Removing poles can split
networks; adding bridge poles can merge them.
All topology and allocation inspection is rebuilt deterministically.

Public inspection exposes poles, generators, consumers, networks, and canonical
connections. Network totals use 64-bit `FactoryPowerTotal`.

Limitations are intentional: no manual wires, switches, batteries, fluids,
solar, voltage, losses, priorities, or brownout speed scaling.
