#include "foundation/snapshot.h"
#include "../src/power_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static FactoryCommand pole(int32_t x, int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {x, y}}
    };
}

static FactoryCommand generator(int32_t x, int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {x, y}}
    };
}

static bool snapshot_equal(
    const FactorySimulation *a, const FactorySimulation *b
)
{
    FactorySnapshotBuffer x = {0};
    FactorySnapshotBuffer y = {0};
    bool equal = false;
    if (factory_simulation_create_snapshot(a, &x) == FACTORY_RESULT_OK
        && factory_simulation_create_snapshot(b, &y) == FACTORY_RESULT_OK) {
        equal = x.size == y.size && memcmp(x.data, y.data, x.size) == 0;
    }
    factory_snapshot_buffer_destroy(&x);
    factory_snapshot_buffer_destroy(&y);
    return equal;
}

static void test_topology_allocation_and_pause(void)
{
    FactoryWorld *world = factory_world_create(20U, 5U);
    FactorySimulation *simulation;
    FactoryPowerPoleInspection pole_state;
    FactoryPowerGeneratorInspection generator_state;
    FactoryPowerConsumerInspection consumer;
    FactoryPowerNetworkInspection network;
    FactoryPowerConnectionInspection edge;
    FactoryExtractor extractor;
    uint32_t balance;

    CHECK(factory_world_add_resource(
        world, 3, 0, FACTORY_RESOURCE_IRON, 20U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 15, 0, FACTORY_RESOURCE_IRON, 20U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, 500U);
    submit(simulation, pole(0, 0));                         /* 1 */
    submit(simulation, generator(0, 1));                    /* 2 */
    submit(simulation, (FactoryCommand){                    /* 3 */
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {3, 0, FACTORY_DIRECTION_SOUTH}}
    });
    submit(simulation, pole(6, 0));                         /* 4 */
    submit(simulation, (FactoryCommand){                    /* 5 */
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            9, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_SOUTH
        }}
    });
    submit(simulation, (FactoryCommand){                    /* 6 */
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {9, 1, FACTORY_DIRECTION_SOUTH}}
    });
    submit(simulation, (FactoryCommand){                    /* 7 */
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {9, 2, FACTORY_DIRECTION_SOUTH}}
    });
    submit(simulation, (FactoryCommand){                    /* 8 */
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {15, 0, FACTORY_DIRECTION_SOUTH}}
    });
    factory_simulation_tick(simulation);

    balance = 500U
        - 2U * FACTORY_CONSTRUCTION_COST_POWER_POLE
        - FACTORY_CONSTRUCTION_COST_POWER_GENERATOR
        - 2U * FACTORY_CONSTRUCTION_COST_EXTRACTOR
        - FACTORY_CONSTRUCTION_COST_REFINERY
        - FACTORY_CONSTRUCTION_COST_ASSEMBLER
        - FACTORY_CONSTRUCTION_COST_INSERTER;
    CHECK(factory_simulation_construction_units(simulation) == balance);
    CHECK(factory_simulation_get_power_connection_count(simulation) == 1U);
    CHECK(factory_simulation_get_power_connection(
        simulation, 0U, &edge
    ) == FACTORY_RESULT_OK);
    CHECK(edge.pole_a == 1U && edge.pole_b == 4U);
    CHECK(factory_simulation_get_power_pole(
        simulation, 1U, &pole_state
    ) == FACTORY_RESULT_OK);
    CHECK(pole_state.network_id == 1U);
    CHECK(pole_state.connected_pole_count == 1U);
    CHECK(factory_simulation_get_power_generator(
        simulation, 2U, &generator_state
    ) == FACTORY_RESULT_OK);
    CHECK(generator_state.connected);
    CHECK(generator_state.attached_pole_id == 1U);
    CHECK(generator_state.generation_capacity
        == FACTORY_BASIC_GENERATOR_CAPACITY);
    CHECK(factory_simulation_get_power_network_count(simulation) == 1U);
    CHECK(factory_simulation_get_power_network(
        simulation, 0U, &network
    ) == FACTORY_RESULT_OK);
    CHECK(network.network_id == 1U);
    CHECK(network.total_generation == 100U);
    CHECK(network.total_demand == 60U);
    CHECK(network.allocated_power == 60U);
    CHECK(network.unused_generation == 40U);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 3U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(consumer.connected && consumer.powered);
    CHECK(consumer.demand == FACTORY_POWER_DEMAND_EXTRACTOR);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 8U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(!consumer.connected && !consumer.powered);
    CHECK(factory_simulation_get_extractor(simulation, 8U, &extractor));
    CHECK(extractor.production_progress == 0U);

    submit(simulation, pole(12, 0));                        /* 9 bridge */
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_power_connection_count(simulation) == 2U);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 8U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(consumer.connected && consumer.powered);
    CHECK(consumer.attached_pole_id == 9U);
    CHECK(factory_simulation_get_extractor(simulation, 8U, &extractor));
    CHECK(extractor.production_progress == 1U);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {9U}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_power_connection_count(simulation) == 1U);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 8U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(!consumer.powered);
    CHECK(factory_simulation_get_extractor(simulation, 8U, &extractor));
    CHECK(extractor.production_progress == 1U);
    CHECK(factory_simulation_construction_units(simulation)
        == balance);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_shortage_and_leftover(void)
{
    FactoryWorld *world = factory_world_create(8U, 8U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryPowerConsumerInspection consumer;
    FactoryPowerNetworkInspection network;

    CHECK(factory_world_add_resource(
        world, 4, 1, FACTORY_RESOURCE_IRON, 5U
    ) == FACTORY_RESULT_OK);
    submit(simulation, pole(3, 3));                         /* 1 */
    submit(simulation, generator(3, 4));                    /* 2 */
    submit(simulation, (FactoryCommand){                    /* 3, 25 */
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {1, 1, FACTORY_DIRECTION_NORTH}}
    });
    submit(simulation, (FactoryCommand){                    /* 4, 25 */
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {2, 1, FACTORY_DIRECTION_NORTH}}
    });
    submit(simulation, (FactoryCommand){                    /* 5, 25 */
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {3, 1, FACTORY_DIRECTION_NORTH}}
    });
    submit(simulation, (FactoryCommand){                    /* 6, 10 */
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {4, 1, FACTORY_DIRECTION_NORTH}}
    });
    submit(simulation, (FactoryCommand){                    /* 7, 25 fails */
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {5, 1, FACTORY_DIRECTION_NORTH}}
    });
    submit(simulation, (FactoryCommand){                    /* 8, 5 uses leftover */
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {3, 2, FACTORY_DIRECTION_NORTH}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_power_network(
        simulation, 0U, &network
    ) == FACTORY_RESULT_OK);
    CHECK(network.total_demand == 115U);
    CHECK(network.allocated_power == 90U);
    CHECK(network.powered_consumer_count == 5U);
    CHECK(network.unpowered_consumer_count == 1U);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 7U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(!consumer.powered);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 8U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(consumer.powered);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_no_implicit_power_and_source_boundary(void)
{
    FactoryWorld *world = factory_world_create(12U, 4U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryPowerConsumerInspection consumer;
    FactoryExtractor extractor;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 1 */
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 1U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(!consumer.connected && !consumer.powered);
    CHECK(factory_simulation_get_extractor(simulation, 1U, &extractor));
    CHECK(extractor.production_progress == 0U);

    submit(simulation, pole(9, 0));                         /* 2 isolated */
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 1U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(!consumer.connected && !consumer.powered);
    CHECK(factory_simulation_get_extractor(simulation, 1U, &extractor));
    CHECK(extractor.production_progress == 0U);

    submit(simulation, generator(9, 1));                    /* 3 remote source */
    factory_simulation_tick(simulation);
    CHECK(factory_power_source_available_generation(simulation, 3U)
        == FACTORY_BASIC_GENERATOR_CAPACITY);
    CHECK(factory_power_source_available_generation(simulation, 1U) == 0U);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 1U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(!consumer.connected && !consumer.powered);

    submit(simulation, pole(3, 0));                         /* 4 bridge/coverage */
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 1U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(consumer.connected && consumer.powered);
    CHECK(factory_simulation_get_extractor(simulation, 1U, &extractor));
    CHECK(extractor.production_progress == 1U);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {3U}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 1U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(consumer.connected && !consumer.powered);
    CHECK(factory_simulation_get_extractor(simulation, 1U, &extractor));
    CHECK(extractor.production_progress == 1U);

    submit(simulation, generator(9, 1));                    /* 5 replacement */
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 1U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(consumer.connected && consumer.powered);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {4U}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_power_consumer(
        simulation, 1U, &consumer
    ) == FACTORY_RESULT_OK);
    CHECK(!consumer.connected && !consumer.powered);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_zero_power_snapshot_and_duplicate_determinism(void)
{
    FactoryWorld *world_a = factory_world_create(2U, 1U);
    FactoryWorld *world_b = factory_world_create(2U, 1U);
    FactorySimulation *a =
        factory_simulation_create_with_construction_units(world_a, UINT32_MAX);
    FactorySimulation *duplicate =
        factory_simulation_create_with_construction_units(world_b, UINT32_MAX);
    FactorySnapshotBuffer snapshot = {0};
    FactorySimulation *loaded = NULL;
    FactoryExtractor state;

    CHECK(factory_world_add_resource(
        world_a, 0, 0, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world_b, 0, 0, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(duplicate, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    for (uint32_t tick = 0U; tick < 20U; ++tick) {
        factory_simulation_tick(a);
        factory_simulation_tick(duplicate);
        CHECK(snapshot_equal(a, duplicate));
    }
    CHECK(factory_simulation_get_extractor(a, 1U, &state));
    CHECK(state.production_progress == 0U);
    CHECK(factory_simulation_create_snapshot(a, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &loaded
    ) == FACTORY_RESULT_OK);
    for (uint32_t tick = 0U; tick < 30U; ++tick) {
        factory_simulation_tick(a);
        factory_simulation_tick(loaded);
        CHECK(snapshot_equal(a, loaded));
    }
    CHECK(factory_simulation_get_extractor(loaded, 1U, &state));
    CHECK(state.production_progress == 0U);

    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(duplicate);
    factory_simulation_destroy(a);
    factory_world_destroy(world_b);
    factory_world_destroy(world_a);
}

static void test_snapshot_continuation(void)
{
    FactoryWorld *world = factory_world_create(10U, 2U);
    FactorySimulation *a =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactorySnapshotBuffer snapshot = {0};
    FactorySnapshotBuffer chain = {0};
    FactorySimulation *b = NULL;

    submit(a, pole(0, 0));
    submit(a, pole(6, 0));
    submit(a, generator(0, 1));
    factory_simulation_tick(a);
    submit(a, pole(9, 0)); /* pending power command */
    CHECK(factory_simulation_create_snapshot(a, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(snapshot.data[8] == 2U);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &b
    ) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(b, &chain)
        == FACTORY_RESULT_OK);
    CHECK(snapshot.size == chain.size);
    CHECK(memcmp(snapshot.data, chain.data, snapshot.size) == 0);
    for (uint32_t tick = 0U; tick < 30U; ++tick) {
        factory_simulation_tick(a);
        factory_simulation_tick(b);
        CHECK(snapshot_equal(a, b));
    }

    factory_snapshot_buffer_destroy(&chain);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world);
}

int main(void)
{
    test_topology_allocation_and_pause();
    test_shortage_and_leftover();
    test_no_implicit_power_and_source_boundary();
    test_zero_power_snapshot_and_duplicate_determinism();
    test_snapshot_continuation();
    if (failures != 0) return 1;
    (void)printf("All power tests passed.\n");
    return 0;
}
