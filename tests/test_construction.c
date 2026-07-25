#include "foundation/simulation.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static FactoryCommand grant(uint32_t amount)
{
    FactoryCommand command = {
        FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS,
        {.grant_construction_units = {amount}}
    };
    return command;
}

static FactoryCommand placement(FactoryEntityType type, int32_t x, int32_t y)
{
    switch (type) {
        case FACTORY_ENTITY_TYPE_EXTRACTOR:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
                {.place_extractor = {x, y, FACTORY_DIRECTION_EAST}}};
        case FACTORY_ENTITY_TYPE_BELT:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_BELT,
                {.place_belt = {x, y, FACTORY_DIRECTION_EAST}}};
        case FACTORY_ENTITY_TYPE_STORAGE:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
                {.place_storage = {x, y}}};
        case FACTORY_ENTITY_TYPE_REFINERY:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_REFINERY,
                {.place_refinery = {
                    x, y, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
                }}};
        case FACTORY_ENTITY_TYPE_ASSEMBLER:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
                {.place_assembler = {x, y, FACTORY_DIRECTION_EAST}}};
        case FACTORY_ENTITY_TYPE_SPLITTER:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_SPLITTER,
                {.place_splitter = {x, y, FACTORY_DIRECTION_EAST}}};
        case FACTORY_ENTITY_TYPE_INSERTER:
            return (FactoryCommand){FACTORY_COMMAND_PLACE_INSERTER,
                {.place_inserter = {x, y, FACTORY_DIRECTION_EAST}}};
        case FACTORY_ENTITY_TYPE_NONE:
        default:
            return grant(0U);
    }
}

static void test_cost_lookup(void)
{
    static const struct {
        FactoryEntityType type;
        uint32_t expected;
    } cases[] = {
        {FACTORY_ENTITY_TYPE_EXTRACTOR, FACTORY_CONSTRUCTION_COST_EXTRACTOR},
        {FACTORY_ENTITY_TYPE_BELT, FACTORY_CONSTRUCTION_COST_BELT},
        {FACTORY_ENTITY_TYPE_STORAGE, FACTORY_CONSTRUCTION_COST_STORAGE},
        {FACTORY_ENTITY_TYPE_REFINERY, FACTORY_CONSTRUCTION_COST_REFINERY},
        {FACTORY_ENTITY_TYPE_ASSEMBLER, FACTORY_CONSTRUCTION_COST_ASSEMBLER},
        {FACTORY_ENTITY_TYPE_SPLITTER, FACTORY_CONSTRUCTION_COST_SPLITTER},
        {FACTORY_ENTITY_TYPE_INSERTER, FACTORY_CONSTRUCTION_COST_INSERTER}
    };

    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint32_t first = 0U;
        uint32_t second = 0U;

        CHECK(factory_entity_construction_cost(cases[index].type, &first));
        CHECK(factory_entity_construction_cost(cases[index].type, &second));
        CHECK(first == cases[index].expected);
        CHECK(second == first);
    }
    {
        uint32_t cost = 123U;

        CHECK(!factory_entity_construction_cost(
            FACTORY_ENTITY_TYPE_NONE, &cost
        ));
        CHECK(cost == 123U);
        CHECK(!factory_entity_construction_cost(
            (FactoryEntityType)999, &cost
        ));
        CHECK(!factory_entity_construction_cost(
            FACTORY_ENTITY_TYPE_BELT, NULL
        ));
    }
}

