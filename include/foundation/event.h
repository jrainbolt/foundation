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
    FACTORY_EVENT_SUNSET
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
    uint32_t quantity;
    uint32_t related_quantity;
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
