#include "belt_internal.h"

#include <stdint.h>
#include <stdlib.h>

void factory_belt_store_destroy(FactoryBeltStore *store)
{
    if (store == NULL) {
        return;
    }
    free(store->items);
    store->items = NULL;
    store->count = 0U;
    store->capacity = 0U;
}

bool factory_belt_store_reserve_one(FactoryBeltStore *store)
{
    FactoryBelt *items;
    size_t capacity;

    if (store == NULL) {
        return false;
    }
    if (store->count < store->capacity) {
        return true;
    }
    capacity = store->capacity == 0U ? 8U : store->capacity * 2U;
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

void factory_belt_store_add(
    FactoryBeltStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection direction
)
{
    FactoryBelt *belt = &store->items[store->count++];

    belt->entity_id = id;
    belt->x = x;
    belt->y = y;
    belt->direction = direction;
    belt->item = FACTORY_ITEM_NONE;
    belt->movement_progress = 0U;
}

const FactoryBelt *factory_belt_store_find(
    const FactoryBeltStore *store,
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

FactoryBelt *factory_belt_store_find_mutable(
    FactoryBeltStore *store,
    FactoryEntityId id
)
{
    return (FactoryBelt *)factory_belt_store_find(store, id);
}

void factory_belt_store_advance(FactoryBeltStore *store)
{
    size_t index;

    for (index = 0U; index < store->count; ++index) {
        FactoryBelt *belt = &store->items[index];

        if (belt->item != FACTORY_ITEM_NONE
            && belt->movement_progress < FACTORY_BELT_TRANSFER_TICKS) {
            ++belt->movement_progress;
        }
    }
}
