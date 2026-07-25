# Items

Foundation defines iron ore, iron plate, copper ore, copper plate, and the
electronic component. `factory_item_name` safely handles unknown values.

Belts remain item-agnostic. Storage uses explicit counters and validated
queries rather than treating unchecked enum values as array indexes.
