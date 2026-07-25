# Storage System

Storage accepts iron ore, iron plate, copper ore, and copper plate from ready
adjacent belts. It keeps separate counters for each type and exposes safe
per-item queries, total usage, and capacity.

Capacity is shared across all item types:

```text
iron ore + iron plate + copper ore + copper plate <= 100
```

Invalid item queries fail without indexing an array. Full storage rejects
incoming items atomically, leaving the source belt occupied and ready. Storage
remains input-only and has no slots, filters, output, or demolition.
