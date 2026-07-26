# Deterministic fluid networks

Pipes are authoritative entities containing only an ID and grid position.
They own no fluid. Tanks expose a `FactoryFluidPort` that names their existing
storage and permits all four cardinal directions. Boiler ports explicitly
name separate input and output storage slots, so the west water network and
east steam network cannot alias.
Ports are the only connection between storage-owning entities and networks.

Topology is marked dirty by pipe or port placement and demolition. At the
start of the next tick, pipes are sorted by ascending entity ID, cardinal
adjacency is discovered, and connected components are assigned the smallest
pipe entity ID in that component. Connection masks, ports, network summaries,
and network IDs are derived and are not serialized.

After a dirty rebuild, transport visits networks and ports in ascending stable
ID order. Each ordered storage pair can move at most
`FACTORY_PIPE_TRANSFER_RATE` (100) integer units per tick. The fuller storage
is the source; equal quantities preserve ascending-ID direction. Storage
validation prevents capacity overflow, underflow, class incompatibility, and
fluid mixing. Failed pairs are skipped without mutation. Networks never own a
hidden reservoir, so the sum of storage quantities is conserved.

Snapshot version 9 stores pipe entity IDs and positions plus the existing
storage state. Ports and network state are reconstructed before the loaded
simulation is returned, preserving continuation determinism.

This transport model intentionally has no pumps, pressure, iterative
equalization, flow animation, heat, chemistry, oil processing, nuclear
systems, pollution, or research.
