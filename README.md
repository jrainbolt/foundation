# Foundation

Foundation is a rendering-independent deterministic C17 simulation engine for
automation, logistics, colony-building, and RTS-style games.

It currently supports parallel iron and copper processing:

```text
Iron deposit   → iron ore   → refinery → iron plate   → storage
Copper deposit → copper ore → refinery → copper plate → storage
```

Refineries begin recipe-less. A frontend places a refinery, inspects its entity
ID, then queues a deterministic recipe-selection command on a later tick.
Recipe changes are rejected while any input, processing, progress, or output
state exists.

Storage shares 100 total capacity across all four item types. Transfers remain
item-agnostic, while extractors map resource types and refineries validate
their selected immutable recipes.

## Build, test, and run

From this directory:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/factory_console_demo
```

The demo runs iron and copper pipelines together, prints separate elemental
accounting, and demonstrates a safely rejected busy-refinery recipe switch.

See `docs/resources.md`, `docs/items.md`, `docs/recipes.md`, and
`docs/refinery-system.md` for the public model and exact rules.
