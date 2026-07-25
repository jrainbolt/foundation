#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand place(int32_t x, int32_t y)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {x, y, FACTORY_DIRECTION_NORTH}}
    };
    return command;
}

int main(void)
{
    FactoryWorld *world = factory_world_create(40U, 1U);
    FactorySimulation *simulation;
    FactoryCommand command;
    size_t index;

    CHECK(world != NULL);
    for (index = 0U; index < 40U; ++index) {
        CHECK(factory_world_add_resource(
            world, (int32_t)index, 0, FACTORY_RESOURCE_IRON, 1U
        ) == FACTORY_RESULT_OK);
    }
    simulation = factory_simulation_create(world);
    CHECK(simulation != NULL);
    command = place(0, 0);
    CHECK(factory_simulation_submit_command(NULL, &command)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_simulation_submit_command(simulation, NULL)
        == FACTORY_RESULT_INVALID_ARGUMENT);

    CHECK(factory_simulation_submit_command(
        simulation, &command
    ) == FACTORY_RESULT_OK);
    command.data.place_extractor.x = 39;
    CHECK(factory_world_get_tile(world, 0, 0)->occupying_entity == 0U);
    CHECK(factory_simulation_get_pending_command_count(simulation) == 1U);
    factory_simulation_tick(simulation);
    CHECK(factory_world_get_tile(world, 0, 0)->occupying_entity != 0U);
    CHECK(factory_world_get_tile(world, 39, 0)->occupying_entity == 0U);
    CHECK(factory_simulation_get_pending_command_count(simulation) == 0U);

    for (index = 1U; index <= FACTORY_COMMAND_QUEUE_CAPACITY; ++index) {
        command = place((int32_t)index, 0);
        CHECK(factory_simulation_submit_command(
            simulation, &command
        ) == FACTORY_RESULT_OK);
    }
    command = place(39, 0);
    CHECK(factory_simulation_submit_command(
        simulation, &command
    ) == FACTORY_RESULT_QUEUE_FULL);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result_count(simulation)
        == FACTORY_COMMAND_QUEUE_CAPACITY);
    for (index = 0U; index < FACTORY_COMMAND_QUEUE_CAPACITY; ++index) {
        const FactoryCommandResult *result =
            factory_simulation_get_command_result(simulation, index);
        CHECK(result != NULL);
        CHECK(result->command.data.place_extractor.x == (int32_t)index + 1);
        CHECK(result->result == FACTORY_RESULT_OK);
    }
    CHECK(factory_simulation_get_command_result(
        simulation, FACTORY_COMMAND_QUEUE_CAPACITY
    ) == NULL);

    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result_count(simulation) == 0U);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);

    if (failures != 0) {
        return 1;
    }
    (void)printf("All command tests passed.\n");
    return 0;
}
