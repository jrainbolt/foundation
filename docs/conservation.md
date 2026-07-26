# Material conservation

Elemental accounting uses integer half-units (`FACTORY_ELEMENT_UNIT_SCALE = 2`)
and never floating point.

| Item | Scaled iron units | Scaled copper units |
|---|---:|---:|
| Iron ore / iron plate | 2 | 0 |
| Copper ore / copper plate | 0 | 2 |
| Electronic component | 2 | 2 |
| Iron gear | 4 | 0 |
| Copper wire | 0 | 1 |

The public `factory_item_iron_units` and `factory_item_copper_units` helpers are
the authoritative item-content mapping used by conservation checks. Thus one
copper plate and two copper wires both contain two scaled copper units.

Accounting must include deposits, machine inputs, in-process material, counted
machine outputs, belts, splitter buffers, inserter-held items, and storage.
Recipe tests derive input and output totals from the immutable recipe definitions
and these item helpers, preventing a second recipe-accounting table.

Storage inventory and its output buffer are separate ownership locations. When
storage fills the buffer it decrements exactly one inventory count, preserving
the scaled total before any later inserter pickup.

Snapshot loading validates every count and canonical ownership state before the
simulation is returned. The format does not store separate authoritative
conservation totals; totals remain derived from the restored world and item
ownership using the existing integer-scaled helpers.

Power generation, demand, allocation, and network membership do not represent
item ownership and therefore do not enter material-conservation totals. Placing
or demolishing a pole or generator affects construction inventory only through
the same atomic cost and refund rules used by every other placeable entity.
