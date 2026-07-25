#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand assembler(
    int32_t x,
    int32_t y,
    FactoryDirection direction
)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {x, y, direction}}
    };
    return command;
}

int main(void)
{
    FactoryAssemblerRecipe recipe;
    FactoryWorld *world = factory_world_create(3U, 2U);
    FactorySimulation *simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryCommand valid = assembler(1, 0, FACTORY_DIRECTION_EAST);
    FactoryCommand occupied = assembler(1, 0, FACTORY_DIRECTION_WEST);
    FactoryCommand outside = assembler(3, 0, FACTORY_DIRECTION_NORTH);
    FactoryCommand invalid = assembler(0, 0, (FactoryDirection)99);
    const FactoryCommandResult *result;
    FactoryAssembler state;
    FactoryEntityId id;
    FactoryStorage storage = {0};
    uint32_t amount = 99U;

    CHECK(factory_assembler_recipe_get(
        FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT, &recipe
    ));
    CHECK(recipe.input_count == 2U);
    CHECK(recipe.input_items[0] == FACTORY_ITEM_IRON_PLATE);
    CHECK(recipe.input_amounts[0] == 1U);
    CHECK(recipe.input_items[1] == FACTORY_ITEM_COPPER_PLATE);
    CHECK(recipe.input_amounts[1] == 1U);
    CHECK(recipe.output_item == FACTORY_ITEM_ELECTRONIC_COMPONENT);
    CHECK(recipe.output_amount == 1U);
    CHECK(recipe.processing_ticks
        == FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS);
    CHECK(!factory_assembler_recipe_get(
        FACTORY_ASSEMBLER_RECIPE_NONE, &recipe
    ));
    CHECK(!factory_assembler_recipe_get(
        (FactoryAssemblerRecipeId)99, &recipe
    ));
    CHECK(strcmp(factory_item_name(
        FACTORY_ITEM_ELECTRONIC_COMPONENT
    ), "electronic component") == 0);

    CHECK(factory_simulation_submit_command(simulation, &invalid)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_simulation_submit_command(simulation, &valid)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &occupied)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &outside)
        == FACTORY_RESULT_OK);
    factory_simulation_tick(simulation);
    result = factory_simulation_get_command_result(simulation, 0U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK);
    id = result == NULL ? 0U : result->entity_id;
    CHECK(factory_simulation_entity_is_valid(simulation, id));
    CHECK(factory_simulation_is_assembler(simulation, id));
    CHECK(factory_simulation_get_assembler(simulation, id, &state));
    CHECK(state.x == 1 && state.y == 0);
    CHECK(state.output_direction == FACTORY_DIRECTION_EAST);
    CHECK(state.recipe_id == FACTORY_ASSEMBLER_RECIPE_NONE);
    CHECK(state.input_slots[0].count == 0U);
    CHECK(state.input_slots[1].count == 0U);
    CHECK(state.output_item == FACTORY_ITEM_NONE);
    CHECK(state.output_amount == 0U);
    CHECK(!state.processing);
    CHECK(state.processing_progress == 0U);
    CHECK(factory_world_get_tile(world, 1, 0)->occupying_entity == id);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_TILE_OCCUPIED);
    CHECK(factory_simulation_get_command_result(simulation, 2U)->result
        == FACTORY_RESULT_OUT_OF_BOUNDS);
    CHECK(!factory_simulation_entity_is_valid(simulation, id + 1U));
    CHECK(!factory_simulation_get_assembler(simulation, id, NULL));

    storage.electronic_component_amount = 1U;
    CHECK(factory_storage_get_item_amount(
        &storage, FACTORY_ITEM_ELECTRONIC_COMPONENT, &amount
    ));
    CHECK(amount == 1U);
    CHECK(factory_storage_get_total_amount(&storage) == 1U);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);

    if (failures != 0) {
        return 1;
    }
    (void)printf("All assembler tests passed.\n");
    return 0;
}
