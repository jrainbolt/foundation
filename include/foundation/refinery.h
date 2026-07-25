#ifndef FOUNDATION_REFINERY_H
#define FOUNDATION_REFINERY_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/command.h"
#include "foundation/recipe.h"

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryDirection input_direction;
    FactoryDirection output_direction;
    FactoryRecipeId recipe_id;
    FactoryItemType input_item;
    uint32_t input_amount;
    FactoryItemType output_item;
    uint32_t output_amount;
    uint32_t processing_progress;
    bool processing;
} FactoryRefinery;

#endif
