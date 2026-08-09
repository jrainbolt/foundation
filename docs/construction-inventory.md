# Construction Inventory

Each simulation begins with one `uint32_t` bootstrap balance of generic
construction units. The balance starts at zero when using
`factory_simulation_create()`. Tests or special scenarios may use
`factory_simulation_create_with_construction_units()` to select an explicit
initial balance.

Construction units are not production items. They cannot enter logistics or
resource deposits, and they do not contribute to iron or copper conservation.

Before the first Construction Depot commits,
`FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS` credits this reserve during FIFO
command processing. A zero grant is a successful no-op and overflow fails
without mutation. After bootstrap completes, grants fail with
`FACTORY_RESULT_INVALID_STATE`; construction material is then physical only.

Public code can inspect the balance with
`factory_simulation_construction_units()`. No mutable pointer is exposed.

```text
current units
= initial units
 + successful grants
 - successful placement costs
 + successful demolition refunds
```

Failed commands contribute zero. A grant or demolition refund queued before a
placement can fund that placement in the same tick. Reversing the order does
not retroactively fund the earlier command.

When the first depot commits, its cost is deducted and the complete remainder
must fit in that depot. The reserve becomes zero permanently. Snapshot v19
stores this lifecycle transition, so demolition cannot reactivate bootstrap.
