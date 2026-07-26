#include "event_internal.h"

#include "simulation_internal.h"

#include <stdlib.h>

bool factory_event_batch_reserve(FactoryEventBatch *batch, size_t count)
{
    FactoryEvent *items;
    size_t capacity;
    if (count <= batch->capacity) return true;
    capacity = batch->capacity == 0U ? 16U : batch->capacity;
    while (capacity < count) {
        if (capacity > SIZE_MAX / 2U) {
            capacity = count;
            break;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*items)) return false;
    items = realloc(batch->items, capacity * sizeof(*items));
    if (items == NULL) return false;
    batch->items = items;
    batch->capacity = capacity;
    return true;
}

void factory_event_batch_destroy(FactoryEventBatch *batch)
{
    if (batch == NULL) return;
    free(batch->items);
    *batch = (FactoryEventBatch){0};
}

void factory_simulation_emit_event(
    FactorySimulation *simulation, FactoryEvent event
)
{
    FactoryEventBatch *batch;
    if (simulation == NULL) return;
    batch = &simulation->events;
    if (!batch->recording) return;
    /* Tick preflight guarantees capacity before authoritative mutation. */
    if (batch->count >= batch->capacity) abort();
    event.tick = simulation->tick;
    batch->items[batch->count++] = event;
}

size_t factory_simulation_get_event_count(
    const FactorySimulation *simulation
)
{
    return simulation == NULL ? 0U : simulation->events.count;
}

const FactoryEvent *factory_simulation_get_event(
    const FactorySimulation *simulation, size_t index
)
{
    if (simulation == NULL || index >= simulation->events.count) return NULL;
    return &simulation->events.items[index];
}

void factory_simulation_clear_events(FactorySimulation *simulation)
{
    if (simulation != NULL) simulation->events.count = 0U;
}
