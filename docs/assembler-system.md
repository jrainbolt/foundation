# Assembler system

An assembler occupies one tile, retains its output orientation, and starts with
recipe `NONE`. Recipe selection is a separate FIFO command and is safe only
when the machine has no inputs, in-process work, progress, or output.

Two generic input slots expose item, count, and capacity derived from the
selected immutable recipe. Electronic components use separate iron and copper
slots; gears use a capacity-two iron slot; wire uses one copper slot. Required
inputs transfer into processing atomically. Processing gains progress 1 on its
start tick and completes after the recipe's fixed 15 updates.

The single output buffer is counted. Electronic components and gears produce
one item; wire produces two. Logistics removes one item per transfer, and no
new cycle begins until the entire buffer is empty.

See `assembler-recipes.md` for the recipe table, switching rules, logistics
selection, conservation, and current limitations.
