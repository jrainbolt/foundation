#ifndef FOUNDATION_ASSEMBLER_RECIPE_H
#define FOUNDATION_ASSEMBLER_RECIPE_H

#include <stdbool.h>
#include <stdint.h>

#include "foundation/item.h"
#include "foundation/research.h"

#define FACTORY_ASSEMBLER_MAX_INPUT_TYPES 2U
#define FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS 15U

typedef enum {
    FACTORY_ASSEMBLER_RECIPE_NONE = 0,
    FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT,
    FACTORY_ASSEMBLER_RECIPE_IRON_GEAR,
    FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE,
    FACTORY_ASSEMBLER_RECIPE_COUNT
} FactoryAssemblerRecipeId;

typedef struct {
    FactoryAssemblerRecipeId recipe_id;
    FactoryItemType input_items[FACTORY_ASSEMBLER_MAX_INPUT_TYPES];
    uint32_t input_amounts[FACTORY_ASSEMBLER_MAX_INPUT_TYPES];
    uint32_t input_count;
    FactoryItemType output_item;
    uint32_t output_amount;
    uint32_t processing_ticks;
    FactoryUnlockFlags required_unlock;
} FactoryAssemblerRecipe;

bool factory_assembler_recipe_get(
    FactoryAssemblerRecipeId recipe_id,
    FactoryAssemblerRecipe *out_recipe
);

#endif
