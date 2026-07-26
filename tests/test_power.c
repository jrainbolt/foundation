#include "foundation/snapshot.h"
#include "foundation/steam_turbine.h"
#include "../src/fluid_internal.h"
#include "../src/power_internal.h"
#include "power_fixture.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    if (command.type == FACTORY_COMMAND_PLACE_POWER_GENERATOR)
        simulation->fixture_initial_generator_fuel =
            FACTORY_TEST_GENERATOR_FUEL_QUANTITY;
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

static void empty_fixture_burner(
    FactorySimulation *simulation, FactoryEntityId id
)
{
    FactoryBurner *burner =
        factory_burner_store_find_mutable(&simulation->burners, id);
    CHECK(burner != NULL);
    if (burner != NULL) {
        burner->inventory_item = FACTORY_ITEM_NONE;
        burner->inventory_quantity = 0U;
        burner->current_fuel_item = FACTORY_ITEM_NONE;
        burner->remaining_burn_ticks = 0U;
        burner->released_energy = 0U;
    }
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

    empty_fixture_burner(simulation, 3U);
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
    CHECK(snapshot.data[8] == FACTORY_SNAPSHOT_VERSION);
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

static void test_multiple_generator_actual_energy_consumption(void)
{
    FactoryWorld *world = factory_world_create(8U, 5U);
    FactorySimulation *a =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactorySimulation *b = NULL;
    FactorySnapshotBuffer snapshot = {0};
    FactoryPowerNetworkInspection network;
    FactoryBurnerInspection first;
    FactoryBurnerInspection second;

    submit(a, pole(3, 2));                                  /* 1 */
    submit(a, generator(2, 3));                             /* 2 */
    submit(a, generator(4, 3));                             /* 3 */
    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {2, 1, FACTORY_DIRECTION_NORTH}}
    });                                                     /* 4 */
    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {4, 1, FACTORY_DIRECTION_NORTH}}
    });                                                     /* 5 */
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_power_network(a, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.allocated_power == 50U);
    CHECK(factory_simulation_get_burner(a, 2U, &first)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_burner(a, 3U, &second)
        == FACTORY_RESULT_OK);
    CHECK(first.released_energy == 50U);
    CHECK(second.released_energy == 100U);

    submit(a, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {1U}}
    });
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_burner(a, 2U, &first)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_burner(a, 3U, &second)
        == FACTORY_RESULT_OK);
    CHECK(first.released_energy == 150U);
    CHECK(second.released_energy == 200U);

    submit(a, pole(3, 2));                                  /* 6 */
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_burner(a, 2U, &first)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_burner(a, 3U, &second)
        == FACTORY_RESULT_OK);
    CHECK(first.released_energy == 200U);
    CHECK(second.released_energy == 300U);

    CHECK(factory_simulation_create_snapshot(a, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &b) == FACTORY_RESULT_OK);
    for (uint32_t tick = 0U; tick < 20U; ++tick) {
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
        CHECK(snapshot_equal(a, b));
        CHECK(factory_simulation_get_burner(a, 2U, &first)
            == FACTORY_RESULT_OK);
        {
            FactoryBurnerInspection loaded;
            CHECK(factory_simulation_get_burner(b, 2U, &loaded)
                == FACTORY_RESULT_OK);
            CHECK(first.released_energy == loaded.released_energy);
            CHECK(first.unreleased_fuel_energy
                == loaded.unreleased_fuel_energy);
        }
    }

    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world);
}

