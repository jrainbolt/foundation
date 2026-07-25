# Entity System

The intended relationship is:

```text
Entity ID
    ↓
Components
    ↓
Simulation systems
```

An entity is currently only a nonzero integer ID managed by an owning entity
manager. Future components will store data associated with those IDs, and
simulation systems will process matching component data in a defined order.
No component model is selected yet.

IDs are preferable to raw pointers because they do not expose memory layout or
allocation choices. They can be validated after destruction and can cross API
boundaries without granting direct mutation access. The current manager does
not reuse destroyed IDs, preventing a stale ID from accidentally referring to
a newly created entity.

The manager is explicit rather than global. This keeps ownership clear and
allows independent simulations and tests to coexist in one process.

The simulation now owns one manager. Extractor records reference its IDs, and
world tiles store an occupying ID or zero. Failed placement invalidates any
entity created during the unsuccessful transaction.
