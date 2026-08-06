# Deterministic unlock enforcement

Construction and assembler recipe selection read unlock requirements only from
immutable content definitions. Validation checks the definition and unlock
before inventory or mutable entity state. A locked command returns
`FACTORY_RESULT_TECHNOLOGY_LOCKED` without mutation, inventory consumption, or
success events, and rejected FIFO commands are never replayed.

The deliberately small gated set is the steam condenser (Fluid Handling) and
the copper-wire assembler recipe (Basic Automation). Core logistics,
production, power, and fluid infrastructure remains available.

Research completion changes the existing research-controller flags, which are
observed by subsequent commands through renderer-neutral queries. Definitions
remain immutable and absent from snapshots and presentation records, so
snapshot version 16 and the presentation schema remain unchanged.
