/*
 * Regression coverage for factory_power_allocate_consumer, the reusable
 * quantum-aware allocation step: ascending entity-ID priority across every
 * generator type, continuous generators contributing bounded amounts,
 * atomic (quantum) generators contributing only complete cycles, and a
 * later self-sufficient atomic generator replacing an earlier partial
 * continuous draw rather than stacking on top of it.
 *
 * Tests A-F exercise the algorithm directly, with the exact entity IDs,
 * availabilities, and demand from the specification, independent of any
 * particular consumer's fixed real-world demand value. Test G exercises the
 * full dispatcher (via factory_power_rebuild/factory_power_consume_
 * generation) to confirm that perturbing a generator store's internal
 * physical order (via a demolish that triggers swap-remove) never changes
 * the allocation outcome -- only ascending entity ID does.
 */
#include "foundation/simulation.h"
#include "foundation/snapshot.h"
#include "foundation/steam_turbine.h"
#include "../src/fluid_internal.h"
#include "../src/power_internal.h"
#include "power_fixture.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

#define NETWORK_ID 1U

static FactoryPowerGeneratorInspection make_generator(
    FactoryEntityId entity_id, FactoryPowerNetworkId network_id
)
{
    return (FactoryPowerGeneratorInspection){
        entity_id, 0, 0, 0U, 0U, network_id, true, 0U
    };
}

static FactoryPowerUnits turbine_steam_per_cycle(void)
{
    const FactorySteamTurbineDefinition *d = factory_steam_turbine_definition_get(
        FACTORY_STEAM_TURBINE_DEFINITION_BASIC);
    return d->steam_per_cycle;
}

/* A: ordinary alone covers demand; the turbine never fires. */
static void test_a_ordinary_alone_satisfies_demand(void)
{
    FactoryPowerGeneratorInspection generators[2] = {
        make_generator(10U, NETWORK_ID), make_generator(20U, NETWORK_ID)
    };
    FactoryPowerUnits available[2] = {200U, 200U};
    FactoryPowerUnits quantum[2] = {0U, 200U};
    bool unlocked[2] = {false, false};
    FactoryPowerUnits scratch[2];
    bool powered = factory_power_allocate_consumer(
        generators, available, quantum, unlocked, 2U, NETWORK_ID, 200U, scratch);
    CHECK(powered);
    CHECK(generators[0].committed_output == 200U);
    CHECK(generators[1].committed_output == 0U);
    CHECK(!unlocked[1]);
    CHECK((unlocked[1] ? turbine_steam_per_cycle() : 0U) == 0U);
}

/* B: turbine has the lower ID and alone covers demand; ordinary is untouched. */
static void test_b_turbine_first_by_stable_id(void)
{
    FactoryPowerGeneratorInspection generators[2] = {
        make_generator(10U, NETWORK_ID), make_generator(20U, NETWORK_ID)
    };
    FactoryPowerUnits available[2] = {200U, 200U};
    FactoryPowerUnits quantum[2] = {200U, 0U};
    bool unlocked[2] = {false, false};
    FactoryPowerUnits scratch[2];
    bool powered = factory_power_allocate_consumer(
        generators, available, quantum, unlocked, 2U, NETWORK_ID, 200U, scratch);
    CHECK(powered);
    CHECK(generators[0].committed_output == 200U);
    CHECK(generators[1].committed_output == 0U);
    CHECK(unlocked[0]);
    CHECK((unlocked[0] ? turbine_steam_per_cycle() : 0U) == 100U);
}

/* C: a partial low-ID ordinary contribution must not strand the consumer or
 * get spent once the later turbine alone can cover the complete demand. */
static void test_c_partial_ordinary_must_not_strand_atomic(void)
{
    FactoryPowerGeneratorInspection generators[2] = {
        make_generator(10U, NETWORK_ID), make_generator(20U, NETWORK_ID)
    };
    FactoryPowerUnits available[2] = {100U, 200U};
    FactoryPowerUnits quantum[2] = {0U, 200U};
    bool unlocked[2] = {false, false};
    FactoryPowerUnits scratch[2];
    bool powered = factory_power_allocate_consumer(
        generators, available, quantum, unlocked, 2U, NETWORK_ID, 200U, scratch);
    CHECK(powered);
    CHECK(generators[0].committed_output == 0U);
    CHECK(generators[1].committed_output == 200U);
    CHECK(unlocked[1]);
    CHECK((unlocked[1] ? turbine_steam_per_cycle() : 0U) == 100U);
}

