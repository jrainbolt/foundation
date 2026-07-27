# Deterministic generic fluids

Foundation fluid quantities are unsigned integer units. The unit is
deliberately abstract: simulation code never converts it to volume, mass,
pressure, or a floating-point value.

`FactoryFluidDefinition` is immutable catalog data. The catalog contains
water in the aqueous class, and live steam (`FACTORY_FLUID_STEAM`) and
turbine exhaust steam (`FACTORY_FLUID_EXHAUST_STEAM`) both in the vapor
class. Entity-owned `FactoryFluidStorage` state records its owner, stable
storage slot, position, accepted class mask, current fluid, quantity, and
capacity. A storage is either empty (`NONE`, zero) or contains exactly one
fluid type. Ordinary fluid tanks accept aqueous or vapor fluids and hold
10,000 units.

Live steam and exhaust steam sharing a class but not an identity is a
deliberate test of the class/type distinction: `accepted_fluid_classes` is a
coarse bitmask that only gates what a storage or port may ever hold (a
class-only check, evaluated once, at the boundary), while `fluid_type` is
the exact identity a non-empty storage is locked to (checked on every insert
and transfer). Two different fluid types can share a class and still never
mix or substitute for each other in a given storage slot -- no
fluid-pair-specific code is required for this to hold; it falls out of the
same generic mismatch check every fluid pair has always used.

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

Steam and exhaust steam have no special subsystem or behavior of their own
in this module -- they are ordinary catalog entries, and the machines that
produce or consume them (boilers, heat exchangers, steam turbines, steam
condensers) own all of the behavior. Pressure, pumps, temperature,
chemistry, and fluid mixing remain intentionally absent.
