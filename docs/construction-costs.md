# Construction Costs

All placement and refund behavior uses
`factory_entity_construction_cost()` as its sole cost source.

| Entity | Construction units |
|---|---:|
| Extractor | 10 |
| Belt | 1 |
| Storage | 8 |
| Refinery | 15 |
| Assembler | 20 |
| Splitter | 5 |
| Inserter | 4 |

Invalid, unknown, placeholder, and non-placeable entity types have no cost.

Placement first verifies that the complete cost is available. Entity-specific
placement then performs its existing checks. The balance is debited only after
placement succeeds, so failure neither spends units nor allocates an ID solely
because of the cost check.

Demolition retains every existing empty-and-idle requirement. It resolves the
same cost and checks credit overflow before mutation. Only successful removal
receives the full refund; invalid, busy, material-containing, and already
removed entities receive nothing.

Costs are immutable, so entities do not store their paid value. Variable
prices, upgrades, damage, or partial refunds require a future policy.
