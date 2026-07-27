#ifndef FOUNDATION_EVENT_H
#define FOUNDATION_EVENT_H

#include <stddef.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/item.h"
#include "foundation/fluid.h"

typedef struct FactorySimulation FactorySimulation;

typedef enum {
    FACTORY_EVENT_ENTITY_CONSTRUCTED = 1,
    FACTORY_EVENT_ENTITY_DEMOLISHED,
    FACTORY_EVENT_PRODUCTION_COMPLETED,
    FACTORY_EVENT_ITEM_TRANSFERRED,
    FACTORY_EVENT_POWER_GAINED,
    FACTORY_EVENT_POWER_LOST,
    FACTORY_EVENT_FUEL_IGNITED,
    FACTORY_EVENT_FUEL_EXHAUSTED,
    FACTORY_EVENT_FLUID_INSERTED,
    FACTORY_EVENT_FLUID_REMOVED,
    FACTORY_EVENT_FLUID_TRANSFERRED,
    FACTORY_EVENT_FLUID_NETWORK_CREATED,
    FACTORY_EVENT_FLUID_NETWORK_SPLIT,
    FACTORY_EVENT_FLUID_NETWORK_MERGED,
    FACTORY_EVENT_PIPE_CONNECTED,
    FACTORY_EVENT_PIPE_DISCONNECTED,
    FACTORY_EVENT_WATER_PRODUCED,
    FACTORY_EVENT_BOILER_CONVERSION_COMPLETED,
    FACTORY_EVENT_STEAM_ENGINE_GENERATION_COMPLETED,
    FACTORY_EVENT_SUNRISE,
    FACTORY_EVENT_SUNSET,
    FACTORY_EVENT_ACCUMULATOR_CHARGED,
    FACTORY_EVENT_ACCUMULATOR_DISCHARGED,
    FACTORY_EVENT_REACTOR_FUELED,
    FACTORY_EVENT_REACTOR_HEAT_GENERATED,
    FACTORY_EVENT_REACTOR_FUEL_EXHAUSTED,
    FACTORY_EVENT_HEAT_NETWORK_CREATED,
    FACTORY_EVENT_HEAT_NETWORK_SPLIT,
    FACTORY_EVENT_HEAT_NETWORK_MERGED,
    FACTORY_EVENT_HEAT_PORT_CONNECTED,
    FACTORY_EVENT_HEAT_PORT_DISCONNECTED,
    FACTORY_EVENT_HEAT_TRANSFERRED,
    FACTORY_EVENT_HEAT_EXCHANGER_CYCLE_COMPLETED,
    FACTORY_EVENT_STEAM_TURBINE_CYCLE_COMPLETED,
    FACTORY_EVENT_STEAM_CONDENSER_CYCLE_COMPLETED
} FactoryEventType;

/*
 * All fields are initialized for every event. Fields not used by an event
 * type are zero/FACTORY_*_NONE.
 *
 * constructed/demolished: entity_id and entity_type identify the entity.
 * production: entity_id produced quantity units of item_type.
 * transfer: entity_id is the source and related_entity_id the destination.
 * power gained/lost: entity_id identifies the consumer.
 * fuel ignited/exhausted: entity_id owns the burner and item_type is fuel.
 * fluid inserted/removed: entity_id owns the affected storage, fluid_type and
 * quantity identify the committed fluid change.
 * fluid transferred: entity_id is the source, related_entity_id is the
 * destination, and fluid_type and quantity identify the committed transfer.
 * water produced: entity_id is the extractor and fluid_type/quantity are the
 * committed water output.
 * boiler conversion: entity_id is the boiler; fluid_type/quantity are the
 * consumed input and related_fluid_type/related_quantity are the output.
 * steam generation: entity_id is the engine, fluid_type/quantity identify
 * consumed steam, and related_quantity is generated electrical energy.
 * sunrise/sunset carry no payload and occur once at their clock boundary.
 * accumulator charged/discharged: entity_id identifies the accumulator,
 * quantity is transferred energy, and related_quantity is resulting storage.
 * reactor fueled/exhausted: entity_id identifies the reactor,
 * nuclear_fuel_id identifies the rod, and quantity is one.
 * reactor heat generated: entity_id identifies the reactor, quantity is
 * generated heat, and related_quantity is resulting stored heat.
 * heat transferred: entity_id is the reactor source, related_entity_id is the
 * exchanger destination, and quantity is heat.
 * exchanger cycle: quantity is heat consumed, related_quantity is water
 * consumed, and third_quantity is steam produced.
 * turbine cycle: entity_id identifies the turbine, nuclear_fuel_id carries
 * its stable definition ID, quantity is completed cycles, related_quantity
 * is consumed steam (equal to produced exhaust steam, a fixed 1:1 ratio),
 * fluid_type/related_fluid_type are the consumed and produced fluid types,
 * and third_quantity is generated electrical energy.
 * condenser cycle: entity_id is the condenser; fluid_type/quantity are the
 * consumed steam and related_fluid_type/related_quantity are the produced
 * water.
 *
 * tick is the simulation tick at the start of the step that emitted the
 * event. Item-transfer quantity is currently one.
 */
typedef struct {
    FactoryEventType type;
    uint64_t tick;
    FactoryEntityId entity_id;
    FactoryEntityId related_entity_id;
    FactoryEntityType entity_type;
    FactoryItemType item_type;
    FactoryFluidType fluid_type;
    FactoryFluidType related_fluid_type;
    uint32_t nuclear_fuel_id;
    uint32_t quantity;
    uint32_t related_quantity;
    uint32_t third_quantity;
} FactoryEvent;

/*
 * The read-only event batch belongs to simulation and remains valid until the
 * next tick, explicit clear, successful load destruction, or simulation
 * destruction. Returns NULL for a NULL simulation or out-of-range index.
 */
size_t factory_simulation_get_event_count(
    const FactorySimulation *simulation
);
const FactoryEvent *factory_simulation_get_event(
    const FactorySimulation *simulation, size_t index
);
void factory_simulation_clear_events(FactorySimulation *simulation);

#endif
