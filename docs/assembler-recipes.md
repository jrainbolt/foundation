# Assembler recipes

Assemblers use one of three immutable recipes from the authoritative table in
`src/assembler_recipe.c`.

| Identifier | Input slot 0 | Input slot 1 | Output | Ticks |
|---|---:|---:|---:|---:|
| `ELECTRONIC_COMPONENT` | 1 iron plate | 1 copper plate | 1 electronic component | 15 |
| `IRON_GEAR` | 2 iron plates | unused | 1 iron gear | 15 |
| `COPPER_WIRE` | 1 copper plate | unused | 2 copper wire | 15 |

New assemblers select `NONE`. They accept no input and do no work until a
`FACTORY_COMMAND_SET_ASSEMBLER_RECIPE` command is applied. Commands run FIFO
before logistics and processing. Placement followed by selection in one command
phase therefore succeeds; the reverse order fails for an invalid entity.

A selection, including clearing to `NONE` or reselecting the current recipe, is
allowed only while the assembler is completely empty and idle. That means both
input counts and the output count are zero, processing is false, and progress is
zero. A rejected change returns `FACTORY_RESULT_ASSEMBLER_NOT_EMPTY` without
mutating the assembler or construction inventory. A valid same-recipe selection
is a state-preserving success.

Each public `FactoryAssembler` inspection contains two bounded generic input
slots. A slot reports its required item, current count, and capacity. Unused
slots have item `NONE`, count zero, and capacity zero. The single counted output
buffer reports an item and count. Each successful logistics transfer removes
one output item, and the output item becomes `NONE` when the count reaches zero.
No new cycle begins while any output remains.

Belts and inserters choose the lowest-index compatible assembler input slot.
Transfer conflict identity remains `(entity_id, logical_slot)`, so no more than
one planned transfer can target a particular counted slot in one tick. Splitters,
belts, inserters, and storage otherwise transport gears and wire generically.

Recipe selection and processing do not consume construction units. Every
assembler still costs and refunds the same fixed 20 construction units.

Limitations remain deliberate: recipes cannot be created at runtime, queued,
unlocked, modified, or loaded from files; assemblers cannot run multiple recipes
simultaneously; and storage has no output behavior.
