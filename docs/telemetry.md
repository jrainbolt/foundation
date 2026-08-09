# Deterministic production and logistics telemetry

`FactoryTelemetry` is an externally owned observer. It retains no simulation
pointers and simulation code never reads it. After a successful tick, a client
calls `factory_telemetry_observe_step` while that tick's event batch is still
visible. Observation does not clear events or mutate presentation or world
state. Canonical snapshots therefore remain byte-identical with and without a
collector.

## Windows and timing

Configuration supplies short and long fixed tick windows plus a bounded entity
capacity. Defaults are 60 ticks, 600 ticks, and 256 stable entity IDs. Storage
is allocated when the collector is created: one global bucket and one compact
per-entity contribution per long-window tick. There is no unbounded history or
per-tick allocation in telemetry itself. The reusable presentation projection
used for activity classification may allocate during rebuild.

The first observation may attach after an already-running simulation and starts
empty history. Thereafter observations must be sequential. Re-observing the
same completed tick returns `FACTORY_TELEMETRY_RESULT_DUPLICATE` without
counting it; an earlier tick or gap returns `NON_SEQUENTIAL` without changing
history. Clients must clear telemetry after replacing/loading a simulation.

## Metric sources

Production and extraction come from committed
`FACTORY_EVENT_PRODUCTION_COMPLETED` events. Logistics received/sent quantities,
storage inflow/outflow, signed per-entity net flow, and Inserter pickup/drop
ownership boundaries come from
`FACTORY_EVENT_ITEM_TRANSFERRED`. Per-tick machine activity uses the existing
presentation status precedence. Belt occupancy is final state; a Belt holding
an item at completed transfer progress counts as blocked. Inserter holding is
reported directly. The current Inserter state cannot reliably distinguish all
idle-source and destination-wait cases, so telemetry does not guess them.

All quantities and rates are integer counters. Queries return totals and the
number of observed ticks; frontends may display a ratio. Additions saturate
rather than wrap and expose a `saturated` flag. Records remain associated with
monotonic entity IDs until `factory_telemetry_clear`, including after demolition.

Construction-material delivery uses ordinary item-transfer metrics, including
depot inflow. Telemetry is excluded from snapshot version 19. Persistent statistics, charts,
network heuristics, and telemetry-driven gameplay are deliberately excluded.
The layer exists so future remote-outpost and rail tests can compare exact
throughput using the same observational evidence.
