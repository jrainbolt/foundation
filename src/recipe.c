#include "foundation/recipe.h"
#include "foundation/content.h"

#include <stddef.h>

const FactoryRecipe *factory_recipe_get(FactoryRecipeId recipe_id)
{
    const FactoryRefineryRecipeDefinition *definition=
        factory_content_refinery_recipe_get(recipe_id);
    return definition==NULL?NULL:&definition->recipe;
}
