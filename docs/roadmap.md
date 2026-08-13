# Roadmap

- [x] Deterministic world, commands, entities, and ticks
- [x] Extraction, belts, storage, refineries, and assemblers
- [x] Empty-only demolition and tile reuse
- [x] One-input, two-output splitters
- [x] Deterministic round-robin routing and blocking fallback
- [x] Inserters
- [x] Machine interaction
- [x] Belt pickup and belt drop
- [x] Cardinal placement orientation
- [x] Internal entity-and-slot logistics endpoints
- [x] Shared atomic ownership transfer operations
- [x] Simulation-owned construction inventory
- [x] Fixed placement costs and full demolition refunds
- [x] Fixed assembler recipes and safe FIFO recipe selection
- [x] Counted generic assembler inputs and multi-item output
- [x] Configurable one-item storage output for inserters
- [x] Canonical versioned binary snapshots and deterministic continuation
- [x] Deterministic Factorio-style power networks
- [x] Deterministic transient simulation event stream
- [x] Deterministic renderer-neutral presentation snapshots
- [x] Deterministic research and technology progression infrastructure
- [x] Deterministic Research Labs and physical science logistics
- [x] Finite resource deposits, depletion, and conservation
- [x] Seeded deterministic procedural world model and authoritative terrain
- [x] Deterministic terrain generation and finite resource patches
- [x] Deterministic production and logistics telemetry
- [x] Physical Construction Depot supply for remote outposts
- [x] Deterministic static rail geometry, topology, and station attachment
- [x] Deterministic rail switches and branching junctions
- [x] Deterministic locomotive ownership and basic rail movement
- [x] Deterministic train consists and Cargo Wagons
- [x] Deterministic train destinations and directional routing
- [x] Derived rail blocks and one-block-ahead reservations
- [x] Ordinary directional Rail Signals and explicit block boundaries
- [x] Chain Signals and deterministic junction interlocking

Finite deposits and seeded authoritative terrain are prerequisites for
deterministic resource patches, remote mining outposts, rail infrastructure, and eventually
deterministic trains, routing, reservations, and stations.
- [x] Unified deterministic immutable content definitions
- [ ] Inserter variants, filters, and longer arms

Godot bindings, incremental synchronization, and other frontend integrations
remain separate future milestones.

Rail progression remains deliberately staged: static rail topology → switches
→ locomotive movement → consists/wagons → destinations/routing → rail
blocks/reservations → ordinary rail signals → chain signals/interlocking → freight stations
→ schedules/automated logistics.
