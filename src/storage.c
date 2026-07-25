#include "storage_internal.h"

#include <stdint.h>
#include <stdlib.h>

void factory_storage_store_destroy(FactoryStorageStore *store)
{
    if (store == NULL) {
        return;
    }
    free(store->items);
    store->items = NULL;
    store->count = 0U;
    store->capacity = 0U;
}

bool factory_storage_store_reserve_one(FactoryStorageStore *store)
{
    FactoryStorage *items;
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

void factory_storage_store_add(
    FactoryStorageStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y
)
{
    FactoryStorage *storage = &store->items[store->count++];

    storage->entity_id = id;
    storage->x = x;
    storage->y = y;
    storage->iron_ore_amount = 0U;
    storage->iron_plate_amount = 0U;
    storage->copper_ore_amount = 0U;
    storage->copper_plate_amount = 0U;
    storage->total_capacity = FACTORY_STORAGE_CAPACITY;
}

const FactoryStorage *factory_storage_store_find(
    const FactoryStorageStore *store,
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

FactoryStorage *factory_storage_store_find_mutable(
    FactoryStorageStore *store,
    FactoryEntityId id
)
{
    return (FactoryStorage *)factory_storage_store_find(store, id);
}

bool factory_storage_get_item_amount(
    const FactoryStorage *storage,
    FactoryItemType item,
    uint32_t *out_amount
)
{
    if (storage == NULL || out_amount == NULL) {
        return false;
    }
    if (item == FACTORY_ITEM_IRON_ORE) {
        *out_amount = storage->iron_ore_amount;
        return true;
    }
    if (item == FACTORY_ITEM_IRON_PLATE) {
        *out_amount = storage->iron_plate_amount;
        return true;
    }
    if (item == FACTORY_ITEM_COPPER_ORE) {
        *out_amount = storage->copper_ore_amount;
        return true;
    }
    if (item == FACTORY_ITEM_COPPER_PLATE) {
        *out_amount = storage->copper_plate_amount;
        return true;
    }
    return false;
}

uint32_t factory_storage_get_total_amount(const FactoryStorage *storage)
{
    return storage == NULL
        ? 0U
        : storage->iron_ore_amount
            + storage->iron_plate_amount
            + storage->copper_ore_amount
            + storage->copper_plate_amount;
}
