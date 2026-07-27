# Foundation Godot visualizer

This subtree is a read-only Godot 4.x frontend. The root Foundation CMake build
does not discover or build it. The native `FoundationSimulation` GDExtension
owns one world/simulation pair and one presentation snapshot. Godot receives
deep-copied dictionaries and arrays; no Foundation-owned pointer crosses the
boundary.

## Pinned prerequisites

- Godot 4.5 or a later compatible Godot 4.x release
- `godot-cpp` tag `godot-4.5-stable`, commit
  `e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77`
- SCons and an Apple Clang or GCC-compatible C/C++ toolchain

Keep `godot-cpp` in a separate checkout:

```sh
git clone --branch godot-4.5-stable --depth 1 \
  https://github.com/godotengine/godot-cpp.git ../godot-cpp-4.5
export GODOT_CPP_DIR="$(cd ../godot-cpp-4.5 && pwd)"
```

The exact required tag and commit are recorded in `GODOT_CPP_VERSION` and
`GODOT_CPP_COMMIT`. The build wrapper refuses an untagged or mismatched
checkout. These bindings officially target Godot 4.5; the adapter is also
runtime-tested against the forward-compatible Godot 4.7.1 GDExtension API.

## Build and run

From `godot/`, on macOS (including Apple Silicon):

```sh
./tools/build_extension.sh macos template_debug
godot --editor --path .
```

The default macOS build is universal. For a smaller Apple-Silicon-only build,
run SCons directly with the same checked dependency:

```sh
scons platform=macos target=template_debug arch=arm64
```

On Linux x86_64 or arm64:

```sh
./tools/build_extension.sh linux template_debug
godot --editor --path .
```

Use Reset Demo, Step, and Run/Pause in the scene. Run mode advances a fixed
number of integer simulation steps at a 12 Hz cadence; frame delta is never
passed into Foundation. The event panel retains only the latest 80 copied
event lines.

For a headless integration smoke test after building:

```sh
godot --headless --path . --script tests/smoke.gd
godot --headless --path . --script tests/main_scene_smoke.gd
```

The first test checks native-class loading, safe uninitialized access,
deterministic reset, exact tick stepping, repeated event reads, explicit event
clearing, presentation rebuilding, independent simultaneous adapters, and
destruction. The second loads the real main scene and checks visual counts,
run, pause, single-step, reset, scene reload, and shutdown. Test failures call
`quit(1)` and therefore produce a nonzero process exit.

The C++ public-header/ABI smoke test can run without Godot after the normal
Foundation build:

```sh
c++ -std=c++17 -I../include native/tests/cpp_abi_smoke.cpp \
  ../build/libfactory_engine.a -o /tmp/foundation_cpp_abi_smoke
/tmp/foundation_cpp_abi_smoke

c++ -std=c++17 native/tests/integer_conversion_test.cpp \
  -o /tmp/foundation_integer_conversion_test
/tmp/foundation_integer_conversion_test
```

## Adapter contract

`FoundationSimulation` is a `RefCounted` class with:

- `reset_demo()`, `step()`, and bounded `step_many(count)`
- `get_tick()`
- `get_entities()`, `get_resources()`, and `get_power_edges()`
- `get_events()` and `clear_events()`
- `place_fluid_tank()`, `insert_fluid()`, `remove_fluid()`, and
  `transfer_fluid()`

A successful `get_entities()` export contains exactly one dictionary for every
valid native presentation entity. Conversion failure sets the adapter error
and returns an empty array; it never returns a successful partial batch.
- `has_error()`, `get_last_error()`, and `clear_error()`
- `rebuild_presentation()`
- `result_name(result)`

Every mutating method returns a `FactoryResult` integer. The GDScript frontend
shows the corresponding stable diagnostic name. `step_many` accepts 0 through
10,000 steps and rejects other values.

Entity dictionaries contain `id`, `type`, `x`, `y`, `direction`, `status`, and
`powered`, plus type-specific integer/bool payloads. Resource dictionaries
contain `x`, `y`, `type`, `remaining`, and `occupying_entity_id`. Power edges
contain canonical endpoint IDs `a` and `b`. Event dictionaries copy every
public `FactoryEvent` field. Generator dictionaries expose
`maximum_output`, `allocated`, `fuel_item`, `fuel_ticks`, `fuel_active`, and
`energy_available`; the adapter copies these authoritative presentation fields
and does not derive burn state.

