#ifndef FOUNDATION_EXTRACTOR_INTERNAL_H
#define FOUNDATION_EXTRACTOR_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "factory/world.h"
#include "foundation/extractor.h"

typedef struct {
    FactoryExtractor *items;
    size_t count;
    size_t capacity;
} FactoryExtractorStore;

void factory_extractor_store_destroy(FactoryExtractorStore *store);
bool factory_extractor_store_reserve_one(FactoryExtractorStore *store);
void factory_extractor_store_add(
    FactoryExtractorStore *store,
    FactoryEntityId entity_id,
    int32_t x,
    int32_t y,
    FactoryResourceType resource_type,
    FactoryItemType produced_item,
    FactoryDirection direction
);
const FactoryExtractor *factory_extractor_store_find(
    const FactoryExtractorStore *store,
    FactoryEntityId entity_id
);
FactoryExtractor *factory_extractor_store_find_mutable(
    FactoryExtractorStore *store,
    FactoryEntityId entity_id
);
void factory_extractor_store_update(
    FactoryExtractorStore *store,
    FactoryWorld *world
);

#endif
