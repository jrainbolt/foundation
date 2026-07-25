#ifndef FOUNDATION_SPLITTER_INTERNAL_H
#define FOUNDATION_SPLITTER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "foundation/splitter.h"

typedef struct {
    FactorySplitter *items;
    size_t count;
    size_t capacity;
} FactorySplitterStore;

void factory_splitter_store_destroy(FactorySplitterStore *store);
bool factory_splitter_store_reserve_one(FactorySplitterStore *store);
void factory_splitter_store_add(
    FactorySplitterStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y,
    FactoryDirection facing
);
const FactorySplitter *factory_splitter_store_find(
    const FactorySplitterStore *store,
    FactoryEntityId id
);
FactorySplitter *factory_splitter_store_find_mutable(
    FactorySplitterStore *store,
    FactoryEntityId id
);
bool factory_splitter_store_remove(
    FactorySplitterStore *store,
    FactoryEntityId id
);

#endif
