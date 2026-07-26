#ifndef FOUNDATION_STORAGE_H
#define FOUNDATION_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/entity.h"
#include "foundation/item.h"

#define FACTORY_STORAGE_CAPACITY 100U
#define FACTORY_STORAGE_IRON_ORE_CAPACITY FACTORY_STORAGE_CAPACITY

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    uint32_t iron_ore_amount;
    uint32_t iron_plate_amount;
    uint32_t copper_ore_amount;
    uint32_t copper_plate_amount;
    uint32_t electronic_component_amount;
    uint32_t iron_gear_amount;
    uint32_t copper_wire_amount;
    uint32_t biomass_pellet_amount;
    uint32_t total_capacity;
    FactoryItemType configured_output_item;
    FactoryItemType output_item;
    bool output_occupied;
} FactoryStorage;

bool factory_storage_get_item_amount(
    const FactoryStorage *storage,
    FactoryItemType item,
    uint32_t *out_amount
);
uint32_t factory_storage_get_total_amount(const FactoryStorage *storage);

#endif
