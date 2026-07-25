#ifndef FOUNDATION_INSERTER_INTERNAL_H
#define FOUNDATION_INSERTER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "foundation/inserter.h"

typedef struct {
    FactoryInserter *items;
    size_t count;
    size_t capacity;
} FactoryInserterStore;

void factory_inserter_store_destroy(FactoryInserterStore *store);
bool factory_inserter_store_reserve_one(FactoryInserterStore *store);
void factory_inserter_store_add(
    FactoryInserterStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection facing
);
const FactoryInserter *factory_inserter_store_find(
    const FactoryInserterStore *store,
    FactoryEntityId id
);
FactoryInserter *factory_inserter_store_find_mutable(
    FactoryInserterStore *store,
    FactoryEntityId id
);
bool factory_inserter_store_remove(
    FactoryInserterStore *store,
    FactoryEntityId id
);

#endif
