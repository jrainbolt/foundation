#ifndef FOUNDATION_SPLITTER_H
#define FOUNDATION_SPLITTER_H

#include <stdint.h>

#include "foundation/command.h"
#include "foundation/item.h"

typedef enum {
    FACTORY_SPLITTER_OUTPUT_LEFT = 0,
    FACTORY_SPLITTER_OUTPUT_RIGHT
} FactorySplitterOutput;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryDirection facing;
    FactoryItemType item;
    FactorySplitterOutput next_output;
} FactorySplitter;

#endif
