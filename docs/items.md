# Items

Foundation defines iron ore, iron plate, copper ore, copper plate, electronic
components, iron gears, and copper wire. `factory_item_name` safely handles
unknown values.

Belts remain item-agnostic. Storage uses explicit counters and validated
queries rather than treating unchecked enum values as array indexes.

Elemental helpers use half-unit integer scaling: ore and plates contain two
scaled units, a gear contains four iron units, and each wire contains one copper
unit. See `conservation.md`.
