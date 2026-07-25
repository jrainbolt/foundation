#ifndef FOUNDATION_ASSEMBLER_H
#define FOUNDATION_ASSEMBLER_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/assembler_recipe.h"
#include "foundation/command.h"

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryDirection output_direction;
    FactoryAssemblerRecipeId recipe_id;
    uint32_t iron_plate_amount;
    uint32_t copper_plate_amount;
    FactoryItemType output_item;
    uint32_t output_amount;
    uint32_t processing_progress;
    bool processing;
} FactoryAssembler;

#endif
