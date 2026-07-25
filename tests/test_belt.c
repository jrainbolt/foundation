#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand belt_command(
    int32_t x,
    int32_t y,
    FactoryDirection direction
)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {x, y, direction}}
    };
    return command;
}

int main(void)
{
    FactoryWorld *world = factory_world_create(3U, 2U);
    FactorySimulation *simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryCommand first = belt_command(1, 0, FACTORY_DIRECTION_WEST);
    FactoryCommand occupied = belt_command(1, 0, FACTORY_DIRECTION_EAST);
    FactoryCommand outside = belt_command(3, 0, FACTORY_DIRECTION_NORTH);
    FactoryCommand invalid = belt_command(0, 0, (FactoryDirection)99);
    const FactoryCommandResult *result;
    FactoryBelt belt;
    FactoryEntityId id;

    CHECK(simulation != NULL);
    CHECK(factory_simulation_submit_command(simulation, &invalid)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_simulation_submit_command(simulation, &first)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &occupied)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &outside)
        == FACTORY_RESULT_OK);
    CHECK(factory_world_get_tile(world, 1, 0)->occupying_entity == 0U);
    factory_simulation_tick(simulation);

    result = factory_simulation_get_command_result(simulation, 0U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK);
    id = result == NULL ? 0U : result->entity_id;
    CHECK(factory_simulation_entity_is_valid(simulation, id));
    CHECK(factory_simulation_is_belt(simulation, id));
    CHECK(factory_simulation_get_belt(simulation, id, &belt));
    CHECK(belt.x == 1 && belt.y == 0);
    CHECK(belt.direction == FACTORY_DIRECTION_WEST);
    CHECK(belt.item == FACTORY_ITEM_NONE);
    CHECK(belt.movement_progress == 0U);
    CHECK(factory_world_get_tile(world, 1, 0)->occupying_entity == id);

    result = factory_simulation_get_command_result(simulation, 1U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_TILE_OCCUPIED);
    result = factory_simulation_get_command_result(simulation, 2U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OUT_OF_BOUNDS);
    CHECK(!factory_simulation_entity_is_valid(simulation, id + 1U));
    CHECK(factory_world_get_tile(world, 2, 0)->occupying_entity == 0U);
    CHECK(!factory_simulation_get_belt(simulation, id, NULL));

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    if (failures != 0) {
        return 1;
    }
    (void)printf("All belt tests passed.\n");
    return 0;
}
