#include "foundation/assembler_recipe.h"

#include <stddef.h>

const FactoryAssemblerRecipe *factory_assembler_recipe_get(
    FactoryAssemblerRecipeId recipe_id
)
{
    static const FactoryAssemblerRecipe electronic_component = {
        {FACTORY_ITEM_IRON_PLATE, FACTORY_ITEM_COPPER_PLATE},
        {1U, 1U},
        2U,
        FACTORY_ITEM_ELECTRONIC_COMPONENT,
        1U,
        FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS
    };

    return recipe_id == FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT
        ? &electronic_component : NULL;
}