/* D: the turbine alone is insufficient, so the valid combined allocation
 * keeps the ordinary contribution and adds exactly one atomic cycle. */
static void test_d_valid_combined_allocation(void)
{
    FactoryPowerGeneratorInspection generators[2] = {
        make_generator(10U, NETWORK_ID), make_generator(20U, NETWORK_ID)
    };
    FactoryPowerUnits available[2] = {100U, 200U};
    FactoryPowerUnits quantum[2] = {0U, 200U};
    bool unlocked[2] = {false, false};
    FactoryPowerUnits scratch[2];
    bool powered = factory_power_allocate_consumer(
        generators, available, quantum, unlocked, 2U, NETWORK_ID, 300U, scratch);
    CHECK(powered);
    CHECK(generators[0].committed_output == 100U);
    CHECK(generators[1].committed_output == 200U);
    CHECK(unlocked[1]);
    CHECK((unlocked[1] ? turbine_steam_per_cycle() : 0U) == 100U);
}

/* E: no combination reaches the full demand; nothing may be committed. */
static void test_e_no_valid_exact_allocation(void)
{
    FactoryPowerGeneratorInspection generators[2] = {
        make_generator(10U, NETWORK_ID), make_generator(20U, NETWORK_ID)
    };
    FactoryPowerUnits available[2] = {50U, 200U};
    FactoryPowerUnits quantum[2] = {0U, 200U};
    bool unlocked[2] = {false, false};
    FactoryPowerUnits scratch[2];
    bool powered = factory_power_allocate_consumer(
        generators, available, quantum, unlocked, 2U, NETWORK_ID, 300U, scratch);
    CHECK(!powered);
    CHECK(generators[0].committed_output == 0U);
    CHECK(generators[1].committed_output == 0U);
    CHECK(!unlocked[0] && !unlocked[1]);
    CHECK((unlocked[1] ? turbine_steam_per_cycle() : 0U) == 0U);
}

/* F: two atomic sources; the demand needs both cycles, so both fire. */
static void test_f_two_atomic_sources(void)
{
    FactoryPowerGeneratorInspection generators[2] = {
        make_generator(10U, NETWORK_ID), make_generator(20U, NETWORK_ID)
    };
    FactoryPowerUnits available[2] = {200U, 200U};
    FactoryPowerUnits quantum[2] = {200U, 200U};
    bool unlocked[2] = {false, false};
    FactoryPowerUnits scratch[2];
    bool powered = factory_power_allocate_consumer(
        generators, available, quantum, unlocked, 2U, NETWORK_ID, 400U, scratch);
    CHECK(powered);
    CHECK(generators[0].committed_output == 200U);
    CHECK(generators[1].committed_output == 200U);
    CHECK(unlocked[0] && unlocked[1]);
    CHECK(turbine_steam_per_cycle() == 100U);
}

/* G: perturbing a generator store's internal physical order (via a
 * demolish that swap-removes a middle entry) must not change the
 * allocation outcome -- only ascending entity ID may, and both simulations
 * below reach it via the identical command sequence, so their entity IDs,
 * snapshots, and events must match at every tick. */
static void submit(FactorySimulation *s, FactoryCommand command)
{
    if (command.type == FACTORY_COMMAND_PLACE_POWER_GENERATOR)
        s->fixture_initial_generator_fuel = FACTORY_TEST_GENERATOR_FUEL_QUANTITY;
    CHECK(factory_simulation_submit_command(s, &command) == FACTORY_RESULT_OK);
}

static bool snapshot_equal(const FactorySimulation *a, const FactorySimulation *b)
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