static FactorySimulation *build_mixed_generator_turbine(
    FactoryWorld **out_world, bool turbine_first,
    FactoryEntityId *out_turbine_id
)
{
    static const int32_t assembler_positions[8][2] = {
        {1, 1}, {2, 1}, {3, 1}, {5, 1}, {6, 1}, {7, 1}, {1, 7}, {2, 7}
    };
    FactoryWorld *world = factory_world_create(9U, 9U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    size_t i;
    FactoryCommand turbine_command = {
        FACTORY_COMMAND_PLACE_STEAM_TURBINE, {.place_steam_turbine = {4, 2}}
    };
    submit(simulation, pole(4, 4));
    if (turbine_first) {
        submit(simulation, turbine_command);
        submit(simulation, generator(4, 6));
    } else {
        submit(simulation, generator(4, 6));
        submit(simulation, turbine_command);
    }
    for (i = 0U; i < 8U; ++i)
        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler = {
                assembler_positions[i][0], assembler_positions[i][1],
                FACTORY_DIRECTION_NORTH
            }}
        });
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    {
        FactoryEntityId turbine_id = turbine_first ? 2U : 3U;
        FactoryFluidStorage *storage =
            factory_fluid_storage_store_find_slot_mutable(
                &simulation->fluid_storages, turbine_id,
                FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
        CHECK(storage != NULL);
        if (storage != NULL) {
            storage->fluid_type = FACTORY_FLUID_STEAM;
            storage->quantity = 200U;
        }
        *out_turbine_id = turbine_id;
    }
    *out_world = world;
    return simulation;
}

/*
 * Regression for the mixed-generator quantum bug: a network with one basic
 * generator (100, divisible) and one steam turbine (200, atomic) covering
 * exactly 200 of consumer demand must dispatch the full turbine quantum
 * rather than conservatively stopping at the divisible generator's 100 and
 * discarding the turbine's otherwise-usable cycle. The check runs with the
 * turbine on both sides of the generator's entity ID, since the dispatcher
 * and the real per-generator consumption pass must agree regardless of
 * placement order.
 */
static void check_mixed_generator_turbine_quantum(bool turbine_first)
{
    FactoryWorld *world;
    FactorySimulation *simulation;
    FactoryEntityId turbine_id;
    FactoryPowerNetworkInspection network;
    FactorySteamTurbineInspection turbine;

    simulation = build_mixed_generator_turbine(&world, turbine_first, &turbine_id);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_power_network(simulation, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.total_demand == 200U);
    /*
     * The basic generator's 100 continuous capacity is always "on" and
     * serves the four lowest-ID assemblers first (ascending entity-ID
     * priority); the turbine then fires its full 200-unit cycle for the
     * rest. total_generation reports raw realized capacity (100 continuous
     * + the turbine's fired 200), of which only 200 is actually claimed by
     * consumers -- the turbine's other 100 sits unused with no accumulator
     * to catch it.
     */
    CHECK(network.total_generation == 300U);
    CHECK(network.allocated_power == 200U);
    CHECK(network.unused_generation == 100U);
    CHECK(network.powered_consumer_count == 8U);
    CHECK(network.unpowered_consumer_count == 0U);
    CHECK(factory_simulation_get_steam_turbine(simulation, turbine_id, &turbine)
        == FACTORY_RESULT_OK);
    CHECK(turbine.actual_output == 200U
        && turbine.steam_consumed_last_tick == 100U
        && turbine.completed_cycles_last_tick == 1U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_mixed_generator_turbine_quantum(void)
{
    check_mixed_generator_turbine_quantum(true);
    check_mixed_generator_turbine_quantum(false);
}

static void test_mixed_generator_turbine_determinism(void)
{
    FactoryWorld *world_a;
    FactoryWorld *world_b;
    FactorySimulation *a;
    FactorySimulation *b;
    FactoryEntityId turbine_id_a;
    FactoryEntityId turbine_id_b;
    uint32_t tick;

    a = build_mixed_generator_turbine(&world_a, false, &turbine_id_a);
    b = build_mixed_generator_turbine(&world_b, false, &turbine_id_b);
    CHECK(turbine_id_a == turbine_id_b);
    for (tick = 0U; tick < 10U; ++tick) {
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
        CHECK(snapshot_equal(a, b));
    }

    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world_b);
    factory_world_destroy(world_a);
}

int main(void)
{
    test_topology_allocation_and_pause();
    test_shortage_and_leftover();
    test_no_implicit_power_and_source_boundary();
    test_zero_power_snapshot_and_duplicate_determinism();
    test_snapshot_continuation();
    test_multiple_generator_actual_energy_consumption();
    test_mixed_generator_turbine_quantum();
    test_mixed_generator_turbine_determinism();
    if (failures != 0) return 1;
    (void)printf("All power tests passed.\n");
    return 0;
}
