# Snapshot format version 1

Foundation snapshots are canonical little-endian binary files. Every value is
written field-by-field; no C structure, pointer, enum representation, padding,
timestamp, or process-specific value enters the format.

## Header

| Offset | Width | Field |
|---:|---:|---|
| 0 | 8 | Magic bytes `FOUNDATN` |
| 8 | 4 | Version (`1`) |
| 12 | 4 | Header size (`48`) |
| 16 | 8 | Total snapshot size |
| 24 | 8 | Payload size |
| 32 | 8 | Simulation tick |
| 40 | 4 | Section count (`12`) |
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

Version 1 requires each section exactly once in this order:

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
| 10 | Storage | 56 |
| 11 | Pending commands | 24 |
| 12 | Command results | 68 |

Unknown, reordered, duplicated, missing, incorrectly sized, or unsupported
sections are rejected. Exact full-buffer consumption is required.

Records retain subsystem array order because it is deterministic state. Entity
manager records preserve live-ID order and the next ID. World tiles are encoded
row-major. Commands use a type plus five explicit 32-bit payload fields; unused
fields are zero. Results contain that command encoding followed by result,
entity, position, construction, assembler-recipe, and storage-output fields.

Storage records include all seven counters, capacity, output configuration,
buffer item, and occupancy. Assembler records include recipe, both generic
counted slots, processing fields, and counted output. Inserter records include
the complete state-machine state and source/destination coordinates.

Version 1 has no compression, encryption, checksum, optional sections, or
migration decoder. A future incompatible change must introduce a deliberate
new-version decoder or compatibility policy.
