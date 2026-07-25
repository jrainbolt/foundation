#include "foundation/recipe.h"

#include <stddef.h>

const FactoryRecipe *factory_recipe_get(FactoryRecipeId recipe_id)
{
    static const FactoryRecipe iron_plate = {
        FACTORY_ITEM_IRON_ORE,
        1U,
        FACTORY_ITEM_IRON_PLATE,
        1U,
        FACTORY_IRON_PLATE_PROCESSING_TICKS
    };
    static const FactoryRecipe copper_plate = {
        FACTORY_ITEM_COPPER_ORE,
        1U,
        FACTORY_ITEM_COPPER_PLATE,
        1U,
        FACTORY_IRON_PLATE_PROCESSING_TICKS
    };

    switch (recipe_id) {
        case FACTORY_RECIPE_IRON_PLATE:
            return &iron_plate;
        case FACTORY_RECIPE_COPPER_PLATE:
            return &copper_plate;
        case FACTORY_RECIPE_NONE:
        default:
            return NULL;
    }
}
