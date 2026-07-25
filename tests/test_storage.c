#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand storage_command(int32_t x, int32_t y)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage = {x, y}}
    };
    return command;
}

int main(void)
{
    FactoryWorld *world = factory_world_create(2U, 1U);
    FactorySimulation *simulation = factory_simulation_create(world);
    FactoryCommand first = storage_command(0, 0);
    FactoryCommand occupied = storage_command(0, 0);
    FactoryCommand outside = storage_command(-1, 0);
    const FactoryCommandResult *result;
    FactoryStorage storage;
    FactoryEntityId id;

    CHECK(factory_simulation_submit_command(simulation, &first)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &occupied)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &outside)
        == FACTORY_RESULT_OK);
    factory_simulation_tick(simulation);
    result = factory_simulation_get_command_result(simulation, 0U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK);
    id = result == NULL ? 0U : result->entity_id;
    CHECK(factory_simulation_is_storage(simulation, id));
    CHECK(factory_simulation_get_storage(simulation, id, &storage));
    CHECK(storage.x == 0 && storage.y == 0);
    CHECK(storage.iron_ore_amount == 0U);
    CHECK(storage.iron_plate_amount == 0U);
    CHECK(storage.total_capacity == FACTORY_STORAGE_CAPACITY);
    CHECK(factory_world_get_tile(world, 0, 0)->occupying_entity == id);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_TILE_OCCUPIED);
    CHECK(factory_simulation_get_command_result(simulation, 2U)->result
        == FACTORY_RESULT_OUT_OF_BOUNDS);
    CHECK(!factory_simulation_entity_is_valid(simulation, id + 1U));
    CHECK(factory_world_get_tile(world, 1, 0)->occupying_entity == 0U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    if (failures != 0) {
        return 1;
    }
    (void)printf("All storage tests passed.\n");
    return 0;
}
