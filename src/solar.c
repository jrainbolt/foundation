#include "solar_internal.h"

#include "simulation_internal.h"

#include <stdlib.h>

void factory_solar_generator_store_destroy(FactorySolarGeneratorStore *store)
{
    if (store == NULL) return;
    free(store->items);
    *store = (FactorySolarGeneratorStore){0};
}

bool factory_solar_generator_store_reserve_one(FactorySolarGeneratorStore *store)
{
    FactorySolarGenerator *items;
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

void factory_solar_generator_store_add(
    FactorySolarGeneratorStore *store, FactoryEntityId id, int32_t x, int32_t y)
{
    store->items[store->count++] = (FactorySolarGenerator){id, x, y, 0U};
}

const FactorySolarGenerator *factory_solar_generator_store_find(
    const FactorySolarGeneratorStore *store, FactoryEntityId id)
{
    if (store == NULL || id == 0U) return NULL;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].entity_id == id) return &store->items[i];
    return NULL;
}

bool factory_solar_generator_store_remove(
    FactorySolarGeneratorStore *store, FactoryEntityId id)
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

void factory_solar_generator_begin_tick(FactorySimulation *simulation)
{
    if (simulation == NULL) return;
    for (size_t i = 0U; i < simulation->solar_generators.count; ++i)
        simulation->solar_generators.items[i].generated_last_tick = 0U;
}

FactoryPowerUnits factory_solar_generator_available(
    const FactorySimulation *simulation, FactoryEntityId id)
{
    if (simulation == NULL
        || factory_solar_generator_store_find(
            &simulation->solar_generators, id) == NULL) return 0U;
    return FACTORY_SOLAR_GENERATOR_MAX_OUTPUT
        * factory_solar_intensity(simulation->clock.time_of_day)
        / FACTORY_SOLAR_INTENSITY_SCALE;
}

bool factory_solar_generator_record_generation(
    FactorySimulation *simulation, FactoryEntityId id,
    FactoryPowerUnits generated)
{
    FactorySolarGenerator *solar;
    if (simulation == NULL || generated == 0U
        || generated > factory_solar_generator_available(simulation, id))
        return false;
    solar = (FactorySolarGenerator *)factory_solar_generator_store_find(
        &simulation->solar_generators, id);
    if (solar == NULL) return false;
    solar->generated_last_tick += generated;
    return true;
}

FactoryResult factory_simulation_get_solar_generator(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactorySolarGeneratorInspection *out)
{
    const FactorySolarGenerator *solar;
    if (simulation == NULL || id == 0U || out == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    solar = factory_solar_generator_store_find(
        &simulation->solar_generators, id);
    if (solar == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    *out = (FactorySolarGeneratorInspection){
        id, FACTORY_SOLAR_GENERATOR_MAX_OUTPUT,
        factory_solar_generator_available(simulation, id),
        solar->generated_last_tick, solar->generated_last_tick != 0U};
    return FACTORY_RESULT_OK;
}
