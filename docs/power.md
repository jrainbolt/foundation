# Deterministic power networks

Power uses integer units and Factorio-style pole coverage. A pole covers
machines within Chebyshev distance 3 and connects automatically to every pole
within Chebyshev distance 6. Connections are undirected and derived each tick.
The renderer-facing edge list stores `pole_a < pole_b` and is ordered by
ascending `pole_a`, then `pole_b`.

Connected pole components form networks. Discovery begins with poles in
ascending entity-ID order, and a component's network ID is its lowest pole ID.
A consumer or generator attaches to the lowest-ID covering pole, even if
another pole is nearer. It belongs only to that pole's component.

Basic generators provide a constant 100 units when covered by a pole. Network
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

Power poles cost 3 construction units and basic generators cost 30. Both have
no material state, may be demolished immediately, and refund their full fixed
cost. Removing poles can split networks; adding bridge poles can merge them.
All topology and allocation inspection is rebuilt deterministically.

Public inspection exposes poles, generators, consumers, networks, and canonical
connections. Network totals use 64-bit `FactoryPowerTotal`.

Limitations are intentional: no manual wires, switches, batteries, fuel,
fluids, solar, voltage, losses, priorities, brownout speed scaling, or stored
energy.
