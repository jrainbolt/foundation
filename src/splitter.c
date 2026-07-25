#include "splitter_internal.h"

#include <stdint.h>
#include <stdlib.h>

void factory_splitter_store_destroy(FactorySplitterStore *store)
{
    if (store == NULL) {
        return;
    }
    free(store->items);
    store->items = NULL;
    store->count = 0U;
    store->capacity = 0U;
}

bool factory_splitter_store_reserve_one(FactorySplitterStore *store)
{
    FactorySplitter *items;
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

void factory_splitter_store_add(
    FactorySplitterStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection facing
)
{
    FactorySplitter *splitter = &store->items[store->count++];

    splitter->entity_id = id;
    splitter->x = x;
    splitter->y = y;
    splitter->facing = facing;
    splitter->item = FACTORY_ITEM_NONE;
    splitter->next_output = FACTORY_SPLITTER_OUTPUT_LEFT;
}

const FactorySplitter *factory_splitter_store_find(
    const FactorySplitterStore *store,
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

FactorySplitter *factory_splitter_store_find_mutable(
    FactorySplitterStore *store,
    FactoryEntityId id
)
{
    return (FactorySplitter *)factory_splitter_store_find(store, id);
}

bool factory_splitter_store_remove(
    FactorySplitterStore *store,
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
