# Processing

Belt transfer commits recipe-matching ore to the refinery input before the
processing phase. If the refinery has a selected recipe, is idle, and output is empty, processing
starts in that same tick: the input slot is consumed into one explicitly
accounted in-process iron unit, and progress advances to 1.

Progress advances once per refinery update. The tenth update creates exactly
one iron plate, resets progress to zero, and marks the refinery idle. Producer
output transfer planning occurs earlier in the tick, so a newly completed
plate remains owned by the refinery until at least the next tick.

Both iron and copper use their selected recipe definitions rather than
hard-coded machine behavior. Material ownership is always exactly one of
deposit, extractor output, belt, refinery input, refinery in-process state,
refinery output, or storage.

Assembler input transfer also precedes processing. Both required slots may be
filled in one transfer phase, after which processing starts at progress 1/15.
New assembler output cannot move until the next tick.
