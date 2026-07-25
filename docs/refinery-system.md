# Refinery System

A newly placed refinery has `FACTORY_RECIPE_NONE`, accepts no input, and
remains idle. After inspecting the placement result, a frontend submits
`FACTORY_COMMAND_SET_REFINERY_RECIPE` using the known entity ID on a later
tick. Commands do not reference future placement results.

Selection is idempotent for the current recipe. Switching to a different
recipe requires a completely idle refinery: no processing or progress, empty
input, and empty output. Invalid recipes, invalid or non-refinery entities, and
busy machines are rejected without changing recipe or material.

A ready belt must occupy the configured input side, point at the refinery, and
carry the selected recipe's input item. Iron recipes reject copper ore, copper
recipes reject iron ore, and recipe-less refineries reject all items.

Input and output each hold one item. Output is sent only to an empty belt in
the configured direction. Power, selectable recipe queues, upgrades, and
multiple slots remain out of scope.
