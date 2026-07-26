# Deterministic steam-powered generation

A steam engine is one simulation entity composed from three authoritative
records: an explicit vapor-only steam input storage, a west-facing port naming
that storage slot, and the existing power-generator component. It has no
burner, water, heat, pressure, turbine, or electrical buffer.

The immutable basic generation recipe maps 100 steam units to 100 electrical
energy units and caps output at 100 units per tick. The current ratio is
exactly one-to-one, enabling continuous integer conversion without fractional
remainders. The recipe and cap remain catalog data rather than renderer logic.

Generation is demand-aware. During power-network rebuild the engine advertises
the lesser of stored steam and its 100-unit output cap. The existing power
allocator assigns complete consumer demands in ascending entity-ID order.
During the existing generator-consumption pass, the engine consumes exactly
the allocation dispatched to it and emits one conversion event. Disconnected
engines, zero-demand networks, unused capacity, and allocations too small to
power the next consumer consume no steam.

Relevant tick ordering is:

1. commands and topology-changing placement or demolition;
2. fluid topology rebuild and deterministic pipe transport;
3. burner energy release and boiler/water production;
4. steam-engine latest-activity reset;
5. power topology rebuild and demand allocation;
6. source consumption, including steam engines;
7. remaining production and logistics.

Steam transported into an engine during step 2 is therefore available for
same-tick power allocation. Boiler output produced during step 3 becomes
network-transportable on the following tick because fluid transport has
already completed.

Snapshot version 10 stores the engine identity, position, recipe, generator
component, and explicit steam storage. Fluid and power network IDs are
reconstructed. Latest-tick generation activity, events, and presentation are
transient. Snapshot-chain identity and continuation tests cover engines
connected to both network types.

This milestone intentionally excludes turbines, pressure, temperature, heat
transfer or loss, condensers, cooling, pumps, chemistry, oil processing,
nuclear systems, pollution, research, and steam or turbine animation.
