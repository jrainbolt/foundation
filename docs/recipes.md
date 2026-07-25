# Recipes

Recipes are immutable built-in records looked up by ID. `NONE` and unknown IDs
return `NULL`.

```text
IRON_PLATE:   1 iron ore   → 10 updates → 1 iron plate
COPPER_PLATE: 1 copper ore → 10 updates → 1 copper plate
```

There is no runtime registration. Multiple inputs, alternate recipes, and
scenario-defined recipes are deferred.

Assembler recipes use a separate immutable type with an explicit bounded input
count:

```text
1 iron plate + 1 copper plate → 15 updates → 1 electronic component
```
