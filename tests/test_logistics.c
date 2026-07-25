#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand extractor(int32_t x, int32_t y, FactoryDirection d)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {x, y, d}}
    };
    return command;
}

static FactoryCommand belt(int32_t x, int32_t y, FactoryDirection d)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {x, y, d}}
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

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static uint32_t accounted_iron(
    FactorySimulation *simulation,
    FactoryWorld *world,
    const FactoryEntityId *ids,
    size_t count
)
{
    uint32_t total = 0U;
    size_t index;

    for (int32_t y = 0; y < (int32_t)factory_world_get_height(world); ++y) {
        for (int32_t x = 0; x < (int32_t)factory_world_get_width(world); ++x) {
            total += factory_world_get_tile(world, x, y)->resource_amount;
        }
    }
    for (index = 0U; index < count; ++index) {
        FactoryExtractor extractor_state;
        FactoryBelt belt_state;
        FactoryStorage storage_state;

        if (factory_simulation_get_extractor(
                simulation, ids[index], &extractor_state)) {
            total += extractor_state.output_amount;
        } else if (factory_simulation_get_belt(
                simulation, ids[index], &belt_state)) {
            total += belt_state.item == FACTORY_ITEM_IRON_ORE ? 1U : 0U;
        } else if (factory_simulation_get_storage(
                simulation, ids[index], &storage_state)) {
            total += storage_state.iron_ore_amount;
        }
    }
    return total;
}

