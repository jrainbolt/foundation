#ifndef FOUNDATION_REFINERY_INTERNAL_H
#define FOUNDATION_REFINERY_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "foundation/refinery.h"
#include "foundation/power.h"

typedef struct {
    FactoryRefinery *items;
    size_t count;
    size_t capacity;
} FactoryRefineryStore;

void factory_refinery_store_destroy(FactoryRefineryStore *store);
bool factory_refinery_store_reserve_one(FactoryRefineryStore *store);
void factory_refinery_store_add(
    FactoryRefineryStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection input_direction,
    FactoryDirection output_direction
);
const FactoryRefinery *factory_refinery_store_find(
    const FactoryRefineryStore *store,
    FactoryEntityId id
);
FactoryRefinery *factory_refinery_store_find_mutable(
    FactoryRefineryStore *store,
    FactoryEntityId id
);
bool factory_refinery_store_remove(
    FactoryRefineryStore *store,
    FactoryEntityId entity_id
);
void factory_refinery_store_update(
    FactoryRefineryStore *store,
    const FactorySimulation *simulation
);

#endif
