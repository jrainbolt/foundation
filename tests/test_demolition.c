#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static FactoryCommand demolish(FactoryEntityId id)
{
    FactoryCommand command = {
        FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity = {id}}
    };
    return command;
}

static FactoryCommand belt(int32_t x, int32_t y)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {x, y, FACTORY_DIRECTION_EAST}}
    };
    return command;
}

static FactoryCommand storage(int32_t x, int32_t y)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage = {x, y}}
    };
    return command;
}

static void test_empty_lifecycle_and_fifo(void)
{
    FactoryWorld *world = factory_world_create(8U, 1U);
    FactorySimulation *simulation;
    FactoryEntityId ids[8];
    const FactoryCommand placements[8] = {
        {FACTORY_COMMAND_PLACE_EXTRACTOR,
            {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt = {2, 0, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt = {3, 0, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_REFINERY,
            {.place_refinery = {
                4, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
            }}},
        {FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler = {5, 0, FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {6, 0}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt = {7, 0, FACTORY_DIRECTION_WEST}}}
    };
    const FactoryCommandResult *result;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    for (size_t index = 0U; index < 8U; ++index) {
        submit(simulation, placements[index]);
    }
    factory_simulation_tick(simulation);
    for (size_t index = 0U; index < 8U; ++index) {
        ids[index] =
            factory_simulation_get_command_result(simulation, index)->entity_id;
    }
    CHECK(factory_simulation_get_entity_count(simulation) == 8U);

    /* Middle belt removal followed by same-tick reuse succeeds FIFO. */
    submit(simulation, demolish(ids[2]));
    submit(simulation, storage(2, 0));
    CHECK(factory_simulation_entity_is_valid(simulation, ids[2]));
    factory_simulation_tick(simulation);
    result = factory_simulation_get_command_result(simulation, 0U);
    CHECK(result->result == FACTORY_RESULT_OK);
    CHECK(result->entity_id == ids[2]);
    CHECK(result->entity_type == FACTORY_ENTITY_TYPE_BELT);
    CHECK(result->x == 2 && result->y == 0);
    CHECK(!factory_simulation_entity_is_valid(simulation, ids[2]));
    CHECK(!factory_simulation_is_belt(simulation, ids[2]));
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->entity_id
        > ids[7]);
    CHECK(factory_simulation_is_belt(simulation, ids[1]));
    CHECK(factory_simulation_is_belt(simulation, ids[3]));
    CHECK(factory_world_get_tile(world, 2, 0)->occupying_entity
        == factory_simulation_get_command_result(simulation, 1U)->entity_id);

    /* All empty entity types are removable; extractor progress is discarded. */
    submit(simulation, demolish(ids[0]));
    submit(simulation, demolish(ids[1]));
    submit(simulation, demolish(ids[3]));
    submit(simulation, demolish(ids[4]));
    submit(simulation, demolish(ids[5]));
    submit(simulation, demolish(ids[6]));
    submit(simulation, demolish(ids[7]));
    submit(simulation, demolish(9U));
    factory_simulation_tick(simulation);
    for (size_t index = 0U; index < 8U; ++index) {
        CHECK(factory_simulation_get_command_result(simulation, index)->result
            == FACTORY_RESULT_OK);
    }
    CHECK(factory_simulation_get_entity_count(simulation) == 0U);
    CHECK(factory_world_get_tile(world, 0, 0)->resource
        == FACTORY_RESOURCE_IRON);
    CHECK(factory_world_get_tile(world, 0, 0)->resource_amount == 10U);
    for (int32_t x = 0; x < 8; ++x) {
        CHECK(factory_world_get_tile(world, x, 0)->occupying_entity == 0U);
    }

    submit(simulation, demolish(ids[0]));
    submit(simulation, demolish(0U));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_ENTITY_NOT_FOUND);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_INVALID_ARGUMENT);

    /* Reverse order: placement sees occupancy before later demolition. */
    submit(simulation, belt(1, 0));
    factory_simulation_tick(simulation);
    ids[0] = factory_simulation_get_command_result(simulation, 0U)->entity_id;
    submit(simulation, storage(1, 0));
    submit(simulation, demolish(ids[0]));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_TILE_OCCUPIED);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_world_get_tile(world, 1, 0)->occupying_entity == 0U);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_material_blocks_demolition(void)
{
    FactoryWorld *world = factory_world_create(2U, 1U);
    FactorySimulation *simulation;
    FactoryBelt state;
    uint32_t deposit_before;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 2U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, belt(1, 0));
    for (uint32_t tick = 0U; tick < 20U; ++tick) {
        factory_simulation_tick(simulation);
    }
    CHECK(factory_simulation_get_belt(simulation, 2U, &state));
    CHECK(state.item == FACTORY_ITEM_IRON_ORE);
    deposit_before = factory_world_get_tile(world, 0, 0)->resource_amount;
    submit(simulation, demolish(2U));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    CHECK(factory_simulation_entity_is_valid(simulation, 2U));
    CHECK(factory_simulation_get_belt(simulation, 2U, &state));
    CHECK(state.item == FACTORY_ITEM_IRON_ORE);
    CHECK(factory_world_get_tile(world, 1, 0)->occupying_entity == 2U);
    CHECK(factory_world_get_tile(world, 0, 0)->resource_amount
        == deposit_before);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_lifecycle_determinism(void)
{
    FactoryWorld *world_a = factory_world_create(2U, 1U);
    FactoryWorld *world_b = factory_world_create(2U, 1U);
    FactorySimulation *a = factory_simulation_create_with_construction_units(world_a, UINT32_MAX);
    FactorySimulation *b = factory_simulation_create_with_construction_units(world_b, UINT32_MAX);

    submit(a, belt(0, 0));
    submit(b, belt(0, 0));
    factory_simulation_tick(a);
    factory_simulation_tick(b);
    submit(a, demolish(1U));
    submit(a, storage(0, 0));
    submit(b, demolish(1U));
    submit(b, storage(0, 0));
    factory_simulation_tick(a);
    factory_simulation_tick(b);
    CHECK(factory_simulation_get_tick(a) == factory_simulation_get_tick(b));
    CHECK(factory_simulation_get_command_result(a, 0U)->result
        == factory_simulation_get_command_result(b, 0U)->result);
    CHECK(factory_simulation_get_command_result(a, 1U)->entity_id
        == factory_simulation_get_command_result(b, 1U)->entity_id);
    CHECK(factory_world_get_tile(world_a, 0, 0)->occupying_entity
        == factory_world_get_tile(world_b, 0, 0)->occupying_entity);
    factory_simulation_destroy(a);
    factory_simulation_destroy(b);
    factory_world_destroy(world_a);
    factory_world_destroy(world_b);
}

int main(void)
{
    test_empty_lifecycle_and_fifo();
    test_material_blocks_demolition();
    test_lifecycle_determinism();
    if (failures != 0) {
        return 1;
    }
    (void)printf("All demolition tests passed.\n");
    return 0;
}