Godot Variant integers are signed 64-bit. One shared checked conversion accepts
Foundation unsigned values from zero through `INT64_MAX` exactly. Larger values
are rejected: the affected read returns `-1` for a scalar or an empty array for
a record batch, `has_error()` becomes true, and `get_last_error()` identifies
the offending field and value. The frontend checks this error before replacing
its previously synchronized visual state. Values never wrap, saturate, convert
to floating point, or become decimal-string substitutes in integer fields.

The current true 64-bit crossings are simulation/event ticks (`uint64_t`) and
power-source allocated network power (`FactoryPowerTotal`, an alias of
`uint64_t`). Entity IDs, related and occupying IDs, attached pole IDs, power
network IDs, edge endpoints, counts, and quantities are currently unsigned
32-bit values. Identity fields also use the checked helper so a future public
type widening cannot silently introduce collisions.

The demo deposits are authored through the public world resource API. Every
entity and recipe/output selection is submitted through the public FIFO command
API and checked before the presentation snapshot is exposed.

## Verification record

The correction pass used
`4.7.1.stable.official.a13da4feb` from
`/Applications/Godot.app/Contents/MacOS/Godot`. Successful commands were:

```sh
/Applications/Godot.app/Contents/MacOS/Godot --version
/Applications/Godot.app/Contents/MacOS/Godot \
  --headless --path . --script tests/smoke.gd
/Applications/Godot.app/Contents/MacOS/Godot \
  --headless --path . --script tests/main_scene_smoke.gd
/Applications/Godot.app/Contents/MacOS/Godot \
  --path . --script tests/capture_visual.gd
/Applications/Godot.app/Contents/MacOS/Godot --path . --quit-after 180
```

The 4.7.1 editor was launched with `--editor --path .`, initialized the
extension using the Metal/OpenGL compatibility renderer without registration,
load, or symbol errors, and exited with status zero. The captured graphical
main scene was inspected for the deterministic layout, all nine entity
categories, distinct iron/copper deposit markers, canonical power wires,
machine progress, tick/status panel, and populated event log. Automated
main-scene coverage verifies run, pause, single-step, reset, and visual
collection counts.

Platform status:

| Platform | Configured | Compiled | Runtime verified |
| --- | --- | --- | --- |
| macOS arm64 | Yes | Yes | Yes, Godot 4.7.1 |
| macOS universal | Yes | No in correction pass | No |
| Linux x86_64 | Yes | No | No |
| Linux arm64 | Yes | No | No |

### Turbine-exhaust re-verification

Re-run after introducing authoritative turbine exhaust fluid and the real
closed loop (Water -> Heat Exchanger -> Live Steam -> Steam Turbine ->
Exhaust Steam -> Steam Condenser -> Water), using the same
`4.7.1.stable.official.a13da4feb` at `/Applications/Godot.app`. `godot-cpp`
was a fresh checkout at the pinned tag/commit (no prior sibling checkout
existed in this environment); `scons` came from the `SCons` package already
present in the local Python user site. Successful commands:

```sh
export GODOT_CPP_DIR=/path/to/godot-cpp-4.5   # pinned godot-4.5-stable checkout
./tools/build_extension.sh macos template_debug
/Applications/Godot.app/Contents/MacOS/Godot \
  --headless --path . --script tests/smoke.gd
/Applications/Godot.app/Contents/MacOS/Godot \
  --headless --path . --script tests/main_scene_smoke.gd
/Applications/Godot.app/Contents/MacOS/Godot --path . --quit-after 180
```

The build, both headless smoke tests, and a non-editor game-mode launch
(`--quit-after 180`, no `--editor`) all completed cleanly: no registration,
load, symbol, or parse errors, zero exit status throughout. One real bug was
caught and fixed by this run that a native-only C harness could not have
caught: `tests/smoke.gd` used C-style `/* */` block comments, which GDScript
does not support -- `godot --headless --script` failed with a parse error
until they were changed to `#` line comments. The demo world now has 48
entities (was 49): the turbine's east tile is reserved for a real exhaust
pipe to the condenser, so the accumulator and solar generator moved to
(12, 1) and (12, 2), and the condenser moved from tapping the boiler's
live-steam network at (11, 5) to sitting directly on the turbine's exhaust
network at (11, 0). The full `--editor` launch and `tests/capture_visual.gd`
were not re-run in this pass; only the headless smoke tests and a non-
editor game-mode launch were.
