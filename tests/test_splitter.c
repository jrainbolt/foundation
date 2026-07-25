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

static FactoryCommand splitter(
    int32_t x,
    int32_t y,
    FactoryDirection facing
)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_SPLITTER,
        {.place_splitter = {x, y, facing}}
    };
    return command;
}

typedef struct {
    FactoryWorld *world;
    FactorySimulation *simulation;
    FactoryEntityId ids[7];
} Network;

static Network create_network(bool left_output, bool right_output)
{
    Network network = {0};
    size_t result_index = 0U;

    network.world = factory_world_create(4U, 3U);
    CHECK(factory_world_add_resource(
        network.world, 0, 1, FACTORY_RESOURCE_IRON, 6U
    ) == FACTORY_RESULT_OK);
    network.simulation = factory_simulation_create_with_construction_units(network.world, UINT32_MAX);
    submit(network.simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(network.simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(network.simulation, splitter(2, 1, FACTORY_DIRECTION_EAST));
    if (left_output) {
        submit(network.simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_BELT,
            {.place_belt = {2, 0, FACTORY_DIRECTION_EAST}}
        });
        submit(network.simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_STORAGE,
            {.place_storage = {3, 0}}
        });
    }
    if (right_output) {
        submit(network.simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_BELT,
            {.place_belt = {2, 2, FACTORY_DIRECTION_EAST}}
        });
        submit(network.simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_STORAGE,
            {.place_storage = {3, 2}}
        });
    }
    factory_simulation_tick(network.simulation);
    while (result_index
        < factory_simulation_get_command_result_count(network.simulation)) {
        const FactoryCommandResult *result =
            factory_simulation_get_command_result(
                network.simulation, result_index
            );
        network.ids[result_index] = result->entity_id;
        ++result_index;
    }
    return network;
}

static void destroy_network(Network *network)
{
    factory_simulation_destroy(network->simulation);
    factory_world_destroy(network->world);
}

static uint32_t accounted(const Network *network)
{
    uint32_t total =
        factory_world_get_tile(network->world, 0, 1)->resource_amount;

    for (FactoryEntityId id = 1U;
        id <= factory_simulation_get_entity_count(network->simulation);
        ++id) {
        FactoryExtractor extractor;
        FactoryBelt belt;
        FactorySplitter splitter_state;
        FactoryStorage storage;

        if (factory_simulation_get_extractor(
                network->simulation, id, &extractor)) {
            total += extractor.output_amount;
        } else if (factory_simulation_get_belt(
                network->simulation, id, &belt)) {
            total += belt.item == FACTORY_ITEM_NONE ? 0U : 1U;
        } else if (factory_simulation_get_splitter(
                network->simulation, id, &splitter_state)) {
            total += splitter_state.item == FACTORY_ITEM_NONE ? 0U : 1U;
        } else if (factory_simulation_get_storage(
                network->simulation, id, &storage)) {
            total += factory_storage_get_total_amount(&storage);
        }
    }
    return total;
}

static void test_placement_and_round_robin(void)
{
    Network network = create_network(true, true);
    FactorySplitter state;
    FactoryStorage left;
    FactoryStorage right;

    CHECK(factory_simulation_is_splitter(network.simulation, 3U));
    CHECK(factory_simulation_get_splitter(network.simulation, 3U, &state));
    CHECK(state.x == 2 && state.y == 1);
    CHECK(state.facing == FACTORY_DIRECTION_EAST);
    CHECK(state.item == FACTORY_ITEM_NONE);
    CHECK(state.next_output == FACTORY_SPLITTER_OUTPUT_LEFT);
    CHECK(factory_world_get_tile(network.world, 2, 1)->occupying_entity == 3U);
    for (uint32_t tick = 1U; tick < 90U; ++tick) {
        factory_simulation_tick(network.simulation);
        CHECK(accounted(&network) == 6U);
    }
    CHECK(factory_simulation_get_storage(network.simulation, 5U, &left));
    CHECK(factory_simulation_get_storage(network.simulation, 7U, &right));
    CHECK(left.iron_ore_amount == 2U);
    CHECK(right.iron_ore_amount == 2U);
    CHECK(factory_simulation_get_splitter(network.simulation, 3U, &state));
    CHECK(state.next_output == FACTORY_SPLITTER_OUTPUT_LEFT);
    destroy_network(&network);
}

