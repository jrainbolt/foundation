#ifndef FOUNDATION_EXTRACTOR_H
#define FOUNDATION_EXTRACTOR_H

#include <stdint.h>

#include "foundation/command.h"
#include "foundation/item.h"

#define FACTORY_EXTRACTOR_PRODUCTION_TICKS 20U

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryResourceType resource_type;
    FactoryItemType produced_item;
    FactoryDirection output_direction;
    uint32_t production_progress;
    FactoryItemType output_item;
    uint32_t output_amount;
} FactoryExtractor;

#endif
