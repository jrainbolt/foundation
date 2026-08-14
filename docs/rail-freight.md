# Deterministic rail freight stations

Rail Stations own a small authoritative freight buffer in addition to their
existing spatial attachment. A station is configured by FIFO commands with a
mode (`DISABLED`, `LOAD`, or `UNLOAD`) and one stable item ID. Its capacity is
200 items. Changing the configured item while the buffer is non-empty is
rejected without mutation.

Only a train whose route is `ARRIVED` at its configured destination station is
eligible. Stations are processed by ascending entity ID, and each station may
commit at most one transfer per tick. The eligible train is the lowest train
ID, and its Cargo Wagons are checked in consist order. A committed transfer is
the minimum of the 10-item quantum, source quantity, and destination free
space. Incompatible, empty, and full wagons are skipped. There is no partial
mutation when no transfer is possible.

Station buffers are ordinary logistics endpoints. Inserters may insert into a
station in `LOAD` mode and remove from one in `UNLOAD` mode, using the same
authoritative endpoint transaction used by other logistics owners. Train
movement itself never transfers cargo.

`FACTORY_EVENT_RAIL_FREIGHT_TRANSFERRED` records each committed station/wagon
ownership change. Its entity is the station, related entity is the wagon,
item and quantity identify the cargo, related quantity is the train ID, and
third quantity is the station mode. Presentation exports IDs, quantities,
eligibility, and last-tick activity without owning freight state.

Snapshot version 28 stores station mode, configured item, and quantity.
Eligibility, transfer possibility, and last-tick activity are derived or
transient and are rebuilt/cleared on load. No rail topology, route, or
reservation semantics changed in this milestone.
