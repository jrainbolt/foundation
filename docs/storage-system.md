# Storage System

Storage accepts iron ore, iron plate, copper ore, copper plate, electronic
components, iron gears, and copper wire. It keeps separate counters and exposes
safe per-item queries, total usage, and capacity.

Capacity is shared across all item types:

```text
iron ore + iron plate + copper ore + copper plate + components + gears + wire
<= 100
```

Invalid item queries fail without indexing an array. Full storage rejects
incoming items atomically, leaving the source belt occupied and ready.

Storage may configure one exported item and moves at most one matching inventory
item into its output buffer. Inserters can pick up only from this buffer.
See `storage-output.md` for timing, backpressure, and switching rules.
