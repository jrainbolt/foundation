# Items

Foundation currently defines iron ore, iron plate, copper ore, and copper
plate. `factory_item_name` handles every supported value and safely reports
unknown values.

Belts remain item-agnostic. Storage uses explicit counters and validated
queries rather than treating unchecked enum values as array indexes.
