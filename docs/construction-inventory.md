# Construction Inventory

Each simulation begins with one `uint32_t` bootstrap balance of generic
construction units. The balance starts at zero when using
`factory_simulation_create()`. Tests or special scenarios may use
`factory_simulation_create_with_construction_units()` to select an explicit
initial balance.

Bootstrap construction units are not production items and cannot enter
logistics or resource deposits. After the first depot commits, the remaining
units become `FACTORY_ITEM_CONSTRUCTION_MATERIAL` owned by that depot. From
then on the material follows ordinary physical logistics and conservation.

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

Remote construction therefore follows one ownership path:

```text
bootstrap reserve -> first Construction Depot
Construction Depot output -> Inserter/Belt/Storage/Rail logistics
logistics -> remote Construction Depot input -> covered construction
```

Belts, Storage, Inserters, Rail Stations, and Cargo Wagons may own material in
transit, but only an eligible Construction Depot may pay a placement cost.
Construction commands execute before the tick's logistics updates; connected
export machinery can move only the balance left after successful commands.
