#include "accumulator_internal.h"

#include "simulation_internal.h"

#include <stdlib.h>

void factory_accumulator_store_destroy(FactoryAccumulatorStore *store)
{
    if (store == NULL) return;
    free(store->items);
    *store = (FactoryAccumulatorStore){0};
}

bool factory_accumulator_store_reserve_one(FactoryAccumulatorStore *store)
{
    FactoryAccumulator *items;
    size_t capacity;
    if (store == NULL) return false;
    if (store->count < store->capacity) return true;
    capacity = store->capacity == 0U ? 4U : store->capacity * 2U;
    if (capacity < store->capacity
        || capacity > SIZE_MAX / sizeof(*items)) return false;
    items = realloc(store->items, capacity * sizeof(*items));
    if (items == NULL) return false;
    store->items = items;
    store->capacity = capacity;
    return true;
}

void factory_accumulator_store_add(
    FactoryAccumulatorStore *store,
    FactoryEntityId id, int32_t x, int32_t y)
{
    store->items[store->count++] = (FactoryAccumulator){
        id, x, y, 0U, 0U, 0U};
}

const FactoryAccumulator *factory_accumulator_store_find(
    const FactoryAccumulatorStore *store, FactoryEntityId id)
{
    if (store == NULL || id == 0U) return NULL;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].entity_id == id) return &store->items[i];
    return NULL;
}

FactoryAccumulator *factory_accumulator_store_find_mutable(
    FactoryAccumulatorStore *store, FactoryEntityId id)
{
    return (FactoryAccumulator *)factory_accumulator_store_find(store, id);
}

bool factory_accumulator_store_remove(
    FactoryAccumulatorStore *store, FactoryEntityId id)
{
    if (store == NULL) return false;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].entity_id == id) {
            --store->count;
            store->items[i] = store->items[store->count];
            return true;
        }
    return false;
}

void factory_accumulator_begin_tick(FactorySimulation *simulation)
{
    if (simulation == NULL) return;
    for (size_t i = 0U; i < simulation->accumulators.count; ++i) {
        simulation->accumulators.items[i].charged_last_tick = 0U;
        simulation->accumulators.items[i].discharged_last_tick = 0U;
    }
}

FactoryResult factory_simulation_get_accumulator(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactoryAccumulatorInspection *out)
{
    const FactoryAccumulator *accumulator;
    FactoryEntityId attached = 0U;
    FactoryPowerNetworkId network = FACTORY_POWER_NETWORK_NONE;
    bool connected = false;
    if (simulation == NULL || id == 0U || out == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    accumulator = factory_accumulator_store_find(
        &simulation->accumulators, id);
    if (accumulator == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    for (size_t i = 0U; i < simulation->power.accumulator_count; ++i)
        if (simulation->power.accumulators[i].entity_id == id) {
            attached = simulation->power.accumulators[i].attached_pole_id;
            network = simulation->power.accumulators[i].network_id;
            connected = simulation->power.accumulators[i].connected;
            break;
        }
    *out = (FactoryAccumulatorInspection){
        id, accumulator->stored_energy, FACTORY_ACCUMULATOR_CAPACITY,
        FACTORY_ACCUMULATOR_MAX_CHARGE_RATE,
        FACTORY_ACCUMULATOR_MAX_DISCHARGE_RATE,
        accumulator->charged_last_tick,
        accumulator->discharged_last_tick,
        accumulator->charged_last_tick != 0U
            ? FACTORY_ACCUMULATOR_CHARGING
            : accumulator->discharged_last_tick != 0U
                ? FACTORY_ACCUMULATOR_DISCHARGING
                : FACTORY_ACCUMULATOR_IDLE,
        attached, network, connected};
    return FACTORY_RESULT_OK;
}
