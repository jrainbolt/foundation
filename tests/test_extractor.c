#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand place(
    int32_t x,
    int32_t y,
    FactoryDirection direction
)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {x, y, direction}}
    };
    return command;
}

static FactoryEntityId result_entity(FactorySimulation *simulation)
{
    const FactoryCommandResult *result =
        factory_simulation_get_command_result(simulation, 0U);
    CHECK(result != NULL);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK);
    return result == NULL ? 0U : result->entity_id;
}

static void test_placement_and_production(void)
{
    FactoryWorld *world = factory_world_create(4U, 2U);
    FactorySimulation *simulation;
    FactoryCommand command = place(1, 0, FACTORY_DIRECTION_WEST);
    FactoryExtractor extractor;
    FactoryEntityId entity;
    uint32_t index;

    CHECK(world != NULL);
    CHECK(factory_world_add_resource(
        world, 1, 0, FACTORY_RESOURCE_IRON, 100U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    CHECK(factory_simulation_submit_command(
        simulation, &command
    ) == FACTORY_RESULT_OK);
    factory_simulation_tick(simulation);
    entity = result_entity(simulation);
    CHECK(entity != 0U);
    CHECK(factory_simulation_entity_is_valid(simulation, entity));
    CHECK(factory_simulation_is_extractor(simulation, entity));
    CHECK(factory_world_get_tile(world, 1, 0)->occupying_entity == entity);
    CHECK(factory_simulation_get_extractor(simulation, entity, &extractor));
    CHECK(extractor.x == 1 && extractor.y == 0);
    CHECK(extractor.output_direction == FACTORY_DIRECTION_WEST);
    CHECK(extractor.production_progress == 1U);
    CHECK(extractor.output_item == FACTORY_ITEM_NONE);
    CHECK(extractor.output_amount == 0U);

    for (index = 1U; index < FACTORY_EXTRACTOR_PRODUCTION_TICKS - 1U; ++index) {
        factory_simulation_tick(simulation);
    }
    CHECK(factory_simulation_get_extractor(simulation, entity, &extractor));
    CHECK(extractor.production_progress == 19U);
    CHECK(extractor.output_amount == 0U);
    CHECK(factory_world_get_tile(world, 1, 0)->resource_amount == 100U);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_tick(simulation) == 20U);
    CHECK(factory_simulation_get_extractor(simulation, entity, &extractor));
    CHECK(extractor.production_progress == 0U);
    CHECK(extractor.output_item == FACTORY_ITEM_IRON_ORE);
    CHECK(extractor.output_amount == 1U);
    CHECK(factory_world_get_tile(world, 1, 0)->resource_amount == 99U);
    CHECK(100U == factory_world_get_tile(world, 1, 0)->resource_amount
        + extractor.output_amount);

    for (index = 0U; index < 40U; ++index) {
        factory_simulation_tick(simulation);
    }
    CHECK(factory_simulation_get_extractor(simulation, entity, &extractor));
    CHECK(extractor.production_progress == 0U);
    CHECK(extractor.output_amount == 1U);
    CHECK(factory_world_get_tile(world, 1, 0)->resource_amount == 99U);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_failed_placements_and_fifo(void)
{
    FactoryWorld *world = factory_world_create(4U, 1U);
    FactorySimulation *simulation;
    FactoryCommand commands[5];
    FactoryEntityId successful;

    CHECK(factory_world_add_resource(
        world, 1, 0, FACTORY_RESOURCE_COPPER, 5U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 2, 0, FACTORY_RESOURCE_IRON, 5U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    commands[0] = place(-1, 0, FACTORY_DIRECTION_NORTH);
    commands[1] = place(0, 0, FACTORY_DIRECTION_NORTH);
    commands[2] = place(1, 0, FACTORY_DIRECTION_NORTH);
    commands[3] = place(2, 0, FACTORY_DIRECTION_EAST);
    commands[4] = place(2, 0, FACTORY_DIRECTION_SOUTH);
    for (size_t index = 0U; index < 5U; ++index) {
        CHECK(factory_simulation_submit_command(
            simulation, &commands[index]
        ) == FACTORY_RESULT_OK);
    }
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OUT_OF_BOUNDS);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_NO_RESOURCE);
    CHECK(factory_simulation_get_command_result(simulation, 2U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 3U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 4U)->result
        == FACTORY_RESULT_TILE_OCCUPIED);
    successful =
        factory_simulation_get_command_result(simulation, 3U)->entity_id;
    CHECK(successful == 2U);
    CHECK(factory_simulation_entity_is_valid(simulation, successful));
    CHECK(!factory_simulation_entity_is_valid(simulation, successful + 1U));
    CHECK(factory_world_get_tile(world, 0, 0)->occupying_entity == 0U);
    CHECK(factory_world_get_tile(world, 1, 0)->occupying_entity == 1U);
    CHECK(factory_world_get_tile(world, 1, 0)->resource_amount == 5U);
    CHECK(factory_world_get_tile(world, 2, 0)->resource_amount == 5U);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_depleted_and_multiple(void)
{
    FactoryWorld *world = factory_world_create(2U, 1U);
    FactorySimulation *simulation;
    FactoryCommand first = place(0, 0, FACTORY_DIRECTION_NORTH);
    FactoryCommand second = place(1, 0, FACTORY_DIRECTION_SOUTH);
    FactoryExtractor a;
    FactoryExtractor b;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 0U
    ) == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 1U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 1, 0, FACTORY_RESOURCE_IRON, 1U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    CHECK(factory_simulation_submit_command(simulation, &first)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &second)
        == FACTORY_RESULT_OK);
    for (uint32_t index = 0U; index < 20U; ++index) {
        factory_simulation_tick(simulation);
    }
    CHECK(factory_simulation_get_extractor(simulation, 1U, &a));
    CHECK(factory_simulation_get_extractor(simulation, 2U, &b));
    CHECK(a.output_amount == 1U && b.output_amount == 1U);
    CHECK(factory_world_get_tile(world, 0, 0)->resource_amount == 0U);
    CHECK(factory_world_get_tile(world, 1, 0)->resource_amount == 0U);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    test_placement_and_production();
    test_failed_placements_and_fifo();
    test_depleted_and_multiple();
    if (failures != 0) {
        return 1;
    }
    (void)printf("All extractor tests passed.\n");
    return 0;
}
