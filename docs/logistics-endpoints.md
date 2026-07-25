# Internal Logistics Endpoints

The logistics endpoint layer centralizes the item-slot operations shared by
belts, splitters, machines, inserters, extractors, and storage. It is private
to the simulation and does not add gameplay or public API.

## Identity and slots

An endpoint is an entity ID plus a logical slot. Slots distinguish a belt's
main item, machine input/output, assembler iron and copper inputs, splitter
input and left/right outputs, inserter-held item, and storage input.

Conflict resolution compares the complete endpoint. Two proposals for one
assembler iron slot conflict, while simultaneous proposals for its iron and
copper slots do not.

## Operations and results

The module provides source peek/removal, destination acceptance/insertion, and
atomic transfer. Internal results distinguish success, empty source, blocked
destination, invalid entity, invalid slot/item, incompatibility, and state
mismatch.

Peek and acceptance are read-only. Removal verifies the expected item.
Insertion enforces capacity, recipe compatibility, logical-slot compatibility,
and inserter state where applicable.

## Planning and atomic commit

Transfer planners resolve endpoints, inspect them without mutation, record
intents, and resolve conflicts. A winning commit revalidates both endpoints and
the expected item before changing ownership.

After complete validation, the endpoint module performs two direct,
non-failing writes in the single-threaded commit phase: source removal followed
by destination insertion. There are no callbacks, allocations, or externally
observable steps between them. A failed validation performs neither write.

## Responsibility boundaries

The endpoint layer owns generic slot contents and acceptance rules. Scheduling
and geometry remain in the simulation planner:

- Belt readiness and movement progress determine when a transfer is planned.
- Splitters choose preferred or alternate routes and update preference only
  after a successful endpoint transfer.
- Inserters retain their four-state timing and use their held endpoint only at
  pickup/drop commit boundaries.
- Refinery orientation and splitter rear/side geometry remain planner rules.
- Machine processing and recipe-selection timing remain machine rules.

The dependency direction is simulation planning → endpoint layer →
entity-specific internal stores. Entity stores do not depend on planners.

## Invariants and limitations

Every endpoint belongs to a live entity and uses a slot valid for that entity.
Planning never changes ownership. A successful commit moves exactly one
unchanged item; a failed commit changes nothing. Storage remains input-only.

Assembler endpoints are input 0, input 1, and output. Input acceptance consults
the selected immutable recipe and current count; unused slots and recipe
`NONE` are incompatible. Output removal decrements its count by exactly one.

This is a closed internal dispatch layer. It has no runtime registration,
callbacks, heap allocation per transfer, public polymorphism, or support for
new logistics entities beyond the current engine.
