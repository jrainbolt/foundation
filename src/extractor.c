#include "extractor_internal.h"

#include <stdint.h>
#include <stdlib.h>

#include "world_internal.h"

void factory_extractor_store_destroy(FactoryExtractorStore *store)
{
    if (store == NULL) {
        return;
    }
    free(store->items);
    store->items = NULL;
    store->count = 0U;
    store->capacity = 0U;
}

bool factory_extractor_store_reserve_one(FactoryExtractorStore *store)
{
    FactoryExtractor *resized = NULL;
    size_t capacity = 0U;

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
    resized = realloc(store->items, capacity * sizeof(*store->items));
    if (resized == NULL) {
        return false;
    }
    store->items = resized;
    store->capacity = capacity;
    return true;
}

void factory_extractor_store_add(
    FactoryExtractorStore *store,
    FactoryEntityId entity_id,
    int32_t x,
    int32_t y,
    FactoryResourceType resource_type,
    FactoryItemType produced_item,
    FactoryDirection direction
)
{
    FactoryExtractor *extractor = &store->items[store->count];

    extractor->entity_id = entity_id;
    extractor->x = x;
    extractor->y = y;
    extractor->resource_type = resource_type;
    extractor->produced_item = produced_item;
    extractor->output_direction = direction;
    extractor->production_progress = 0U;
    extractor->output_item = FACTORY_ITEM_NONE;
    extractor->output_amount = 0U;
    ++store->count;
}

const FactoryExtractor *factory_extractor_store_find(
    const FactoryExtractorStore *store,
    FactoryEntityId entity_id
)
{
    size_t index = 0U;

    if (store == NULL || entity_id == 0U) {
        return NULL;
    }
    for (index = 0U; index < store->count; ++index) {
        if (store->items[index].entity_id == entity_id) {
            return &store->items[index];
        }
    }
    return NULL;
}

FactoryExtractor *factory_extractor_store_find_mutable(
    FactoryExtractorStore *store,
    FactoryEntityId entity_id
)
{
    return (FactoryExtractor *)factory_extractor_store_find(store, entity_id);
}

void factory_extractor_store_update(
    FactoryExtractorStore *store,
    FactoryWorld *world
)
{
    size_t index = 0U;

    for (index = 0U; index < store->count; ++index) {
        FactoryExtractor *extractor = &store->items[index];
        const FactoryTile *tile = factory_world_get_tile(
            world, extractor->x, extractor->y
        );

        if (extractor->output_amount != 0U
            || tile == NULL
            || tile->resource != extractor->resource_type
            || tile->resource_amount == 0U) {
            continue;
        }
        ++extractor->production_progress;
        if (extractor->production_progress
            == FACTORY_EXTRACTOR_PRODUCTION_TICKS) {
            if (factory_world_consume_resource(
                    world, extractor->x, extractor->y, 1U
                ) == FACTORY_RESULT_OK) {
                extractor->output_item = extractor->produced_item;
                extractor->output_amount = 1U;
                extractor->production_progress = 0U;
            }
        }
    }
}
