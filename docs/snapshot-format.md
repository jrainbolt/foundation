# Snapshot format version 25

Version 25 adds one authoritative `reserved_block_id` to each locomotive
record. Rail-block membership, occupancy protection, waiting status, and the
blocking train are derived after load. Load rebuilds topology and rejects
missing, duplicate, arrived, or route-incompatible future reservations without
emitting events.

Version 23 adds authoritative train-consist membership and Cargo Wagon records.
Locomotive records now include the first wagon link and vehicle count. Cargo
Wagon records contain entity/rail IDs, entry direction, train and adjacent
membership IDs, and bounded cargo item/quantity. Derived position, network,
activity, and next-rail values remain excluded.

Version 22 added Locomotive records containing the entity ID, occupied rail
entity ID, entry direction, and movement progress. Derived position, network,
next rail, activity, occupancy, and presentation remain excluded.

Version 19 adds the authoritative Construction Depot section, construction
material in Storage records, and the supplying/refund depot ID in command
results. The bootstrap construction reserve remains in metadata.

Version 18 adds the authoritative 64-bit world seed to the world section.
Terrain was already stored per tile; the section now explicitly restores the
seed plus terrain without invoking generation. The world prefix is 16 bytes:
seed, width, and height.

Version 17 adds the authoritative Research Lab component section. Each record
stores entity ID, grid position, and Basic Science quantity. Power topology,
activity, and latest-tick contribution fields remain transient. Older versions
are strictly rejected under the existing snapshot policy.

Version 16 adds bounded authoritative research state to metadata and Basic
Science to storage records. Older snapshot versions remain strictly rejected.

Version 15 gives each Steam Turbine a second fluid storage/port pair for its
exhaust output (`FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT`), alongside the
existing input pair. This is an incompatible structural change from version
14: a version-14 snapshot's fluid-storage section has exactly one
turbine-owned record per turbine, but version 15 validates exactly two, so
older snapshots are rejected outright rather than silently misread.

Version 14 added the authoritative Steam Condenser component section.
Condenser steam and water remain in generic fluid storage; topology and
latest-tick fields are reconstructed or cleared.

Foundation snapshots are canonical little-endian binary files. Every value is
written field-by-field; no C structure, pointer, enum representation, padding,
timestamp, or process-specific value enters the format.

## Header

| Offset | Width | Field |
|---:|---:|---|
| 0 | 8 | Magic bytes `FOUNDATN` |
| 8 | 4 | Version (`19`) |
| 12 | 4 | Header size (`48`) |
| 16 | 8 | Total snapshot size |
| 24 | 8 | Payload size |
| 32 | 8 | Simulation tick |
| 40 | 4 | Section count (`28`) |
| 44 | 4 | Reserved zero |

Unsigned and signed integers use 32-bit or 64-bit two's-complement
little-endian encodings as shown. Booleans are 32-bit `0` or `1`; other values
are rejected. Enums use unsigned 32-bit stable identifiers and are validated.

## Sections

Each section starts with four 32-bit fields:

| Field | Meaning |
|---|---|
| Type | Stable section identifier |
| Version | Section version, currently `1` |
| Record count | Number of fixed-width records |
| Payload size | Bytes following the section header |

Version 19 requires each section exactly once in this order:

| Type | Section | Record width |
|---:|---|---:|
| 1 | Metadata | one 60-byte payload |
| 2 | Entity manager | 4 bytes per live ID plus 8-byte prefix |
| 3 | World tiles | 16 bytes plus 16-byte seed/dimensions prefix |
| 4 | Extractors | 36 |
| 5 | Belts | 24 |
| 6 | Splitters | 24 |
| 7 | Refineries | 48 |
| 8 | Assemblers | 64 |
| 9 | Inserters | 48 |
| 10 | Storage | 68 |
| 11 | Power poles | 12 |
| 12 | Power generators and optional burner payload | 44 |
| 13 | Fluid storages | 32 |
| 14 | Pipes | 12 |
| 15 | Water extractors | 16 |
| 16 | Boilers and burners | 48 |
| 17 | Steam engines | 16 |
| 18 | Solar generators | 12 |
| 19 | Accumulators | 20 |
| 20 | Reactor cores | 40 |
| 21 | Heat conductors | 12 |
| 22 | Heat exchangers | 12 |
| 23 | Steam turbines | 16 |
| 24 | Steam condensers | 16 |
| 25 | Research Labs | 16 |
| 26 | Construction Depots | 16 |
| 27 | Pending commands | 24 |
| 28 | Command results | 72 |