static void test_blocking_and_demolition(void)
{
    Network alternate = create_network(false, true);
    FactorySplitter state;
    FactoryStorage storage;

    while (factory_simulation_get_tick(alternate.simulation) < 30U) {
        factory_simulation_tick(alternate.simulation);
    }
    CHECK(factory_simulation_get_storage(alternate.simulation, 5U, &storage));
    CHECK(storage.iron_ore_amount == 1U);
    CHECK(factory_simulation_get_splitter(alternate.simulation, 3U, &state));
    CHECK(state.next_output == FACTORY_SPLITTER_OUTPUT_LEFT);
    CHECK(accounted(&alternate) == 6U);
    destroy_network(&alternate);

    {
        Network blocked = create_network(false, false);
        FactoryCommand demolition = {
            FACTORY_COMMAND_DEMOLISH_ENTITY,
            {.demolish_entity = {3U}}
        };

        while (factory_simulation_get_tick(blocked.simulation) < 25U) {
            factory_simulation_tick(blocked.simulation);
        }
        CHECK(factory_simulation_get_splitter(
            blocked.simulation, 3U, &state
        ));
        CHECK(state.item == FACTORY_ITEM_IRON_ORE);
        CHECK(state.next_output == FACTORY_SPLITTER_OUTPUT_LEFT);
        submit(blocked.simulation, demolition);
        factory_simulation_tick(blocked.simulation);
        CHECK(factory_simulation_get_command_result(
            blocked.simulation, 0U
        )->result == FACTORY_RESULT_ENTITY_HAS_MATERIAL);
        CHECK(factory_simulation_get_splitter(
            blocked.simulation, 3U, &state
        ));
        CHECK(state.item == FACTORY_ITEM_IRON_ORE);
        CHECK(accounted(&blocked) == 6U);
        destroy_network(&blocked);
    }

    {
        FactoryWorld *world = factory_world_create(1U, 1U);
        FactorySimulation *simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);

        submit(simulation, splitter(0, 0, FACTORY_DIRECTION_NORTH));
        factory_simulation_tick(simulation);
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_DEMOLISH_ENTITY,
            {.demolish_entity = {1U}}
        });
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(simulation, 0U)->result
            == FACTORY_RESULT_OK);
        CHECK(!factory_simulation_is_splitter(simulation, 1U));
        CHECK(factory_world_get_tile(world, 0, 0)->occupying_entity == 0U);
        factory_simulation_destroy(simulation);
        factory_world_destroy(world);
    }
}

static void test_invalid_placement_and_determinism(void)
{
    FactoryWorld *world = factory_world_create(2U, 1U);
    FactorySimulation *simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryCommand invalid = splitter(0, 0, (FactoryDirection)99);
    FactoryCommand outside = splitter(2, 0, FACTORY_DIRECTION_EAST);

    CHECK(factory_simulation_submit_command(simulation, &invalid)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    submit(simulation, splitter(0, 0, FACTORY_DIRECTION_WEST));
    submit(simulation, splitter(0, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, outside);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_TILE_OCCUPIED);
    CHECK(factory_simulation_get_command_result(simulation, 2U)->result
        == FACTORY_RESULT_OUT_OF_BOUNDS);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);

    {
        Network a = create_network(true, true);
        Network b = create_network(true, true);

        for (uint32_t tick = 1U; tick < 130U; ++tick) {
            FactorySplitter sa;
            FactorySplitter sb;

            factory_simulation_tick(a.simulation);
            factory_simulation_tick(b.simulation);
            CHECK(factory_simulation_get_tick(a.simulation)
                == factory_simulation_get_tick(b.simulation));
            CHECK(accounted(&a) == accounted(&b));
            CHECK(factory_simulation_get_splitter(a.simulation, 3U, &sa));
            CHECK(factory_simulation_get_splitter(b.simulation, 3U, &sb));
            CHECK(sa.item == sb.item);
            CHECK(sa.next_output == sb.next_output);
        }
        CHECK(accounted(&a) == 6U);
        destroy_network(&a);
        destroy_network(&b);
    }
}

int main(void)
{
    test_placement_and_round_robin();
    test_blocking_and_demolition();
    test_invalid_placement_and_determinism();
    if (failures != 0) {
        return 1;
    }
    (void)printf("All splitter tests passed.\n");
    return 0;
}