static void test_grants_and_overflow(void)
{
    FactoryWorld *world = factory_world_create(1U, 1U);
    FactorySimulation *simulation = factory_simulation_create(world);

    CHECK(factory_simulation_construction_units(simulation) == 0U);
    submit(simulation, grant(10U));
    CHECK(factory_simulation_construction_units(simulation) == 0U);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_construction_units(simulation) == 10U);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(
        simulation, 0U
    )->construction_units_remaining == 10U);

    submit(simulation, grant(0U));
    submit(simulation, grant(5U));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_construction_units(simulation) == 15U);
    factory_simulation_destroy(simulation);

    simulation = factory_simulation_create_with_construction_units(
        world, UINT32_MAX - 2U
    );
    submit(simulation, grant(3U));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_CONSTRUCTION_INVENTORY_OVERFLOW);
    CHECK(factory_simulation_construction_units(simulation)
        == UINT32_MAX - 2U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_every_entity_placement_and_refund(void)
{
    static const FactoryEntityType types[] = {
        FACTORY_ENTITY_TYPE_EXTRACTOR,
        FACTORY_ENTITY_TYPE_BELT,
        FACTORY_ENTITY_TYPE_STORAGE,
        FACTORY_ENTITY_TYPE_REFINERY,
        FACTORY_ENTITY_TYPE_ASSEMBLER,
        FACTORY_ENTITY_TYPE_SPLITTER,
        FACTORY_ENTITY_TYPE_INSERTER
    };

    for (size_t index = 0U; index < sizeof(types) / sizeof(types[0]); ++index) {
        FactoryWorld *world = factory_world_create(2U, 1U);
        FactorySimulation *simulation;
        uint32_t cost = 0U;

        CHECK(factory_entity_construction_cost(types[index], &cost));
        if (types[index] == FACTORY_ENTITY_TYPE_EXTRACTOR) {
            CHECK(factory_world_add_resource(
                world, 0, 0, FACTORY_RESOURCE_IRON, 1U
            ) == FACTORY_RESULT_OK);
        }
        simulation = factory_simulation_create_with_construction_units(
            world, cost
        );
        submit(simulation, placement(types[index], 0, 0));
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(
            simulation, 0U
        )->result == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_command_result(
            simulation, 0U
        )->entity_type == types[index]);
        CHECK(factory_simulation_get_command_result(
            simulation, 0U
        )->construction_units_changed == cost);
        CHECK(factory_simulation_construction_units(simulation) == 0U);
        CHECK(factory_simulation_get_entity_count(simulation) == 1U);
        CHECK(factory_world_get_tile(world, 0, 0)->occupying_entity == 1U);

        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_DEMOLISH_ENTITY,
            {.demolish_entity = {1U}}
        });
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(
            simulation, 0U
        )->result == FACTORY_RESULT_OK);
        CHECK(factory_simulation_construction_units(simulation) == cost);
        CHECK(factory_simulation_get_entity_count(simulation) == 0U);
        CHECK(factory_world_get_tile(world, 0, 0)->occupying_entity == 0U);

        submit(simulation, (FactoryCommand){
            FACTORY_COMMAND_DEMOLISH_ENTITY,
            {.demolish_entity = {1U}}
        });
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(
            simulation, 0U
        )->result == FACTORY_RESULT_ENTITY_NOT_FOUND);
        CHECK(factory_simulation_construction_units(simulation) == cost);

        factory_simulation_destroy(simulation);
        factory_world_destroy(world);
    }
}

static void test_insufficient_and_failed_placement_atomicity(void)
{
    static const FactoryEntityType types[] = {
        FACTORY_ENTITY_TYPE_EXTRACTOR,
        FACTORY_ENTITY_TYPE_BELT,
        FACTORY_ENTITY_TYPE_STORAGE,
        FACTORY_ENTITY_TYPE_REFINERY,
        FACTORY_ENTITY_TYPE_ASSEMBLER,
        FACTORY_ENTITY_TYPE_SPLITTER,
        FACTORY_ENTITY_TYPE_INSERTER
    };

    for (size_t index = 0U; index < sizeof(types) / sizeof(types[0]); ++index) {
        FactoryWorld *world = factory_world_create(2U, 1U);
        FactorySimulation *simulation;
        uint32_t cost = 0U;

        CHECK(factory_entity_construction_cost(types[index], &cost));
        if (types[index] == FACTORY_ENTITY_TYPE_EXTRACTOR) {
            CHECK(factory_world_add_resource(
                world, 0, 0, FACTORY_RESOURCE_IRON, 1U
            ) == FACTORY_RESULT_OK);
        }
        simulation = factory_simulation_create_with_construction_units(
            world, cost - 1U
        );
        submit(simulation, placement(types[index], 0, 0));
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(
            simulation, 0U
        )->result == FACTORY_RESULT_INSUFFICIENT_CONSTRUCTION_UNITS);
        CHECK(factory_simulation_construction_units(simulation) == cost - 1U);
        CHECK(factory_simulation_get_entity_count(simulation) == 0U);
        CHECK(factory_world_get_tile(world, 0, 0)->occupying_entity == 0U);

        submit(simulation, grant(1U));
        submit(simulation, placement(types[index], 0, 0));
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(
            simulation, 1U
        )->result == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_command_result(
            simulation, 1U
        )->entity_id == 1U);
        factory_simulation_destroy(simulation);
        factory_world_destroy(world);
    }

    {
        FactoryWorld *world = factory_world_create(2U, 1U);
        FactorySimulation *simulation =
            factory_simulation_create_with_construction_units(world, 10U);

        submit(simulation, placement(FACTORY_ENTITY_TYPE_BELT, 0, 0));
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_construction_units(simulation) == 9U);
        submit(simulation, placement(FACTORY_ENTITY_TYPE_BELT, 0, 0));
        submit(simulation, placement(FACTORY_ENTITY_TYPE_BELT, -1, 0));
        submit(simulation, placement(FACTORY_ENTITY_TYPE_BELT, 1, 0));
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_command_result(
            simulation, 0U
        )->result == FACTORY_RESULT_TILE_OCCUPIED);
        CHECK(factory_simulation_get_command_result(
            simulation, 1U
        )->result == FACTORY_RESULT_OUT_OF_BOUNDS);
        CHECK(factory_simulation_get_command_result(
            simulation, 2U
        )->result == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_command_result(
            simulation, 2U
        )->entity_id == 2U);
        CHECK(factory_simulation_construction_units(simulation) == 8U);
        CHECK(factory_simulation_get_entity_count(simulation) == 2U);
        factory_simulation_destroy(simulation);
        factory_world_destroy(world);
    }
}

