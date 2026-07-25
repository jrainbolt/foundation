# Storage System

Storage accepts iron ore, iron plate, copper ore, copper plate, and electronic
components from ready adjacent belts. It keeps separate counters and exposes safe
per-item queries, total usage, and capacity.

Capacity is shared across all item types:

```text
iron ore + iron plate + copper ore + copper plate + components <= 100
```

Invalid item queries fail without indexing an array. Full storage rejects
incoming items atomically, leaving the source belt occupied and ready. Storage
remains input-only and has no slots, filters, output, or demolition.
