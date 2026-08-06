#include "assembler_recipe_internal.h"
#include "foundation/content.h"

#include <stddef.h>

const FactoryAssemblerRecipe *factory_assembler_recipe_find(
    FactoryAssemblerRecipeId recipe_id
)
{
    return factory_content_assembler_recipe_get(recipe_id);
}

bool factory_assembler_recipe_get(
    FactoryAssemblerRecipeId recipe_id,
    FactoryAssemblerRecipe *out_recipe
)
{
    const FactoryAssemblerRecipe *recipe =
        factory_assembler_recipe_find(recipe_id);

    if (recipe == NULL || out_recipe == NULL) {
        return false;
    }
    *out_recipe = *recipe;
    return true;
}
