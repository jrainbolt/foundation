#include "refinery_internal.h"
#include "power_internal.h"
#include "event_internal.h"

#include <stdint.h>
#include <stdlib.h>

void factory_refinery_store_destroy(FactoryRefineryStore *store)
{
    if (store == NULL) {
        return;
    }
    free(store->items);
    store->items = NULL;
    store->count = 0U;
    store->capacity = 0U;
}

bool factory_refinery_store_reserve_one(FactoryRefineryStore *store)
{
    FactoryRefinery *items;
    size_t capacity;

    if (store == NULL) {
        return false;
    }
    if (store->count < store->capacity) {
        return true;
    }
    capacity = store->capacity == 0U ? 4U : store->capacity * 2U;
    if (capacity < store->capacity
        || capacity > SIZE_MAX / sizeof(*store->items)) {
        return false;
    }
    items = realloc(store->items, capacity * sizeof(*store->items));
    if (items == NULL) {
        return false;
    }
    store->items = items;
    store->capacity = capacity;
    return true;
}

void factory_refinery_store_add(
    FactoryRefineryStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection input_direction,
    FactoryDirection output_direction
)
{
    FactoryRefinery *refinery = &store->items[store->count++];

    refinery->entity_id = id;
    refinery->x = x;
    refinery->y = y;
    refinery->input_direction = input_direction;
    refinery->output_direction = output_direction;
    refinery->recipe_id = FACTORY_RECIPE_NONE;
    refinery->input_item = FACTORY_ITEM_NONE;
    refinery->input_amount = 0U;
    refinery->output_item = FACTORY_ITEM_NONE;
    refinery->output_amount = 0U;
    refinery->processing_progress = 0U;
    refinery->processing = false;
}

const FactoryRefinery *factory_refinery_store_find(
    const FactoryRefineryStore *store,
    FactoryEntityId id
)
{
    size_t index;

    if (store == NULL || id == 0U) {
        return NULL;
    }
    for (index = 0U; index < store->count; ++index) {
        if (store->items[index].entity_id == id) {
            return &store->items[index];
        }
    }
    return NULL;
}

FactoryRefinery *factory_refinery_store_find_mutable(
    FactoryRefineryStore *store,
    FactoryEntityId id
)
{
    return (FactoryRefinery *)factory_refinery_store_find(store, id);
}

bool factory_refinery_store_remove(
    FactoryRefineryStore *store,
    FactoryEntityId entity_id
)
{
    size_t index;

    if (store == NULL) {
        return false;
    }
    for (index = 0U; index < store->count; ++index) {
        if (store->items[index].entity_id == entity_id) {
            --store->count;
            store->items[index] = store->items[store->count];
            return true;
        }
    }
    return false;
}

void factory_refinery_store_update(
    FactoryRefineryStore *store,
    FactorySimulation *simulation
)
{
    size_t index;

    for (index = 0U; index < store->count; ++index) {
        FactoryRefinery *refinery = &store->items[index];
        const FactoryRecipe *recipe = factory_recipe_get(refinery->recipe_id);

        if (!factory_power_is_entity_powered(
                simulation, refinery->entity_id)) {
            continue;
        }
        if (recipe != NULL
            && !refinery->processing
            && refinery->output_item == FACTORY_ITEM_NONE
            && refinery->input_item == recipe->input_item
            && refinery->input_amount == recipe->input_amount) {
            refinery->input_item = FACTORY_ITEM_NONE;
            refinery->input_amount = 0U;
            refinery->processing = true;
            refinery->processing_progress = 0U;
        }
        if (recipe == NULL || !refinery->processing) {
            continue;
        }
        ++refinery->processing_progress;
        if (refinery->processing_progress == recipe->processing_ticks) {
            refinery->output_item = recipe->output_item;
            refinery->output_amount = recipe->output_amount;
            refinery->processing = false;
            refinery->processing_progress = 0U;
            factory_simulation_emit_event(simulation, (FactoryEvent){
                .type = FACTORY_EVENT_PRODUCTION_COMPLETED,
                .entity_id = refinery->entity_id,
                .item_type = recipe->output_item,
                .quantity = recipe->output_amount
            });
        }
    }
}
