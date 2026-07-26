#include "reactor_internal.h"

#include "event_internal.h"
#include "simulation_internal.h"

#include <stdlib.h>

static const FactoryNuclearFuelDefinition nuclear_fuels[] = {
    {
        FACTORY_NUCLEAR_FUEL_BASIC_ROD,
        UINT64_C(10000),
        100U,
        FACTORY_REACTOR_MAX_HEAT_OUTPUT_PER_TICK
    }
};

static uint32_t remaining_ticks(
    const FactoryNuclearFuelDefinition *definition,
    FactoryHeatQuantity remaining)
{
    FactoryHeatQuantity ticks;
    if (definition == NULL || remaining == 0U) return 0U;
    ticks = remaining / definition->maximum_heat_output_per_tick
        + (remaining % definition->maximum_heat_output_per_tick != 0U);
    return ticks > UINT32_MAX ? UINT32_MAX : (uint32_t)ticks;
}

size_t factory_nuclear_fuel_definition_count(void)
{
    return sizeof(nuclear_fuels) / sizeof(nuclear_fuels[0]);
}

const FactoryNuclearFuelDefinition *factory_nuclear_fuel_definition_at(
    size_t index)
{
    return index < factory_nuclear_fuel_definition_count()
        ? &nuclear_fuels[index] : NULL;
}

const FactoryNuclearFuelDefinition *factory_nuclear_fuel_definition_get(
    FactoryNuclearFuelId fuel_id)
{
    for (size_t i = 0U; i < factory_nuclear_fuel_definition_count(); ++i)
        if (nuclear_fuels[i].fuel_id == fuel_id)
            return &nuclear_fuels[i];
    return NULL;
}

void factory_reactor_store_destroy(FactoryReactorStore *store)
{
    if (store == NULL) return;
    free(store->items);
    *store = (FactoryReactorStore){0};
}

bool factory_reactor_store_reserve_one(FactoryReactorStore *store)
{
    FactoryReactor *items;
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

void factory_reactor_store_add(
    FactoryReactorStore *store, FactoryEntityId id, int32_t x, int32_t y)
{
    store->items[store->count++] = (FactoryReactor){
        .entity_id = id,
        .x = x,
        .y = y,
        .heat_storage = {0U, FACTORY_REACTOR_HEAT_CAPACITY}
    };
}

const FactoryReactor *factory_reactor_store_find(
    const FactoryReactorStore *store, FactoryEntityId id)
{
    if (store == NULL || id == 0U) return NULL;
    for (size_t i = 0U; i < store->count; ++i)
        if (store->items[i].entity_id == id) return &store->items[i];
    return NULL;
}

FactoryReactor *factory_reactor_store_find_mutable(
    FactoryReactorStore *store, FactoryEntityId id)
{
    return (FactoryReactor *)factory_reactor_store_find(store, id);
}

bool factory_reactor_store_remove(
    FactoryReactorStore *store, FactoryEntityId id)
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

FactoryResult factory_reactor_insert_fuel(
    FactoryReactor *reactor, FactoryNuclearFuelId fuel_id)
{
    if (reactor == NULL || factory_nuclear_fuel_definition_get(fuel_id) == NULL)
        return FACTORY_RESULT_FUEL_INCOMPATIBLE;
    if (reactor->inventory_quantity >= FACTORY_REACTOR_FUEL_INVENTORY_CAPACITY)
        return FACTORY_RESULT_FUEL_INVENTORY_FULL;
    if (reactor->inventory_quantity != 0U
        && reactor->inventory_fuel_id != fuel_id)
        return FACTORY_RESULT_FUEL_INCOMPATIBLE;
    reactor->inventory_fuel_id = fuel_id;
    ++reactor->inventory_quantity;
    return FACTORY_RESULT_OK;
}

void factory_reactor_store_update(
    FactoryReactorStore *store, FactorySimulation *simulation)
{
    for (size_t i = 0U; i < store->count; ++i) {
        FactoryReactor *reactor = &store->items[i];
        const FactoryNuclearFuelDefinition *definition;
        FactoryHeatQuantity output;
        FactoryHeatQuantity capacity;
        reactor->generated_last_tick = 0U;
        reactor->activity = FACTORY_REACTOR_IDLE;
        if (reactor->active_fuel_id == FACTORY_NUCLEAR_FUEL_NONE
            && reactor->inventory_quantity != 0U) {
            definition = factory_nuclear_fuel_definition_get(
                reactor->inventory_fuel_id);
            if (definition == NULL) continue;
            reactor->active_fuel_id = definition->fuel_id;
            reactor->remaining_heat_yield = definition->total_heat_yield;
            --reactor->inventory_quantity;
            if (reactor->inventory_quantity == 0U)
                reactor->inventory_fuel_id = FACTORY_NUCLEAR_FUEL_NONE;
        }
        definition = factory_nuclear_fuel_definition_get(
            reactor->active_fuel_id);
        if (definition == NULL) continue;
        capacity =
            factory_heat_storage_remaining_capacity(&reactor->heat_storage);
        if (capacity == 0U) {
            reactor->activity = FACTORY_REACTOR_BLOCKED_HEAT_FULL;
            continue;
        }
        output = definition->maximum_heat_output_per_tick;
        if (reactor->remaining_heat_yield < output)
            output = reactor->remaining_heat_yield;
        if (capacity < output) output = capacity;
        if (output == 0U
            || !factory_heat_storage_add(&reactor->heat_storage, output))
            continue;
        reactor->remaining_heat_yield -= output;
        reactor->generated_last_tick = output;
        reactor->activity = FACTORY_REACTOR_GENERATING;
        factory_simulation_emit_event(simulation, (FactoryEvent){
            .type = FACTORY_EVENT_REACTOR_HEAT_GENERATED,
            .entity_id = reactor->entity_id,
            .quantity = (uint32_t)output,
            .related_quantity =
                (uint32_t)reactor->heat_storage.stored_heat
        });
        if (reactor->remaining_heat_yield == 0U) {
            factory_simulation_emit_event(simulation, (FactoryEvent){
                .type = FACTORY_EVENT_REACTOR_FUEL_EXHAUSTED,
                .entity_id = reactor->entity_id,
                .nuclear_fuel_id = reactor->active_fuel_id,
                .quantity = 1U
            });
            reactor->active_fuel_id = FACTORY_NUCLEAR_FUEL_NONE;
        }
    }
}

FactoryResult factory_simulation_get_reactor(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactoryReactorInspection *out)
{
    const FactoryReactor *reactor;
    const FactoryNuclearFuelDefinition *definition;
    if (simulation == NULL || id == 0U || out == NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    reactor = factory_reactor_store_find(&simulation->reactors, id);
    if (reactor == NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    definition =
        factory_nuclear_fuel_definition_get(reactor->active_fuel_id);
    *out = (FactoryReactorInspection){
        .entity_id = id,
        .stored_heat = reactor->heat_storage.stored_heat,
        .heat_capacity = reactor->heat_storage.capacity,
        .inventory_fuel_id = reactor->inventory_fuel_id,
        .inventory_quantity = reactor->inventory_quantity,
        .active_fuel_id = reactor->active_fuel_id,
        .remaining_burn_ticks =
            remaining_ticks(definition, reactor->remaining_heat_yield),
        .remaining_heat_yield = reactor->remaining_heat_yield,
        .generated_last_tick = reactor->generated_last_tick,
        .activity = reactor->activity
    };
    return FACTORY_RESULT_OK;
}
