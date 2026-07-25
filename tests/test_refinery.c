#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand refinery(
    int32_t x,
    int32_t y,
    FactoryDirection input,
    FactoryDirection output
)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {x, y, input, output}}
    };
    return command;
}

int main(void)
{
    FactoryWorld *world = factory_world_create(3U, 2U);
    FactorySimulation *simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryCommand valid = refinery(
        1, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
    );
    FactoryCommand occupied = refinery(
        1, 0, FACTORY_DIRECTION_NORTH, FACTORY_DIRECTION_SOUTH
    );
    FactoryCommand outside = refinery(
        3, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
    );
    FactoryCommand same = refinery(
        0, 0, FACTORY_DIRECTION_NORTH, FACTORY_DIRECTION_NORTH
    );
    FactoryCommand bad_input = refinery(
        0, 0, (FactoryDirection)99, FACTORY_DIRECTION_EAST
    );
    FactoryCommand bad_output = refinery(
        0, 0, FACTORY_DIRECTION_WEST, (FactoryDirection)99
    );
    const FactoryCommandResult *result;
    FactoryRefinery state;
    FactoryEntityId id;

    CHECK(factory_simulation_submit_command(simulation, &same)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_simulation_submit_command(simulation, &bad_input)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_simulation_submit_command(simulation, &bad_output)
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
    CHECK(factory_simulation_is_refinery(simulation, id));
    CHECK(factory_simulation_get_refinery(simulation, id, &state));
    CHECK(state.x == 1 && state.y == 0);
    CHECK(state.input_direction == FACTORY_DIRECTION_WEST);
    CHECK(state.output_direction == FACTORY_DIRECTION_EAST);
    CHECK(state.recipe_id == FACTORY_RECIPE_NONE);
    CHECK(state.input_item == FACTORY_ITEM_NONE && state.input_amount == 0U);
    CHECK(state.output_item == FACTORY_ITEM_NONE && state.output_amount == 0U);
    CHECK(!state.processing);
    CHECK(state.processing_progress == 0U);
    CHECK(factory_world_get_tile(world, 1, 0)->occupying_entity == id);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_TILE_OCCUPIED);
    CHECK(factory_simulation_get_command_result(simulation, 2U)->result
        == FACTORY_RESULT_OUT_OF_BOUNDS);
    CHECK(!factory_simulation_entity_is_valid(simulation, id + 1U));
    CHECK(!factory_simulation_get_refinery(simulation, id, NULL));

    {
        FactoryCommand select = {
            FACTORY_COMMAND_SET_REFINERY_RECIPE,
            {.set_refinery_recipe = {id, FACTORY_RECIPE_IRON_PLATE}}
        };
        CHECK(factory_simulation_submit_command(
            simulation, &select
        ) == FACTORY_RESULT_OK);
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(simulation, 0U)->result
            == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_refinery(simulation, id, &state));
        CHECK(state.recipe_id == FACTORY_RECIPE_IRON_PLATE);
        CHECK(factory_simulation_submit_command(
            simulation, &select
        ) == FACTORY_RESULT_OK);
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(simulation, 0U)->result
            == FACTORY_RESULT_OK);
        select.data.set_refinery_recipe.recipe_id = (FactoryRecipeId)99;
        CHECK(factory_simulation_submit_command(
            simulation, &select
        ) == FACTORY_RESULT_OK);
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(simulation, 0U)->result
            == FACTORY_RESULT_INVALID_ARGUMENT);
        CHECK(factory_simulation_get_refinery(simulation, id, &state));
        CHECK(state.recipe_id == FACTORY_RECIPE_IRON_PLATE);
    }

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    if (failures != 0) {
        return 1;
    }
    (void)printf("All refinery tests passed.\n");
    return 0;
}