static void test_fifo_replacement_failures_and_refund_overflow(void)
{
    FactoryWorld *world = factory_world_create(2U, 1U);
    FactorySimulation *simulation = factory_simulation_create(world);

    submit(simulation, grant(1U));
    submit(simulation, placement(FACTORY_ENTITY_TYPE_BELT, 0, 0));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_construction_units(simulation) == 0U);

    submit(simulation, placement(FACTORY_ENTITY_TYPE_BELT, 1, 0));
    submit(simulation, grant(1U));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_INSUFFICIENT_CONSTRUCTION_UNITS);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_construction_units(simulation) == 1U);

    submit(simulation, (FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity = {1U}}});
    submit(simulation, placement(FACTORY_ENTITY_TYPE_BELT, 0, 0));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->entity_id
        == 2U);
    CHECK(factory_simulation_construction_units(simulation) == 1U);

    simulation->belts.items[0].item = FACTORY_ITEM_IRON_ORE;
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity = {2U}}});
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    CHECK(factory_simulation_construction_units(simulation) == 1U);
    CHECK(factory_simulation_entity_is_valid(simulation, 2U));

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);

    world = factory_world_create(1U, 1U);
    simulation = factory_simulation_create_with_construction_units(world, 1U);
    submit(simulation, placement(FACTORY_ENTITY_TYPE_BELT, 0, 0));
    factory_simulation_tick(simulation);
    submit(simulation, grant(UINT32_MAX));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_construction_units(simulation) == UINT32_MAX);
    submit(simulation, (FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity = {1U}}});
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_CONSTRUCTION_INVENTORY_OVERFLOW);
    CHECK(factory_simulation_construction_units(simulation) == UINT32_MAX);
    CHECK(factory_simulation_entity_is_valid(simulation, 1U));
    CHECK(factory_world_get_tile(world, 0, 0)->occupying_entity == 1U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_construction_determinism_and_accounting(void)
{
    FactoryWorld *worlds[2];
    FactorySimulation *simulations[2];
    uint32_t expected = 0U;

    for (size_t run = 0U; run < 2U; ++run) {
        worlds[run] = factory_world_create(3U, 1U);
        simulations[run] = factory_simulation_create(worlds[run]);
        submit(simulations[run], grant(10U));
        submit(simulations[run], placement(
            FACTORY_ENTITY_TYPE_BELT, 0, 0
        ));
        submit(simulations[run], placement(
            FACTORY_ENTITY_TYPE_ASSEMBLER, 1, 0
        ));
    }
    expected += 10U;
    expected -= FACTORY_CONSTRUCTION_COST_BELT;
    for (size_t run = 0U; run < 2U; ++run) {
        factory_simulation_tick(simulations[run]);
        CHECK(factory_simulation_construction_units(
            simulations[run]
        ) == expected);
        CHECK(factory_simulation_get_entity_count(simulations[run]) == 1U);
        CHECK(factory_simulation_get_command_result(
            simulations[run], 2U
        )->result == FACTORY_RESULT_INSUFFICIENT_CONSTRUCTION_UNITS);
        submit(simulations[run], (FactoryCommand){
            FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {1U}}
        });
        submit(simulations[run], placement(
            FACTORY_ENTITY_TYPE_SPLITTER, 0, 0
        ));
    }
    expected += FACTORY_CONSTRUCTION_COST_BELT;
    expected -= FACTORY_CONSTRUCTION_COST_SPLITTER;
    for (size_t run = 0U; run < 2U; ++run) {
        factory_simulation_tick(simulations[run]);
        CHECK(factory_simulation_construction_units(
            simulations[run]
        ) == expected);
        CHECK(factory_simulation_get_command_result(
            simulations[run], 0U
        )->result == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_command_result(
            simulations[run], 1U
        )->entity_id == 2U);
    }
    CHECK(factory_simulation_get_tick(simulations[0])
        == factory_simulation_get_tick(simulations[1]));
    CHECK(factory_simulation_construction_units(simulations[0])
        == factory_simulation_construction_units(simulations[1]));
    CHECK(factory_world_get_tile(worlds[0], 0, 0)->occupying_entity
        == factory_world_get_tile(worlds[1], 0, 0)->occupying_entity);
    for (size_t run = 0U; run < 2U; ++run) {
        factory_simulation_destroy(simulations[run]);
        factory_world_destroy(worlds[run]);
    }
}

int main(void)
{
    test_cost_lookup();
    test_grants_and_overflow();
    test_every_entity_placement_and_refund();
    test_insufficient_and_failed_placement_atomicity();
    test_fifo_replacement_failures_and_refund_overflow();
    test_construction_determinism_and_accounting();

    if (failures != 0) {
        (void)fprintf(stderr, "%d construction test(s) failed\n", failures);
        return 1;
    }
    (void)printf("construction economy tests passed\n");
    return 0;
}
