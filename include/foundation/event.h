#ifndef FOUNDATION_EVENT_H
#define FOUNDATION_EVENT_H

#include <stddef.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/item.h"

typedef struct FactorySimulation FactorySimulation;

typedef enum {
    FACTORY_EVENT_ENTITY_CONSTRUCTED = 1,
    FACTORY_EVENT_ENTITY_DEMOLISHED,
    FACTORY_EVENT_PRODUCTION_COMPLETED,
    FACTORY_EVENT_ITEM_TRANSFERRED,
    FACTORY_EVENT_POWER_GAINED,
    FACTORY_EVENT_POWER_LOST,
    FACTORY_EVENT_FUEL_IGNITED,
    FACTORY_EVENT_FUEL_EXHAUSTED
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
 *
 * tick is the simulation tick at the start of the step that emitted the
 * event. quantity is currently one for every item transfer.
 */
typedef struct {
    FactoryEventType type;
    uint64_t tick;
    FactoryEntityId entity_id;
    FactoryEntityId related_entity_id;
    FactoryEntityType entity_type;
    FactoryItemType item_type;
    uint32_t quantity;
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
