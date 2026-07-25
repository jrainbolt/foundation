# Splitter System

A splitter is a passive one-item logistics entity with one facing direction.
Its input is opposite the facing direction. Its outputs are left and right
relative to facing. For an east-facing splitter, input is west, left is north,
and right is south.

Each item uses deterministic round-robin routing. When both output belts are
empty, `next_output` is selected. If the preferred output is unavailable, the
alternate is tried. If neither is available, the splitter retains the item and
does not change state. After a successful transfer, the next output becomes
the opposite of the side actually used.

Splitter output planning occurs before belt input transfer. An item entering a
splitter therefore cannot leave until a later tick. Contention with other
producers uses the existing lowest-source-entity-ID rule.

Demolition follows the empty-only policy. A buffered splitter cannot be
removed. Splitters have no priorities, filters, merging, overflow mode,
multiple inputs, or extra buffering.