static FactorySimulation *build_reordered_store_scenario(FactoryWorld **out_world)
{
    FactoryWorld *world = factory_world_create(9U, 9U);
    FactorySimulation *s =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    size_t i;
    static const int32_t assembler_positions[8][2] = {
        {1, 1}, {2, 1}, {3, 1}, {5, 1}, {6, 1}, {7, 1}, {1, 7}, {2, 7}
    };
    submit(s, (FactoryCommand){                                  /* 1 */
        FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {4, 4}}});
    submit(s, (FactoryCommand){                                  /* 2: genA */
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {4, 6}}});
    submit(s, (FactoryCommand){                                  /* 3: throwaway */
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {0, 6}}});
    submit(s, (FactoryCommand){                                  /* 4: turbine */
        FACTORY_COMMAND_PLACE_STEAM_TURBINE, {.place_steam_turbine = {4, 2}}});
    /* Demolishing the middle generator (id 3, not physically last in the
     * power_generators store) swap-removes it, moving the turbine's generic
     * generator entry into its slot -- reordering the store without
     * changing any surviving entity's ID. */
    submit(s, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {3U}}});
    for (i = 0U; i < 8U; ++i)
        submit(s, (FactoryCommand){
            FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler = {
                assembler_positions[i][0], assembler_positions[i][1],
                FACTORY_DIRECTION_NORTH
            }}});
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    {
        FactoryFluidStorage *storage = factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages, 4U, FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
        CHECK(storage != NULL);
        if (storage != NULL) {
            storage->fluid_type = FACTORY_FLUID_STEAM;
            storage->quantity = 200U;
        }
    }
    *out_world = world;
    return s;
}

/* Test 2: the reorder must not change either generator's committed_output,
 * not just the snapshot as a whole (committed_output is transient and not
 * part of the snapshot format, so this is an independent check). */
static void test_g_source_order_stability(void)
{
    FactoryWorld *world_a;
    FactoryWorld *world_b;
    FactorySimulation *a = build_reordered_store_scenario(&world_a);
    FactorySimulation *b = build_reordered_store_scenario(&world_b);
    uint32_t tick;
    CHECK(snapshot_equal(a, b));
    for (tick = 0U; tick < 10U; ++tick) {
        FactoryPowerGeneratorInspection generator_a;
        FactoryPowerGeneratorInspection generator_b;
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
        CHECK(snapshot_equal(a, b));
        CHECK(factory_simulation_get_event_count(a)
            == factory_simulation_get_event_count(b));
        CHECK(factory_simulation_get_power_generator(a, 2U, &generator_a)
            == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_power_generator(b, 2U, &generator_b)
            == FACTORY_RESULT_OK);
        CHECK(generator_a.committed_output == generator_b.committed_output);
        CHECK(factory_simulation_get_power_generator(a, 4U, &generator_a)
            == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_power_generator(b, 4U, &generator_b)
            == FACTORY_RESULT_OK);
        CHECK(generator_a.committed_output == generator_b.committed_output);
    }
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world_b);
    factory_world_destroy(world_a);
}

/*
 * Tests 1 and 3-6 exercise the full dispatcher end to end and verify real
 * resource consumption, not just the preflight plan: each generator's
 * burner (or the turbine's steam) is checked directly, proving
 * factory_power_consume_generation executed exactly what was committed
 * rather than re-deriving anything from a network aggregate.
 *
 * Burners are "primed" (fuel emptied, released_energy set to a known large
 * value) immediately before the tick under test, so factory_burner_store_
 * begin_tick's natural per-tick release -- which would otherwise make the
 * exact delta unpredictable -- cannot fire (no fuel to (re)ignite). Any
 * change in released_energy across that tick is then attributable solely to
 * factory_power_consume_generation.
 */
static void prime_burner(
    FactorySimulation *s, FactoryEntityId id, FactoryEnergy released_energy
)
{
    FactoryBurner *burner = factory_burner_store_find_mutable(&s->burners, id);
    CHECK(burner != NULL);
    if (burner == NULL) return;
    burner->inventory_item = FACTORY_ITEM_NONE;
    burner->inventory_quantity = 0U;
    burner->current_fuel_item = FACTORY_ITEM_NONE;
    burner->remaining_burn_ticks = 0U;
    burner->released_energy = released_energy;
}

