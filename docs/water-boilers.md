# Deterministic water extraction and boiler conversion

Water extractors and boilers are simulation-owned production machines. They
use only integer quantities and advance during the deterministic production
phase.

A water extractor owns one explicit aqueous output storage and one port. Its
five-tick cycle commits 100 water only when the full output fits. A blocked
cycle preserves ready progress and emits no event; once capacity is available,
the next production phase commits the full batch and emits one
`WATER_PRODUCED` event.

A boiler owns an existing solid-fuel burner, an explicit aqueous input storage,
an explicit vapor output storage, and separate west/east ports. The immutable
`BOIL_WATER` recipe consumes 100 water and 100 released burner-energy units to
produce 100 steam. Validation of the recipe, input fluid, input quantity,
released energy, output type, and output capacity precedes an allocation-free
commit. Failure changes none of the three authoritative resources.

Tick ordering is command application, fluid-topology rebuild, network
transport, burner energy release, fluid-machine production, power allocation,
and the remaining production/logistics systems. This lets pipe-delivered water
and energy released in the same tick participate in a boiler conversion.
Machine stores and network ports are visited in their deterministic stable
order; no event sorting pass is used.

Snapshot version 9 serializes extractor progress, boiler recipe/activity,
boiler burner state, and every slotted fluid storage. Ports and fluid networks
are derived after load. Events and presentation remain transient, and
save/load continuation produces the same future per-tick event batches.

This milestone intentionally excludes steam engines, turbines, electrical
generation from steam, pumps, pressure, chemistry, heat simulation, oil
processing, pollution, nuclear systems, and research.
