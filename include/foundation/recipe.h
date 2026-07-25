#ifndef FOUNDATION_RECIPE_H
#define FOUNDATION_RECIPE_H

#include <stdint.h>

#include "foundation/item.h"

#define FACTORY_IRON_PLATE_PROCESSING_TICKS 10U

typedef enum {
    FACTORY_RECIPE_NONE = 0,
    FACTORY_RECIPE_IRON_PLATE,
    FACTORY_RECIPE_COPPER_PLATE
} FactoryRecipeId;

typedef struct {
    FactoryItemType input_item;
    uint32_t input_amount;
    FactoryItemType output_item;
    uint32_t output_amount;
    uint32_t processing_ticks;
} FactoryRecipe;

/* Returns immutable static recipe data, or NULL for an invalid ID. */
const FactoryRecipe *factory_recipe_get(FactoryRecipeId recipe_id);

#endif