Unknown, reordered, duplicated, missing, incorrectly sized, or unsupported
sections are rejected. Exact full-buffer consumption is required.

Records retain subsystem array order because it is deterministic state. Entity
manager records preserve live-ID order and the next ID. World tiles are encoded
row-major. Commands use a type plus five explicit 32-bit payload fields; unused
fields are zero. Results contain that command encoding followed by result,
entity, position, construction, assembler-recipe, and storage-output fields.

Metadata contains tick, construction units, active research, completed bits,
and two fixed progress records. Unconsumed science exists only in ordinary
item inventories and Research Lab records. Storage records include all
ten item counters, capacity, output configuration, buffer item, and occupancy.
Assembler records include recipe, both generic
counted slots, processing fields, and counted output. Inserter records include
the complete state-machine state and source/destination coordinates.

Generator records include the burner fuel-class mask, input inventory, current
fuel, remaining burn ticks, and released available energy. The active fuel
definition and remaining ticks derive the exact unreleased energy and next
release position. Steam-engine generator records use canonical zero values for
the unused burner payload. Power edges, network membership,
attachments, and allocation are not serialized;
they are rebuilt from pole and generator positions after loading.

Fluid-storage records contain owner, stable storage slot, grid position,
accepted class mask, fluid type, quantity, and capacity. Empty storage is
canonically `NONE` with zero quantity; non-empty storage must reference a
valid compatible definition. Record count is dynamic: every fluid-owning
machine contributes one record per storage slot it owns -- one for a fluid
tank, two each for boilers, heat exchangers, and steam condensers, and (as
of version 15) two for steam turbines (input and exhaust output, where
version 14 validated only one).

Pipe records contain authoritative entity ID and grid position. Connection
masks, ports, networks, and network IDs are deterministically reconstructed.

Water-extractor records contain entity ID, grid position, and production
progress. Boiler records contain entity ID, position, recipe, latest
conversion-active state, and their complete burner state. Their explicit
fluid storages are carried by the fluid-storage section.

Steam-engine records contain entity ID, grid position, and stable generation
recipe ID. Latest-tick generation activity is transient and resets after load.

Solar-generator records contain entity ID and grid position. Intensity,
available generation, latest-tick output, and power-network membership are
derived and are not serialized.

Accumulator records contain entity ID, grid position, and 64-bit stored
energy. Capacity and rates are immutable definition values. Attachment,
network ID, charge/discharge activity, and latest-tick quantities are derived
or transient.

Reactor-core records contain entity ID, grid position, queued fuel ID and
quantity, active fuel ID, remaining 64-bit heat yield, and authoritative
64-bit stored heat. Heat capacity is an immutable reactor definition value.
Latest generated heat, activity, presentation, and events are transient.

Heat-conductor and heat-exchanger records contain entity ID and grid position.
Exchanger water and steam remain in the generic fluid-storage section.
Connection masks, heat ports, heat/fluid network IDs, latest conversion
quantities, and activity are reconstructed or transient.

Steam-turbine records contain entity ID, grid position, and stable definition
ID -- unchanged in shape by version 15. Steam and exhaust steam are both
carried by generic fluid storage, in two separate storage/port slots (input
and output) as of version 15, where version 14 had only the input slot.
Network IDs, availability, actual output, consumption, exhaust production,
completed cycles, and activity are transient.

Steam-condenser records contain entity ID, grid position, and stable
definition ID -- the same 16-byte shape as steam-turbine records. Steam and
water are carried by generic fluid storage (two slots: input and output).
Fluid/power network IDs, connection state, powered state, consumed steam,
produced water, completed cycles, and activity are transient and are
recomputed after load.

Version 15 has no compression, encryption, checksum, optional sections, or
migration decoder. A future incompatible change must introduce a deliberate
new-version decoder or compatibility policy.

## Version 20 rail sections

Version 20 adds canonical Rail and Rail Station sections. Rail records contain
entity ID, signed grid coordinates, and explicit geometry. Station records
contain entity ID, signed grid coordinates, and orientation. Derived ports,
neighbors, network IDs, station attachment, and topology events are excluded.
Load validates geometry, orientation, terrain, occupancy, and component
correspondence, then rebuilds topology without emitting events.

## Version 21 rail-switch section

Version 21 adds 20-byte Rail Switch records containing entity ID, signed grid
position, stable three-port geometry, and authoritative selected branch.
Physical neighbors, masks, component ID, and traversal results are excluded
and reconstructed. Load rejects invalid geometry/branch values, non-Ground
placement, occupancy conflicts, and entity/component mismatch.
