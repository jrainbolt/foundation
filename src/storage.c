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
    storage->electronic_component_amount = 0U;
    storage->iron_gear_amount = 0U;
    storage->copper_wire_amount = 0U;
    storage->biomass_pellet_amount = 0U;
    storage->basic_science_amount = 0U;
    storage->total_capacity = FACTORY_STORAGE_CAPACITY;
    storage->configured_output_item = FACTORY_ITEM_NONE;
    storage->output_item = FACTORY_ITEM_NONE;
    storage->output_occupied = false;
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

bool factory_storage_store_remove(
    FactoryStorageStore *store,
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
    if (item == FACTORY_ITEM_ELECTRONIC_COMPONENT) {
        *out_amount = storage->electronic_component_amount;
        return true;
    }
    if (item == FACTORY_ITEM_IRON_GEAR) {
        *out_amount = storage->iron_gear_amount;
        return true;
    }
    if (item == FACTORY_ITEM_COPPER_WIRE) {
        *out_amount = storage->copper_wire_amount;
        return true;
    }
    if (item == FACTORY_ITEM_BIOMASS_PELLET) {
        *out_amount = storage->biomass_pellet_amount;
        return true;
    }
    if (item == FACTORY_ITEM_BASIC_SCIENCE) {
        *out_amount = storage->basic_science_amount;
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
            + storage->copper_plate_amount
            + storage->electronic_component_amount
            + storage->iron_gear_amount
            + storage->copper_wire_amount
            + storage->biomass_pellet_amount
            + storage->basic_science_amount;
}

static uint32_t *item_amount(
    FactoryStorage *storage,
    FactoryItemType item
)
{
    switch (item) {
        case FACTORY_ITEM_IRON_ORE:
            return &storage->iron_ore_amount;
        case FACTORY_ITEM_IRON_PLATE:
            return &storage->iron_plate_amount;
        case FACTORY_ITEM_COPPER_ORE:
            return &storage->copper_ore_amount;
        case FACTORY_ITEM_COPPER_PLATE:
            return &storage->copper_plate_amount;
        case FACTORY_ITEM_ELECTRONIC_COMPONENT:
            return &storage->electronic_component_amount;
        case FACTORY_ITEM_IRON_GEAR:
            return &storage->iron_gear_amount;
        case FACTORY_ITEM_COPPER_WIRE:
            return &storage->copper_wire_amount;
        case FACTORY_ITEM_BIOMASS_PELLET:
            return &storage->biomass_pellet_amount;
        case FACTORY_ITEM_BASIC_SCIENCE:
            return &storage->basic_science_amount;
        default:
            return NULL;
    }
}

void factory_storage_store_update(FactoryStorageStore *store)
{
    size_t index;

    if (store == NULL) {
        return;
    }
    for (index = 0U; index < store->count; ++index) {
        FactoryStorage *storage = &store->items[index];
        uint32_t *amount;

        if (storage->output_occupied
            || storage->configured_output_item == FACTORY_ITEM_NONE) {
            continue;
        }
        amount = item_amount(storage, storage->configured_output_item);
        if (amount == NULL || *amount == 0U) {
            continue;
        }
        --*amount;
        storage->output_item = storage->configured_output_item;
        storage->output_occupied = true;
    }
}
