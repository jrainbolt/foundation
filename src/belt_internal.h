#ifndef FOUNDATION_BELT_INTERNAL_H
#define FOUNDATION_BELT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "foundation/belt.h"

typedef struct {
    FactoryBelt *items;
    size_t count;
    size_t capacity;
} FactoryBeltStore;

void factory_belt_store_destroy(FactoryBeltStore *store);
bool factory_belt_store_reserve_one(FactoryBeltStore *store);
void factory_belt_store_add(
    FactoryBeltStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection direction
);
const FactoryBelt *factory_belt_store_find(
    const FactoryBeltStore *store,
    FactoryEntityId id
);
FactoryBelt *factory_belt_store_find_mutable(
    FactoryBeltStore *store,
    FactoryEntityId id
);
bool factory_belt_store_remove(
    FactoryBeltStore *store,
    FactoryEntityId entity_id
);
void factory_belt_store_advance(FactoryBeltStore *store);

#endif
