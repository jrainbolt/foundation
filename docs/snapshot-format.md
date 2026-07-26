# Snapshot format version 6

Foundation snapshots are canonical little-endian binary files. Every value is
written field-by-field; no C structure, pointer, enum representation, padding,
timestamp, or process-specific value enters the format.

## Header

| Offset | Width | Field |
|---:|---:|---|
| 0 | 8 | Magic bytes `FOUNDATN` |
| 8 | 4 | Version (`6`) |
| 12 | 4 | Header size (`48`) |
| 16 | 8 | Total snapshot size |
| 24 | 8 | Payload size |
| 32 | 8 | Simulation tick |
| 40 | 4 | Section count (`16`) |
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

Version 6 requires each section exactly once in this order:

| Type | Section | Record width |
|---:|---|---:|
| 1 | Metadata | one 16-byte payload |
| 2 | Entity manager | 4 bytes per live ID plus 8-byte prefix |
| 3 | World tiles | 16 bytes plus 8-byte dimensions |
| 4 | Extractors | 36 |
| 5 | Belts | 24 |
| 6 | Splitters | 24 |
| 7 | Refineries | 48 |
| 8 | Assemblers | 64 |
| 9 | Inserters | 48 |
| 10 | Storage | 60 |
| 11 | Power poles | 12 |
| 12 | Basic generators and burners | 44 |
| 13 | Fluid storages | 28 |
| 14 | Pipes | 12 |
| 15 | Pending commands | 24 |
| 16 | Command results | 68 |

Unknown, reordered, duplicated, missing, incorrectly sized, or unsupported
sections are rejected. Exact full-buffer consumption is required.

Records retain subsystem array order because it is deterministic state. Entity
manager records preserve live-ID order and the next ID. World tiles are encoded
row-major. Commands use a type plus five explicit 32-bit payload fields; unused
fields are zero. Results contain that command encoding followed by result,
entity, position, construction, assembler-recipe, and storage-output fields.

Storage records include all eight counters, capacity, output configuration,
buffer item, and occupancy. Assembler records include recipe, both generic
counted slots, processing fields, and counted output. Inserter records include
the complete state-machine state and source/destination coordinates.

Generator records include the burner fuel-class mask, input inventory, current
fuel, remaining burn ticks, and released available energy. The active fuel
definition and remaining ticks derive the exact unreleased energy and next
release position. Power edges, network membership,
attachments, and allocation are not serialized;
they are rebuilt from pole and generator positions after loading.

Fluid-storage records contain owner, grid position, accepted class mask, fluid
type, quantity, and capacity. Empty storage is canonically `NONE` with zero
quantity; non-empty storage must reference a valid compatible definition.

Pipe records contain authoritative entity ID and grid position. Connection
masks, ports, networks, and network IDs are deterministically reconstructed.

Version 6 has no compression, encryption, checksum, optional sections, or
migration decoder. A future incompatible change must introduce a deliberate
new-version decoder or compatibility policy.
