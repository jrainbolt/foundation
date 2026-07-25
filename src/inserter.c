#include "inserter_internal.h"

#include <stdint.h>
#include <stdlib.h>

void factory_inserter_store_destroy(FactoryInserterStore *store)
{
    if (store == NULL) {
        return;
    }
    free(store->items);
    store->items = NULL;
    store->count = 0U;
    store->capacity = 0U;
}

bool factory_inserter_store_reserve_one(FactoryInserterStore *store)
{
    FactoryInserter *items;
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

static void adjacent_coordinate(
    int32_t x,
    int32_t y,
    FactoryDirection direction,
    int32_t *out_x,
    int32_t *out_y
)
{
    *out_x = x;
    *out_y = y;
    if (direction == FACTORY_DIRECTION_NORTH) {
        --*out_y;
    } else if (direction == FACTORY_DIRECTION_EAST) {
        ++*out_x;
    } else if (direction == FACTORY_DIRECTION_SOUTH) {
        ++*out_y;
    } else {
        --*out_x;
    }
}

void factory_inserter_store_add(
    FactoryInserterStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection facing
)
{
    FactoryInserter *inserter = &store->items[store->count++];
    FactoryDirection pickup_direction =
        (FactoryDirection)(((int)facing + 2) % 4);

    inserter->entity_id = id;
    inserter->x = x;
    inserter->y = y;
    inserter->facing = facing;
    inserter->held_item = FACTORY_ITEM_NONE;
    inserter->held_amount = 0U;
    inserter->state = FACTORY_INSERTER_STATE_IDLE;
    inserter->progress = 0U;
    adjacent_coordinate(
        x, y, pickup_direction, &inserter->source_x, &inserter->source_y
    );
    adjacent_coordinate(
        x, y, facing, &inserter->destination_x, &inserter->destination_y
    );
}

const FactoryInserter *factory_inserter_store_find(
    const FactoryInserterStore *store,
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

FactoryInserter *factory_inserter_store_find_mutable(
    FactoryInserterStore *store,
    FactoryEntityId id
)
{
    return (FactoryInserter *)factory_inserter_store_find(store, id);
}

bool factory_inserter_store_remove(
    FactoryInserterStore *store,
    FactoryEntityId id
)
{
    size_t index;

    if (store == NULL) {
        return false;
    }
    for (index = 0U; index < store->count; ++index) {
        if (store->items[index].entity_id == id) {
            --store->count;
            store->items[index] = store->items[store->count];
            return true;
        }
    }
    return false;
}
