# Advanced science and distributed production

Advanced science is immutable process-lifetime content composed from existing
simulation systems. Coal is a finite tile-owned resource: one unit removed by
a successful Extractor cycle creates one Coal item. Procedural Coal uses its
own stateless hash channel and is generated only among remote patches.

The production chain is integer-exact:

- Steel: 2 Iron Plate + 1 Coal -> 1 Steel in 20 ticks.
- Advanced Component: 1 Steel + 2 Copper Wire -> 1 component in 20 ticks.
- Advanced Science: 1 Advanced Component + 1 Electronic Component -> 1 science
  in 25 ticks.

Refineries now have two fixed typed input stacks. Existing one-input recipes
declare an empty secondary input. Inputs are committed atomically only when
every exact input is present and output is free.

Research Labs retain one bounded, typed science stack. A lab accepts Basic or
Advanced Science when empty and then only the same type until consumed.
Selecting another technology never converts or deletes that stack. Only a
powered eligible lab whose stack matches the selected technology can commit a
research unit.

Advanced Manufacturing depends on Fluid Handling and consumes three units,
each requiring two Advanced Science and three powered work ticks. Definitions
remain outside snapshots. Typed lab inventory, new storage counts, and the
second refinery input are authoritative, so snapshot version 30 serializes
them canonically.
