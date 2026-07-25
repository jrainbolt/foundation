#ifndef FOUNDATION_INSERTER_H
#define FOUNDATION_INSERTER_H

#include <stdint.h>

#include "foundation/command.h"
#include "foundation/item.h"

#define FACTORY_INSERTER_ACTION_TICKS 1U

typedef enum {
    FACTORY_INSERTER_STATE_IDLE = 0,
    FACTORY_INSERTER_STATE_PICKING_UP,
    FACTORY_INSERTER_STATE_HOLDING,
    FACTORY_INSERTER_STATE_DROPPING
} FactoryInserterState;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryDirection facing;
    FactoryItemType held_item;
    uint32_t held_amount;
    FactoryInserterState state;
    uint32_t progress;
    int32_t source_x;
    int32_t source_y;
    int32_t destination_x;
    int32_t destination_y;
} FactoryInserter;

#endif
