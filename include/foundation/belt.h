#ifndef FOUNDATION_BELT_H
#define FOUNDATION_BELT_H

#include <stdint.h>

#include "foundation/extractor.h"

#define FACTORY_BELT_TRANSFER_TICKS 5U

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryDirection direction;
    FactoryItemType item;
    uint32_t movement_progress;
} FactoryBelt;

#endif
