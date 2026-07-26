#ifndef FOUNDATION_EVENT_INTERNAL_H
#define FOUNDATION_EVENT_INTERNAL_H

#include "foundation/event.h"

typedef struct {
    FactoryEvent *items;
    size_t count;
    size_t capacity;
    bool recording;
} FactoryEventBatch;

bool factory_event_batch_reserve(FactoryEventBatch *batch, size_t count);
void factory_event_batch_destroy(FactoryEventBatch *batch);
void factory_simulation_emit_event(
    FactorySimulation *simulation, FactoryEvent event
);

#endif
