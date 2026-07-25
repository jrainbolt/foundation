#ifndef FOUNDATION_ASSEMBLER_RECIPE_H
#define FOUNDATION_ASSEMBLER_RECIPE_H

#include <stdint.h>

#include "foundation/item.h"

#define FACTORY_ASSEMBLER_MAX_INPUT_TYPES 2U
#define FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS 15U

typedef enum {
    FACTORY_ASSEMBLER_RECIPE_NONE = 0,
    FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT
} FactoryAssemblerRecipeId;

typedef struct {
    FactoryItemType input_items[FACTORY_ASSEMBLER_MAX_INPUT_TYPES];
    uint32_t input_amounts[FACTORY_ASSEMBLER_MAX_INPUT_TYPES];
    uint32_t input_count;
    FactoryItemType output_item;
    uint32_t output_amount;
    uint32_t processing_ticks;
} FactoryAssemblerRecipe;

const FactoryAssemblerRecipe *factory_assembler_recipe_get(
    FactoryAssemblerRecipeId recipe_id
);

#endif
