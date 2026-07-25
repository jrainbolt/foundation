# Construction Inventory

Each simulation owns one `uint32_t` balance of generic construction units.
The balance starts at zero when using `factory_simulation_create()`. Tests or
special scenarios may use `factory_simulation_create_with_construction_units()`
to select an explicit initial balance.

Construction units are not production items. They cannot enter logistics or
resource deposits, and they do not contribute to iron or copper conservation.

`FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS` credits units during FIFO command
processing. A zero grant is a successful no-op. Grants that would exceed
`UINT32_MAX` fail without changing the balance.

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

The first model has no slots, stacks, multiple owners, crafting, logistics
delivery, dropped materials, or player inventory.
