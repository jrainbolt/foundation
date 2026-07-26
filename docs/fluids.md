# Deterministic generic fluids

Foundation fluid quantities are unsigned integer units. The unit is
deliberately abstract: simulation code never converts it to volume, mass,
pressure, or a floating-point value.

`FactoryFluidDefinition` is immutable catalog data. The initial catalog
contains water in the aqueous class and steam in the vapor class.
Entity-owned `FactoryFluidStorage` state records its owner, stable storage
slot, position, accepted class mask, current fluid, quantity, and capacity. A
storage is either empty (`NONE`, zero) or contains exactly one fluid type.
Ordinary fluid tanks accept aqueous or vapor fluids and hold 10,000 units.

Insert, remove, and transfer are explicit FIFO simulation commands. Validation
precedes mutation: zero quantities, missing endpoints, insufficient source
quantity, incompatible classes, mixed fluids, and insufficient destination
capacity leave every storage unchanged. A successful transfer subtracts and
adds the same quantity atomically. Insert and remove deliberately model
external ingress and egress; transfer is conservation-neutral.

Successful operations emit exactly one observational event at their command
position. Failed operations emit none. Fluid state is included in canonical
snapshot version 12; pending fluid commands retain FIFO order. Loaded empty,
partial, and full tanks preserve their exact integer state, while transient
events and presentation copies remain excluded.

Renderer-neutral presentation exposes only fluid type, quantity, and capacity.
The Godot bridge copies those same integer fields.

Steam has no special subsystem or behavior. Pressure, pumps, temperature,
chemistry, and fluid mixing remain intentionally absent.
