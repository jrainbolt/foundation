#include "assembler_recipe_internal.h"

#include <stddef.h>

static const FactoryAssemblerRecipe recipes[] = {
    {
        FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT,
        {FACTORY_ITEM_IRON_PLATE, FACTORY_ITEM_COPPER_PLATE},
        {1U, 1U}, 2U, FACTORY_ITEM_ELECTRONIC_COMPONENT, 1U,
        FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS
    },
    {
        FACTORY_ASSEMBLER_RECIPE_IRON_GEAR,
        {FACTORY_ITEM_IRON_PLATE, FACTORY_ITEM_NONE},
        {2U, 0U}, 1U, FACTORY_ITEM_IRON_GEAR, 1U,
        FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS
    },
    {
        FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE,
        {FACTORY_ITEM_COPPER_PLATE, FACTORY_ITEM_NONE},
        {1U, 0U}, 1U, FACTORY_ITEM_COPPER_WIRE, 2U,
        FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS
    }
};

const FactoryAssemblerRecipe *factory_assembler_recipe_find(
    FactoryAssemblerRecipeId recipe_id
)
{
    size_t index;

    for (index = 0U; index < sizeof(recipes) / sizeof(recipes[0]); ++index) {
        if (recipes[index].recipe_id == recipe_id) {
            return &recipes[index];
        }
    }
    return NULL;
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
