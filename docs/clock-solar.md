# Deterministic clock and solar generation

`FactorySimulationClock` is the simulation-owned authoritative time source.
It stores the completed tick count and its canonical integer derivations:

```text
ticks per day = 2400
day = tick / 2400
time of day = tick % 2400
```

No operating-system time, floating point, or independent subsystem clock is
used. A successful tick processes systems at the clock value visible at the
start of the step and advances the clock during finalization. Failed ticks do
not advance time. Tick overflow is rejected before commands, events, or world
state are changed.

Solar intensity uses a scale of 1000:

- ticks 0–599: night, intensity 0;
- ticks 600–899: linear sunrise ramp from 0 toward 1000;
- ticks 900–1499: full daylight, intensity 1000;
- ticks 1500–1799: linear sunset ramp toward 0;
- ticks 1800–2399: night, intensity 0.

The curve is symmetric and integer-only. A solar generator's maximum output is
100 power units per tick. Available output is
`100 * intensity / 1000`, with truncating integer division. Actual output is
demand-aware and recorded only when the existing power dispatcher allocates
it. There is no fuel, fluid, battery, accumulator, or hidden energy buffer.

Solar generators own a stable entity ID, grid position, transient
latest-tick output, and the existing generic power-generator component.
Generator discovery, pole attachment, network membership, source priority, and
consumer allocation therefore use the same deterministic path as burner and
steam generation.

The tick phase order is command application, clock-boundary events, fluid
topology and transport, burner release, fluid-machine production, generator
latest-tick reset, power topology/allocation and demand-aware source
consumption, burner finalization, item production/logistics, event
finalization, then clock advancement. Sunrise and sunset are emitted once when
a step begins at ticks 600 and 1800 of each day.

Snapshot version 9 stores the clock tick and solar identity/position. Day,
time-of-day, intensity, network state, available output, actual latest-tick
output, presentation, and events are derived or transient. Loading reconstructs
the derived clock fields and power topology before publication.

This milestone intentionally excludes batteries, accumulators, weather,
seasons, clouds, wind, astronomy, lighting, temperature, heat, and variable
day length.