static void test_full_pipeline(void)
{
    FactoryWorld *world = factory_world_create(4U, 1U);
    FactorySimulation *simulation;
    FactoryEntityId ids[4];
    FactoryExtractor extractor_state;
    FactoryBelt first;
    FactoryBelt second;
    FactoryStorage storage_state;
    uint32_t tick;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, extractor(0, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(1, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(2, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, storage(3, 0));
    factory_simulation_tick(simulation);
    for (size_t index = 0U; index < 4U; ++index) {
        ids[index] =
            factory_simulation_get_command_result(simulation, index)->entity_id;
    }
    CHECK(accounted_iron(simulation, world, ids, 4U) == 10U);
    for (tick = 1U; tick < 20U; ++tick) {
        factory_simulation_tick(simulation);
        CHECK(accounted_iron(simulation, world, ids, 4U) == 10U);
    }
    CHECK(factory_simulation_get_extractor(simulation, ids[0], &extractor_state));
    CHECK(factory_simulation_get_belt(simulation, ids[1], &first));
    CHECK(extractor_state.output_amount == 0U);
    CHECK(first.item == FACTORY_ITEM_IRON_ORE);
    CHECK(first.movement_progress == 1U);
    CHECK(factory_world_get_tile(world, 0, 0)->resource_amount == 9U);

    for (tick = 20U; tick < 24U; ++tick) {
        factory_simulation_tick(simulation);
        CHECK(accounted_iron(simulation, world, ids, 4U) == 10U);
    }
    CHECK(factory_simulation_get_belt(simulation, ids[1], &first));
    CHECK(factory_simulation_get_belt(simulation, ids[2], &second));
    CHECK(first.item == FACTORY_ITEM_NONE);
    CHECK(second.item == FACTORY_ITEM_IRON_ORE);
    CHECK(second.movement_progress == 0U);

    for (tick = 24U; tick < 29U; ++tick) {
        factory_simulation_tick(simulation);
        CHECK(accounted_iron(simulation, world, ids, 4U) == 10U);
    }
    CHECK(factory_simulation_get_storage(simulation, ids[3], &storage_state));
    CHECK(storage_state.iron_ore_amount == 1U);
    CHECK(factory_simulation_get_belt(simulation, ids[2], &second));
    CHECK(second.item == FACTORY_ITEM_NONE);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_blocked_and_conflict(void)
{
    FactoryWorld *world = factory_world_create(3U, 3U);
    FactorySimulation *simulation;
    FactoryEntityId ids[5];
    FactoryBelt low;
    FactoryBelt high;
    FactoryBelt destination;
    uint32_t tick;

    CHECK(factory_world_add_resource(
        world, 0, 1, FACTORY_RESOURCE_IRON, 2U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 1, 0, FACTORY_RESOURCE_IRON, 2U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, extractor(0, 1, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(1, 1, FACTORY_DIRECTION_EAST));
    submit(simulation, extractor(1, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(2, 0, FACTORY_DIRECTION_SOUTH));
    submit(simulation, belt(2, 1, FACTORY_DIRECTION_EAST));
    factory_simulation_tick(simulation);
    for (size_t index = 0U; index < 5U; ++index) {
        ids[index] =
            factory_simulation_get_command_result(simulation, index)->entity_id;
    }
    for (tick = 1U; tick < 24U; ++tick) {
        factory_simulation_tick(simulation);
        CHECK(accounted_iron(simulation, world, ids, 5U) == 4U);
    }
    CHECK(factory_simulation_get_belt(simulation, ids[1], &low));
    CHECK(factory_simulation_get_belt(simulation, ids[3], &high));
    CHECK(factory_simulation_get_belt(simulation, ids[4], &destination));
    CHECK(low.item == FACTORY_ITEM_NONE);
    CHECK(high.item == FACTORY_ITEM_IRON_ORE);
    CHECK(high.movement_progress == FACTORY_BELT_TRANSFER_TICKS);
    CHECK(destination.item == FACTORY_ITEM_IRON_ORE);
    CHECK(destination.movement_progress == 0U);
    CHECK(accounted_iron(simulation, world, ids, 5U) == 4U);

    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_belt(simulation, ids[3], &high));
    CHECK(high.item == FACTORY_ITEM_IRON_ORE);
    CHECK(high.movement_progress == FACTORY_BELT_TRANSFER_TICKS);
    CHECK(accounted_iron(simulation, world, ids, 5U) == 4U);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_cardinal_directions(void)
{
    static const struct {
        int32_t extractor_x;
        int32_t extractor_y;
        int32_t belt_x;
        int32_t belt_y;
        int32_t storage_x;
        int32_t storage_y;
        FactoryDirection input_direction;
        FactoryDirection belt_direction;
    } cases[] = {
        {0, 1, 1, 1, 2, 1, FACTORY_DIRECTION_EAST, FACTORY_DIRECTION_EAST},
        {2, 1, 1, 1, 0, 1, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_WEST},
        {1, 2, 1, 1, 1, 0, FACTORY_DIRECTION_NORTH, FACTORY_DIRECTION_NORTH},
        {1, 0, 1, 1, 1, 2, FACTORY_DIRECTION_SOUTH, FACTORY_DIRECTION_SOUTH}
    };

    for (size_t index = 0U; index < 4U; ++index) {
        FactoryWorld *world = factory_world_create(3U, 3U);
        FactorySimulation *simulation;
        FactoryStorage state;

        CHECK(factory_world_add_resource(
            world,
            cases[index].extractor_x,
            cases[index].extractor_y,
            FACTORY_RESOURCE_IRON,
            1U
        ) == FACTORY_RESULT_OK);
        simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
        submit(simulation, extractor(
            cases[index].extractor_x,
            cases[index].extractor_y,
            cases[index].input_direction
        ));
        submit(simulation, belt(
            cases[index].belt_x,
            cases[index].belt_y,
            cases[index].belt_direction
        ));
        submit(simulation, storage(
            cases[index].storage_x, cases[index].storage_y
        ));
        for (uint32_t tick = 0U; tick < 24U; ++tick) {
            factory_simulation_tick(simulation);
        }
        CHECK(factory_simulation_get_storage(simulation, 3U, &state));
        CHECK(state.iron_ore_amount == 1U);
        factory_simulation_destroy(simulation);
        factory_world_destroy(world);
    }
}

static void test_full_storage_stalls(void)
{
    FactoryWorld *world = factory_world_create(3U, 1U);
    FactorySimulation *simulation;
    FactoryEntityId ids[3];
    FactoryStorage storage_state;
    FactoryBelt belt_state;
    uint32_t tick = 0U;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON,
        FACTORY_STORAGE_IRON_ORE_CAPACITY + 1U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, extractor(0, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(1, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, storage(2, 0));
    factory_simulation_tick(simulation);
    for (size_t index = 0U; index < 3U; ++index) {
        ids[index] =
            factory_simulation_get_command_result(simulation, index)->entity_id;
    }
    while (tick < 3000U) {
        factory_simulation_get_storage(simulation, ids[2], &storage_state);
        if (storage_state.iron_ore_amount
            == FACTORY_STORAGE_IRON_ORE_CAPACITY) {
            break;
        }
        factory_simulation_tick(simulation);
        CHECK(accounted_iron(
            simulation, world, ids, 3U
        ) == FACTORY_STORAGE_IRON_ORE_CAPACITY + 1U);
        ++tick;
    }
    CHECK(storage_state.iron_ore_amount == FACTORY_STORAGE_IRON_ORE_CAPACITY);
    for (tick = 0U; tick < 30U; ++tick) {
        factory_simulation_tick(simulation);
        CHECK(accounted_iron(
            simulation, world, ids, 3U
        ) == FACTORY_STORAGE_IRON_ORE_CAPACITY + 1U);
    }
    CHECK(factory_simulation_get_storage(simulation, ids[2], &storage_state));
    CHECK(storage_state.iron_ore_amount == storage_state.total_capacity);
    CHECK(factory_simulation_get_belt(simulation, ids[1], &belt_state));
    CHECK(belt_state.item == FACTORY_ITEM_IRON_ORE);
    CHECK(belt_state.movement_progress == FACTORY_BELT_TRANSFER_TICKS);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

typedef struct {
    uint64_t tick;
    uint32_t deposit;
    FactoryExtractor extractor_state;
    FactoryBelt first;
    FactoryBelt second;
    FactoryStorage storage_state;
} ObservableState;

static ObservableState deterministic_pipeline_run(void)
{
    FactoryWorld *world = factory_world_create(4U, 1U);
    FactorySimulation *simulation;
    ObservableState state;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, extractor(0, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(1, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(2, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, storage(3, 0));
    for (uint32_t tick = 0U; tick < 80U; ++tick) {
        factory_simulation_tick(simulation);
    }
    state.tick = factory_simulation_get_tick(simulation);
    state.deposit = factory_world_get_tile(world, 0, 0)->resource_amount;
    CHECK(factory_simulation_get_extractor(simulation, 1U, &state.extractor_state));
    CHECK(factory_simulation_get_belt(simulation, 2U, &state.first));
    CHECK(factory_simulation_get_belt(simulation, 3U, &state.second));
    CHECK(factory_simulation_get_storage(simulation, 4U, &state.storage_state));
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
    return state;
}

static void test_deterministic_pipeline(void)
{
    ObservableState first = deterministic_pipeline_run();
    ObservableState second = deterministic_pipeline_run();

    CHECK(first.tick == second.tick);
    CHECK(first.deposit == second.deposit);
    CHECK(first.extractor_state.entity_id == second.extractor_state.entity_id);
    CHECK(first.extractor_state.production_progress
        == second.extractor_state.production_progress);
    CHECK(first.extractor_state.output_item
        == second.extractor_state.output_item);
    CHECK(first.first.entity_id == second.first.entity_id);
    CHECK(first.first.item == second.first.item);
    CHECK(first.first.movement_progress == second.first.movement_progress);
    CHECK(first.second.entity_id == second.second.entity_id);
    CHECK(first.second.item == second.second.item);
    CHECK(first.second.movement_progress == second.second.movement_progress);
    CHECK(first.storage_state.entity_id == second.storage_state.entity_id);
    CHECK(first.storage_state.iron_ore_amount
        == second.storage_state.iron_ore_amount);
}

static void test_blocking_targets(void)
{
    FactoryWorld *world = factory_world_create(4U, 2U);
    FactorySimulation *simulation;
    FactoryEntityId extractor_id;
    FactoryEntityId belt_id;
    FactoryExtractor extractor_state;
    FactoryBelt belt_state;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 3U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 2, 1, FACTORY_RESOURCE_IRON, 1U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, extractor(0, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(1, 0, FACTORY_DIRECTION_NORTH));
    submit(simulation, extractor(2, 1, FACTORY_DIRECTION_NORTH));
    submit(simulation, belt(2, 0, FACTORY_DIRECTION_EAST));
    factory_simulation_tick(simulation);
    extractor_id =
        factory_simulation_get_command_result(simulation, 0U)->entity_id;
    belt_id = factory_simulation_get_command_result(simulation, 1U)->entity_id;
    for (uint32_t tick = 1U; tick < 40U; ++tick) {
        factory_simulation_tick(simulation);
    }
    CHECK(factory_simulation_get_extractor(
        simulation, extractor_id, &extractor_state
    ));
    CHECK(factory_simulation_get_belt(simulation, belt_id, &belt_state));
    /* The first ore fills the out-of-bounds belt; the second stays buffered. */
    CHECK(extractor_state.output_amount == 1U);
    CHECK(belt_state.item == FACTORY_ITEM_IRON_ORE);
    CHECK(belt_state.movement_progress == FACTORY_BELT_TRANSFER_TICKS);

    /* Belt at (2,0) points to empty (3,0), a non-belt/non-storage target. */
    CHECK(factory_simulation_get_belt(simulation, 4U, &belt_state));
    CHECK(belt_state.item == FACTORY_ITEM_IRON_ORE);
    CHECK(belt_state.movement_progress == FACTORY_BELT_TRANSFER_TICKS);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    test_full_pipeline();
    test_blocked_and_conflict();
    test_cardinal_directions();
    test_full_storage_stalls();
    test_deterministic_pipeline();
    test_blocking_targets();
    if (failures != 0) {
        return 1;
    }
    (void)printf("All logistics tests passed.\n");
    return 0;
}
