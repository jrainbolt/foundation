#include "assembler_internal.h"

#include <stdint.h>
#include <stdlib.h>

void factory_assembler_store_destroy(FactoryAssemblerStore *store)
{
    if (store == NULL) {
        return;
    }
    free(store->items);
    store->items = NULL;
    store->count = 0U;
    store->capacity = 0U;
}

bool factory_assembler_store_reserve_one(FactoryAssemblerStore *store)
{
    FactoryAssembler *items;
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

void factory_assembler_store_add(
    FactoryAssemblerStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection output_direction
)
{
    FactoryAssembler *assembler = &store->items[store->count++];

    assembler->entity_id = id;
    assembler->x = x;
    assembler->y = y;
    assembler->output_direction = output_direction;
    assembler->recipe_id =
        FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT;
    assembler->iron_plate_amount = 0U;
    assembler->copper_plate_amount = 0U;
    assembler->output_item = FACTORY_ITEM_NONE;
    assembler->output_amount = 0U;
    assembler->processing_progress = 0U;
    assembler->processing = false;
}

const FactoryAssembler *factory_assembler_store_find(
    const FactoryAssemblerStore *store,
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

FactoryAssembler *factory_assembler_store_find_mutable(
    FactoryAssemblerStore *store,
    FactoryEntityId id
)
{
    return (FactoryAssembler *)factory_assembler_store_find(store, id);
}

bool factory_assembler_store_remove(
    FactoryAssemblerStore *store,
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

void factory_assembler_store_update(FactoryAssemblerStore *store)
{
    size_t index;

    for (index = 0U; index < store->count; ++index) {
        FactoryAssembler *assembler = &store->items[index];
        const FactoryAssemblerRecipe *recipe =
            factory_assembler_recipe_get(assembler->recipe_id);

        if (!assembler->processing
            && assembler->output_item == FACTORY_ITEM_NONE
            && assembler->iron_plate_amount == recipe->input_amounts[0]
            && assembler->copper_plate_amount == recipe->input_amounts[1]) {
            assembler->iron_plate_amount = 0U;
            assembler->copper_plate_amount = 0U;
            assembler->processing = true;
            assembler->processing_progress = 0U;
        }
        if (!assembler->processing) {
            continue;
        }
        ++assembler->processing_progress;
        if (assembler->processing_progress == recipe->processing_ticks) {
            assembler->processing = false;
            assembler->processing_progress = 0U;
            assembler->output_item = recipe->output_item;
            assembler->output_amount = recipe->output_amount;
        }
    }
}
