# Assembler recipes

Assemblers use one of three immutable recipes from the authoritative table in
the unified immutable content layer.

| Identifier | Input slot 0 | Input slot 1 | Output | Ticks |
|---|---:|---:|---:|---:|
| `ELECTRONIC_COMPONENT` | 1 iron plate | 1 copper plate | 1 electronic component | 15 |
| `IRON_GEAR` | 2 iron plates | unused | 1 iron gear | 15 |
| `COPPER_WIRE` | 1 copper plate | unused | 2 copper wire | 15 |

New assemblers select `NONE`. They accept no input and do no work until a
`FACTORY_COMMAND_SET_ASSEMBLER_RECIPE` command is applied. Commands run FIFO
before logistics and processing. Placement followed by selection in one command
phase therefore succeeds; the reverse order fails for an invalid entity.

Changing to a different recipe, including clearing to `NONE`, is allowed only
while the assembler is completely empty and idle. Both input counts and the
output count must be zero, processing must be false, and progress must be zero.
A rejected change returns `FACTORY_RESULT_ASSEMBLER_NOT_EMPTY` without mutation.
Reselecting the current recipe is a state-preserving success regardless of
buffer state and emits no event.

An actual committed change emits
`FACTORY_EVENT_ASSEMBLER_RECIPE_CHANGED` with the assembler ID and stable old
and new recipe IDs. The event is observational and transient. Recipe state and
pending configuration commands are canonical snapshot state; events are not.

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

Clients request changes through the public FIFO command API and inspect normal
command results and rebuilt presentation records. They never mutate an
assembler directly. Recipes cannot be created or modified at runtime and an
assembler cannot run multiple recipes simultaneously.
