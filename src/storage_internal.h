#ifndef FOUNDATION_STORAGE_INTERNAL_H
#define FOUNDATION_STORAGE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "foundation/storage.h"

typedef struct {
    FactoryStorage *items;
    size_t count;
    size_t capacity;
} FactoryStorageStore;

void factory_storage_store_destroy(FactoryStorageStore *store);
bool factory_storage_store_reserve_one(FactoryStorageStore *store);
void factory_storage_store_add(
    FactoryStorageStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y
);
const FactoryStorage *factory_storage_store_find(
    const FactoryStorageStore *store,
    FactoryEntityId id
);
FactoryStorage *factory_storage_store_find_mutable(
    FactoryStorageStore *store,
    FactoryEntityId id
);

#endif
