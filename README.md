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

The demo shows partial input buffering, 15-tick processing, deferred output
transfer, component storage, separate iron/copper conservation, safely rejected
busy demolition, and same-tick tile reuse.

Demolition supports all placed entity types but succeeds only when the target
owns no material and has no active work. IDs remain invalid forever after
removal; replacement entities receive newer IDs.

Splitters add passive one-input/two-output routing with a one-item buffer.
They alternate successful left/right transfers, fall back when the preferred
output is blocked, and preserve state when both outputs are blocked.

See `docs/splitter-system.md` for orientation, fairness, and blocking rules.
