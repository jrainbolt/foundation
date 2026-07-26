# Foundation

Foundation is a rendering-independent deterministic C17 simulation engine for
automation, logistics, colony-building, and RTS-style games.

Its current production chain includes:

```text
Iron deposit   → iron plate ┐
                            ├→ assembler → electronic component → storage
Copper deposit → copper plate┘
```

The fixed assembler recipe consumes one iron plate and one copper plate over
15 updates. Its two one-item input slots are independent logical transfer
destinations, so different plates can arrive simultaneously while same-item
conflicts resolve by lowest source entity ID.

All public headers are under `include/foundation/`; C symbols retain their
existing `Factory` and `factory_` prefixes.

## Build, test, and run

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/factory_console_demo
```

The demo shows inserters supplying refinery and assembler inputs, observable
pickup/holding/drop phases, component production, and final storage delivery.

Demolition supports all placed entity types but succeeds only when the target
owns no material and has no active work. IDs remain invalid forever after
removal; replacement entities receive newer IDs.

Splitters add passive one-input/two-output routing with a one-item buffer.
They alternate successful left/right transfers, fall back when the preferred
output is blocked, and preserve state when both outputs are blocked.

See `docs/splitter-system.md` for orientation, fairness, and blocking rules.

Inserters actively move one item between adjacent logistics endpoints using
separate pickup and drop commits. They retain held material under backpressure,
and same-endpoint contention is won by the lowest inserter entity ID.

See `docs/inserter-system.md` for timing, ownership, and interaction rules.

Internally, item movement uses entity-and-slot logistics endpoints so source
inspection, destination acceptance, and atomic ownership commits share one
implementation. This refactor does not change the public API or gameplay.

Placement spends simulation-owned construction units, while successful
empty-only demolition refunds the full fixed cost. Construction units remain
separate from iron, copper, and production logistics. See
`docs/construction-inventory.md` and `docs/construction-costs.md`.

Assemblers now default to no recipe and can select fixed electronic-component,
iron-gear, or copper-wire recipes through the FIFO command queue. Inputs and
outputs are bounded counted buffers, including the two-wire output. See
`docs/assembler-recipes.md` and `docs/conservation.md`.

Storage can export one configured item through a deterministic one-item buffer
for inserter pickup. Inventory remains private and backpressure stops further
extraction while the buffer is occupied. See `docs/storage-output.md`.

Versioned snapshots preserve the complete world, entity manager, subsystem
state, queued commands, command results, and future entity allocation in a
canonical portable binary format. See `docs/save-load.md` and
`docs/snapshot-format.md`.

Factorio-style power poles form automatic deterministic networks and allocate
whole-machine power by ascending entity ID. See `docs/power.md`.
