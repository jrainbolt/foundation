#include "foundation/item.h"
#include "foundation/recipe.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

int main(void)
{
    const FactoryRecipe *recipe = factory_recipe_get(
        FACTORY_RECIPE_IRON_PLATE
    );

    CHECK(recipe != NULL);
    CHECK(recipe->input_item == FACTORY_ITEM_IRON_ORE);
    CHECK(recipe->input_amount == 1U);
    CHECK(recipe->output_item == FACTORY_ITEM_IRON_PLATE);
    CHECK(recipe->output_amount == 1U);
    CHECK(recipe->processing_ticks == FACTORY_IRON_PLATE_PROCESSING_TICKS);
    CHECK(factory_recipe_get((FactoryRecipeId)99) == NULL);
    CHECK(strcmp(factory_item_name(FACTORY_ITEM_NONE), "none") == 0);
    CHECK(strcmp(factory_item_name(FACTORY_ITEM_IRON_ORE), "iron ore") == 0);
    CHECK(strcmp(factory_item_name(FACTORY_ITEM_IRON_PLATE), "iron plate") == 0);
    CHECK(strcmp(factory_item_name(FACTORY_ITEM_COPPER_ORE), "copper ore") == 0);
    CHECK(strcmp(factory_item_name(FACTORY_ITEM_COPPER_PLATE), "copper plate") == 0);
    CHECK(factory_recipe_get(FACTORY_RECIPE_NONE) == NULL);
    recipe = factory_recipe_get(FACTORY_RECIPE_COPPER_PLATE);
    CHECK(recipe != NULL);
    CHECK(recipe->input_item == FACTORY_ITEM_COPPER_ORE);
    CHECK(recipe->input_amount == 1U);
    CHECK(recipe->output_item == FACTORY_ITEM_COPPER_PLATE);
    CHECK(recipe->output_amount == 1U);
    CHECK(recipe->processing_ticks == FACTORY_IRON_PLATE_PROCESSING_TICKS);
    CHECK(strcmp(factory_item_name((FactoryItemType)99), "invalid item") == 0);

    if (failures != 0) {
        return 1;
    }
    (void)printf("All recipe tests passed.\n");
    return 0;
}