static void prime_accumulator_headroom(
    FactorySimulation *s, FactoryEntityId id, FactoryElectricalEnergy headroom
)
{
    FactoryAccumulator *a =
        factory_accumulator_store_find_mutable(&s->accumulators, id);
    CHECK(a != NULL);
    if (a == NULL) return;
    CHECK(headroom <= FACTORY_ACCUMULATOR_CAPACITY);
    a->stored_energy = FACTORY_ACCUMULATOR_CAPACITY - headroom;
}

static FactoryCommand pole_command(int32_t x, int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {x, y}}
    };
}

static FactoryCommand generator_command(int32_t x, int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {x, y}}
    };
}

static FactoryCommand assembler_command(int32_t x, int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {x, y, FACTORY_DIRECTION_NORTH}}
    };
}

/* 1: two continuous generators (100 each), 150 demand (six 25-unit
 * assemblers) -> ascending-ID priority fills the lower-ID generator (100)
 * before the higher-ID one takes the rest (50). */
static void test_1_two_continuous_generators(void)
{
    FactoryWorld *world = factory_world_create(9U, 9U);
    FactorySimulation *s =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    static const int32_t positions[6][2] = {
        {1, 1}, {2, 1}, {3, 1}, {5, 1}, {6, 1}, {7, 1}
    };
    size_t i;
    FactoryPowerGeneratorInspection generator_a;
    FactoryPowerGeneratorInspection generator_b;
    FactoryPowerNetworkInspection network;
    FactoryBurnerInspection burner;
    submit(s, pole_command(4, 4));                  /* 1 */
    submit(s, generator_command(4, 2));              /* 2: A, avail 100 */
    submit(s, generator_command(4, 6));              /* 3: B, avail 100 */
    for (i = 0U; i < 6U; ++i)
        submit(s, assembler_command(positions[i][0], positions[i][1]));
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    prime_burner(s, 2U, 100000U);
    prime_burner(s, 3U, 100000U);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);

    CHECK(factory_simulation_get_power_generator(s, 2U, &generator_a)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_power_generator(s, 3U, &generator_b)
        == FACTORY_RESULT_OK);
    CHECK(generator_a.committed_output == 100U);
    CHECK(generator_b.committed_output == 50U);
    CHECK(factory_simulation_get_burner(s, 2U, &burner) == FACTORY_RESULT_OK);
    CHECK(burner.released_energy == 99900U);
    CHECK(factory_simulation_get_burner(s, 3U, &burner) == FACTORY_RESULT_OK);
    CHECK(burner.released_energy == 99950U);
    CHECK(factory_simulation_get_power_network(s, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.allocated_power == 150U);
    CHECK(network.total_generation == 200U);
    CHECK(network.unused_generation == 50U);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

/* 3: consumer demand (100, four 25-unit assemblers) fully drains generator
 * A; the accumulator's 50-unit charge is then attributed to generator B,
 * the only one with leftover, not split or drawn from A. */
static void test_3_continuous_plus_accumulator(void)
{
    FactoryWorld *world = factory_world_create(11U, 11U);
    FactorySimulation *s =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    static const int32_t positions[4][2] = {
        {8, 2}, {8, 3}, {8, 4}, {8, 5}
    };
    size_t i;
    FactoryPowerGeneratorInspection generator_a;
    FactoryPowerGeneratorInspection generator_b;
    FactoryPowerNetworkInspection network;
    FactoryAccumulatorInspection accumulator;
    FactoryBurnerInspection burner;
    submit(s, pole_command(5, 5));                   /* 1 */
    submit(s, generator_command(5, 2));               /* 2: A, avail 100 */
    submit(s, generator_command(5, 8));               /* 3: B, avail 100 */
    submit(s, (FactoryCommand){                       /* 4: accumulator */
        FACTORY_COMMAND_PLACE_ACCUMULATOR, {.place_accumulator = {2, 5}}});
    for (i = 0U; i < 4U; ++i)
        submit(s, assembler_command(positions[i][0], positions[i][1]));
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    prime_burner(s, 2U, 100000U);
    prime_burner(s, 3U, 100000U);
    prime_accumulator_headroom(s, 4U, 50U);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);

    CHECK(factory_simulation_get_power_generator(s, 2U, &generator_a)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_power_generator(s, 3U, &generator_b)
        == FACTORY_RESULT_OK);
    CHECK(generator_a.committed_output == 100U);
    CHECK(generator_b.committed_output == 50U);
    CHECK(factory_simulation_get_burner(s, 2U, &burner) == FACTORY_RESULT_OK);
    CHECK(burner.released_energy == 99900U);
    CHECK(factory_simulation_get_burner(s, 3U, &burner) == FACTORY_RESULT_OK);
    CHECK(burner.released_energy == 99950U);
    CHECK(factory_simulation_get_accumulator(s, 4U, &accumulator)
        == FACTORY_RESULT_OK);
    CHECK(accumulator.stored_energy == FACTORY_ACCUMULATOR_CAPACITY);
    CHECK(factory_simulation_get_power_network(s, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.allocated_power == 100U);
    CHECK(network.accumulator_charge == 50U);
    CHECK(network.unused_generation == 50U);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

/* 4: a single turbine, 100 demand (four 25-unit assemblers), no
 * accumulator -- the one complete 200-unit cycle fires (ascending-ID
 * replacement trivially applies to each 25-unit consumer in turn), only
 * 100 is claimed, and the other 100 is unused. */
static void test_4_atomic_partial_use(void)
{
    FactoryWorld *world = factory_world_create(11U, 11U);
    FactorySimulation *s =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    static const int32_t positions[4][2] = {
        {8, 2}, {8, 3}, {8, 4}, {8, 5}
    };
    size_t i;
    FactoryPowerGeneratorInspection generator;
    FactoryPowerNetworkInspection network;
    FactorySteamTurbineInspection turbine;
    submit(s, pole_command(5, 5));                   /* 1 */
    submit(s, (FactoryCommand){                       /* 2: turbine */
        FACTORY_COMMAND_PLACE_STEAM_TURBINE, {.place_steam_turbine = {5, 2}}});
    for (i = 0U; i < 4U; ++i)
        submit(s, assembler_command(positions[i][0], positions[i][1]));
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    {
        FactoryFluidStorage *storage = factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages, 2U, FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
        CHECK(storage != NULL);
        if (storage != NULL) {
            storage->fluid_type = FACTORY_FLUID_STEAM;
            storage->quantity = 200U;
        }
    }
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);

    CHECK(factory_simulation_get_power_generator(s, 2U, &generator)
        == FACTORY_RESULT_OK);
    CHECK(generator.committed_output == 100U);
    CHECK(factory_simulation_get_steam_turbine(s, 2U, &turbine)
        == FACTORY_RESULT_OK);
    CHECK(turbine.actual_output == 200U);
    CHECK(turbine.steam_consumed_last_tick == 100U);
    CHECK(turbine.completed_cycles_last_tick == 1U);
    CHECK(factory_simulation_get_power_network(s, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.allocated_power == 100U);
    CHECK(network.unused_generation == 100U);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

/* 5: same turbine and 100 demand as test 4, plus an accumulator that
 * accepts 50 -- the fired cycle's leftover (100) covers the charge, no
 * second cycle is needed (there isn't one available), and 50 still goes
 * unused. */
static void test_5_atomic_partial_use_plus_accumulator(void)
{
    FactoryWorld *world = factory_world_create(11U, 11U);
    FactorySimulation *s =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    static const int32_t positions[4][2] = {
        {8, 2}, {8, 3}, {8, 4}, {8, 5}
    };
    size_t i;
    FactoryPowerGeneratorInspection generator;
    FactoryPowerNetworkInspection network;
    FactorySteamTurbineInspection turbine;
    FactoryAccumulatorInspection accumulator;
    submit(s, pole_command(5, 5));                   /* 1 */
    submit(s, (FactoryCommand){                       /* 2: turbine */
        FACTORY_COMMAND_PLACE_STEAM_TURBINE, {.place_steam_turbine = {5, 2}}});
    submit(s, (FactoryCommand){                       /* 3: accumulator */
        FACTORY_COMMAND_PLACE_ACCUMULATOR, {.place_accumulator = {2, 5}}});
    for (i = 0U; i < 4U; ++i)
        submit(s, assembler_command(positions[i][0], positions[i][1]));
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    {
        FactoryFluidStorage *storage = factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages, 2U, FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
        CHECK(storage != NULL);
        if (storage != NULL) {
            storage->fluid_type = FACTORY_FLUID_STEAM;
            storage->quantity = 200U;
        }
    }
    prime_accumulator_headroom(s, 3U, 50U);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);

    CHECK(factory_simulation_get_power_generator(s, 2U, &generator)
        == FACTORY_RESULT_OK);
    CHECK(generator.committed_output == 150U);
    CHECK(factory_simulation_get_steam_turbine(s, 2U, &turbine)
        == FACTORY_RESULT_OK);
    CHECK(turbine.actual_output == 200U);
    CHECK(turbine.steam_consumed_last_tick == 100U);
    CHECK(factory_simulation_get_accumulator(s, 3U, &accumulator)
        == FACTORY_RESULT_OK);
    CHECK(accumulator.stored_energy == FACTORY_ACCUMULATOR_CAPACITY);
    CHECK(factory_simulation_get_power_network(s, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.allocated_power == 100U);
    CHECK(network.accumulator_charge == 50U);
    CHECK(network.unused_generation == 50U);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

/* 6: one continuous generator and one turbine, 200 demand (eight 25-unit
 * assemblers) -- the continuous generator's actual burner draw and the
 * turbine's actual steam draw must each exactly match what was planned. */
static void test_6_mixed_continuous_and_atomic(void)
{
    FactoryWorld *world = factory_world_create(11U, 11U);
    FactorySimulation *s =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    static const int32_t positions[8][2] = {
        {8, 2}, {8, 3}, {8, 4}, {8, 5}, {8, 6}, {8, 7}, {8, 8}, {7, 8}
    };
    size_t i;
    FactoryPowerGeneratorInspection generator_a;
    FactoryPowerGeneratorInspection generator_turbine;
    FactoryPowerNetworkInspection network;
    FactorySteamTurbineInspection turbine;
    FactoryBurnerInspection burner;
    submit(s, pole_command(5, 5));                   /* 1 */
    submit(s, generator_command(5, 2));               /* 2: A, avail 100 */
    submit(s, (FactoryCommand){                       /* 3: turbine */
        FACTORY_COMMAND_PLACE_STEAM_TURBINE, {.place_steam_turbine = {2, 5}}});
    for (i = 0U; i < 8U; ++i)
        submit(s, assembler_command(positions[i][0], positions[i][1]));
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    prime_burner(s, 2U, 100000U);
    {
        FactoryFluidStorage *storage = factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages, 3U, FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
        CHECK(storage != NULL);
        if (storage != NULL) {
            storage->fluid_type = FACTORY_FLUID_STEAM;
            storage->quantity = 200U;
        }
    }
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);

    CHECK(factory_simulation_get_power_generator(s, 2U, &generator_a)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_power_generator(s, 3U, &generator_turbine)
        == FACTORY_RESULT_OK);
    CHECK(generator_a.committed_output == 100U);
    CHECK(generator_turbine.committed_output == 100U);
    CHECK(factory_simulation_get_burner(s, 2U, &burner) == FACTORY_RESULT_OK);
    CHECK(burner.released_energy == 99900U);
    CHECK(factory_simulation_get_steam_turbine(s, 3U, &turbine)
        == FACTORY_RESULT_OK);
    CHECK(turbine.actual_output == 200U);
    CHECK(turbine.steam_consumed_last_tick == 100U);
    CHECK(factory_simulation_get_power_network(s, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.allocated_power == 200U);
    CHECK(network.total_generation == 300U);
    CHECK(network.unused_generation == 100U);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

int main(void)
{
    test_a_ordinary_alone_satisfies_demand();
    test_b_turbine_first_by_stable_id();
    test_c_partial_ordinary_must_not_strand_atomic();
    test_d_valid_combined_allocation();
    test_e_no_valid_exact_allocation();
    test_f_two_atomic_sources();
    test_1_two_continuous_generators();
    test_g_source_order_stability();
    test_3_continuous_plus_accumulator();
    test_4_atomic_partial_use();
    test_5_atomic_partial_use_plus_accumulator();
    test_6_mixed_continuous_and_atomic();
    if (failures != 0) return 1;
    (void)printf("All power allocation tests passed.\n");
    return 0;
}
