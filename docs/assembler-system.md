# Assembler System

An assembler occupies one tile and has the fixed electronic-component recipe.
It stores one iron plate, one copper plate, and one output item. Any cardinally
adjacent belt may provide input when it points into the assembler and carries
a required item whose matching slot is empty.

The iron and copper slots are distinct logical transfer destinations. Two
different plate types may arrive in one tick. Competing transfers for the same
slot resolve by lowest source entity ID; the loser remains on its ready belt.

With both plates present and output empty, processing consumes both and gains
progress 1 in that tick. After 15 updates it produces one electronic component.
Newly completed output waits until the following tick. Full output preserves
buffered inputs and prevents another recipe from starting.

Assemblers have no recipe selection, power, upgrades, extra slots, or direct
storage transfer.
