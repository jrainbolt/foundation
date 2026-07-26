#include "foundation/simulation.h"
#include "power_fixture.h"

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

static FactoryCommand inserter(
    int32_t x,
    int32_t y,
    FactoryDirection facing
)
{
    FactoryCommand command = {
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {x, y, facing}}
    };
    return command;
}

static uint32_t accounted_iron(
    FactorySimulation *simulation,
    FactoryWorld *world,
    int32_t deposit_x,
    int32_t deposit_y,
    FactoryEntityId max_id
)
{
    const FactoryTile *deposit = factory_world_get_tile(
        world, deposit_x, deposit_y
    );
    uint32_t total = deposit == NULL ? 0U : deposit->resource_amount;
    FactoryEntityId id;

    for (id = 1U; id <= max_id; ++id) {
        FactoryExtractor extractor_state;
        FactoryBelt belt_state;
        FactorySplitter splitter_state;
        FactoryInserter inserter_state;
        FactoryRefinery refinery_state;
        FactoryAssembler assembler_state;
        FactoryStorage storage_state;

        if (factory_simulation_get_extractor(
                simulation, id, &extractor_state)) {
            total += extractor_state.output_amount;
        } else if (factory_simulation_get_belt(
                simulation, id, &belt_state)) {
            total += belt_state.item == FACTORY_ITEM_NONE ? 0U : 1U;
        } else if (factory_simulation_get_splitter(
                simulation, id, &splitter_state)) {
            total += splitter_state.item == FACTORY_ITEM_NONE ? 0U : 1U;
        } else if (factory_simulation_get_inserter(
                simulation, id, &inserter_state)) {
            if (inserter_state.held_item == FACTORY_ITEM_ELECTRONIC_COMPONENT) {
                ++total;
            } else if (inserter_state.held_item == FACTORY_ITEM_IRON_ORE
                || inserter_state.held_item == FACTORY_ITEM_IRON_PLATE) {
                ++total;
            }
        } else if (factory_simulation_get_refinery(
                simulation, id, &refinery_state)) {
            total += refinery_state.input_amount;
            total += refinery_state.processing ? 1U : 0U;
            total += refinery_state.output_amount;
        } else if (factory_simulation_get_assembler(
                simulation, id, &assembler_state)) {
            total += assembler_state.input_slots[0].count;
            total += assembler_state.processing ? 1U : 0U;
            total += assembler_state.output_amount;
        } else if (factory_simulation_get_storage(
                simulation, id, &storage_state)) {
            total += storage_state.iron_ore_amount;
            total += storage_state.iron_plate_amount;
            total += storage_state.electronic_component_amount;
        }
    }
    return total;
}

static uint32_t item_element_amount(
    FactoryItemType item,
    bool copper
)
{
    if (item == FACTORY_ITEM_ELECTRONIC_COMPONENT) {
        return 1U;
    }
    if (copper) {
        return item == FACTORY_ITEM_COPPER_ORE
            || item == FACTORY_ITEM_COPPER_PLATE;
    }
    return item == FACTORY_ITEM_IRON_ORE
        || item == FACTORY_ITEM_IRON_PLATE;
}

static uint32_t accounted_element(
    FactorySimulation *simulation,
    FactoryWorld *world,
    FactoryEntityId max_id,
    bool copper
)
{
    uint32_t total = factory_world_get_tile(
        world, copper ? 0 : 0, copper ? 4 : 0
    )->resource_amount;

    for (FactoryEntityId id = 1U; id <= max_id; ++id) {
        FactoryExtractor extractor;
        FactoryBelt belt;
        FactoryInserter inserter_state;
        FactoryRefinery refinery;
        FactoryAssembler assembler;
        FactoryStorage storage;

        if (factory_simulation_get_extractor(
                simulation, id, &extractor)) {
            total += extractor.output_amount
                * item_element_amount(extractor.output_item, copper);
        } else if (factory_simulation_get_belt(simulation, id, &belt)) {
            total += item_element_amount(belt.item, copper);
        } else if (factory_simulation_get_inserter(
                simulation, id, &inserter_state)) {
            total += inserter_state.held_amount
                * item_element_amount(inserter_state.held_item, copper);
        } else if (factory_simulation_get_refinery(
                simulation, id, &refinery)) {
            total += refinery.input_amount
                * item_element_amount(refinery.input_item, copper);
            total += refinery.output_amount
                * item_element_amount(refinery.output_item, copper);
            if (refinery.processing) {
                const FactoryRecipe *recipe = factory_recipe_get(
                    refinery.recipe_id
                );

                total += recipe == NULL
                    ? 0U
                    : item_element_amount(recipe->input_item, copper);
            }
        } else if (factory_simulation_get_assembler(
                simulation, id, &assembler)) {
            total += copper
                ? assembler.input_slots[1].count
                : assembler.input_slots[0].count;
            if (assembler.processing) {
                ++total;
            }
            total += assembler.output_amount
                * item_element_amount(assembler.output_item, copper);
        } else if (factory_simulation_get_storage(
                simulation, id, &storage)) {
            total += copper
                ? storage.copper_ore_amount + storage.copper_plate_amount
                    + storage.electronic_component_amount
                : storage.iron_ore_amount + storage.iron_plate_amount
                    + storage.electronic_component_amount;
        }
    }
    return total;
}

static void test_placement_orientation_and_invalid_sources(void)
{
    FactoryWorld *world = factory_world_create(4U, 5U);
    FactorySimulation *simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryInserter state;
    FactoryCommand invalid = inserter(0, 0, (FactoryDirection)99);

    CHECK(factory_simulation_submit_command(simulation, &invalid)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    submit(simulation, inserter(1, 1, FACTORY_DIRECTION_EAST));
    submit(simulation, inserter(1, 1, FACTORY_DIRECTION_NORTH));
    submit(simulation, inserter(-1, 0, FACTORY_DIRECTION_NORTH));
    CHECK(factory_test_submit_power_row(simulation, 4U, 3U));
    factory_simulation_tick(simulation);

    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(simulation, 1U)->result
        == FACTORY_RESULT_TILE_OCCUPIED);
    CHECK(factory_simulation_get_command_result(simulation, 2U)->result
        == FACTORY_RESULT_OUT_OF_BOUNDS);
    CHECK(factory_simulation_is_inserter(simulation, 1U));
    CHECK(factory_simulation_get_inserter(simulation, 1U, &state));
    CHECK(state.facing == FACTORY_DIRECTION_EAST);
    CHECK(state.source_x == 0 && state.source_y == 1);
    CHECK(state.destination_x == 2 && state.destination_y == 1);
    CHECK(state.state == FACTORY_INSERTER_STATE_IDLE);
    CHECK(state.held_item == FACTORY_ITEM_NONE && state.held_amount == 0U);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {0, 1}}
    });
    factory_simulation_tick(simulation);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_inserter(simulation, 1U, &state));
    CHECK(state.state == FACTORY_INSERTER_STATE_IDLE);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {1U}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(!factory_simulation_entity_is_valid(simulation, 1U));

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_belt_pickup_storage_drop_timing_and_backpressure(void)
{
    FactoryWorld *world = factory_world_create(4U, 3U);
    FactorySimulation *simulation;
    FactoryInserter state;
    FactoryStorage storage_state;
    bool saw_picking = false;
    bool saw_holding = false;
    bool saw_dropping = false;
    bool tested_pickup_demolition = false;
    uint64_t pickup_tick = 0U;
    uint64_t drop_tick = 0U;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 3U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, inserter(2, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {3, 0}}
    });
    CHECK(factory_test_submit_power_row(simulation, 4U, 1U));
    factory_simulation_tick(simulation);

    for (uint32_t tick = 0U; tick < 100U; ++tick) {
        FactoryItemType before;

        CHECK(factory_simulation_get_inserter(simulation, 3U, &state));
        before = state.held_item;
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_inserter(simulation, 3U, &state));
        saw_picking |= state.state == FACTORY_INSERTER_STATE_PICKING_UP;
        saw_holding |= state.state == FACTORY_INSERTER_STATE_HOLDING;
        saw_dropping |= state.state == FACTORY_INSERTER_STATE_DROPPING;
        if (before == FACTORY_ITEM_NONE
            && state.held_item != FACTORY_ITEM_NONE
            && pickup_tick == 0U) {
            pickup_tick = factory_simulation_get_tick(simulation);
        }
        CHECK(accounted_iron(simulation, world, 0, 0, 4U) == 3U);
        CHECK(factory_simulation_get_storage(simulation, 4U, &storage_state));
        if (storage_state.iron_ore_amount == 1U && drop_tick == 0U) {
            drop_tick = factory_simulation_get_tick(simulation);
        }
        if (state.state == FACTORY_INSERTER_STATE_PICKING_UP
            && !tested_pickup_demolition) {
            submit(simulation, (FactoryCommand){
                FACTORY_COMMAND_DEMOLISH_ENTITY,
                {.demolish_entity = {3U}}
            });
            factory_simulation_tick(simulation);
            CHECK(factory_simulation_get_command_result(
                simulation, 0U
            )->result == FACTORY_RESULT_ENTITY_BUSY);
            CHECK(factory_simulation_entity_is_valid(simulation, 3U));
            CHECK(factory_simulation_get_inserter(
                simulation, 3U, &state
            ));
            if (state.held_item != FACTORY_ITEM_NONE
                && pickup_tick == 0U) {
                pickup_tick = factory_simulation_get_tick(simulation);
            }
            tested_pickup_demolition = true;
        }
    }
    CHECK(saw_picking && saw_holding && saw_dropping);
    CHECK(tested_pickup_demolition);
    CHECK(pickup_tick != 0U && drop_tick > pickup_tick);
    CHECK(storage_state.iron_ore_amount == 3U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);

    world = factory_world_create(5U, 3U);
    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 3U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, inserter(2, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {3, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {4, 0}}
    });
    CHECK(factory_test_submit_power_row(simulation, 5U, 1U));
    factory_simulation_tick(simulation);
    {
        for (uint32_t tick = 0U; tick < 100U; ++tick) {
            factory_simulation_tick(simulation);
            CHECK(accounted_iron(simulation, world, 0, 0, 5U) == 3U);
        }
    }
    CHECK(factory_simulation_get_storage(simulation, 5U, &storage_state));
    CHECK(storage_state.iron_ore_amount == 3U);
    CHECK(factory_simulation_get_inserter(simulation, 3U, &state));
    CHECK(state.state == FACTORY_INSERTER_STATE_IDLE);
    CHECK(state.held_item == FACTORY_ITEM_NONE);
    /*
     * Delivery resumed after the downstream belt cleared, and all three
     * items reached storage without bypassing the held state.
     */
    for (uint32_t tick = 0U; tick < 2U; ++tick) {
        factory_simulation_tick(simulation);
        CHECK(accounted_iron(simulation, world, 0, 0, 5U) == 3U);
    }

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_drop_contention_lowest_id_wins(void)
{
    FactoryWorld *world = factory_world_create(5U, 4U);
    FactorySimulation *simulation;
    FactoryBelt destination;
    FactoryInserter left;
    FactoryInserter right;
    FactoryStorage storage;
    bool saw_backpressure = false;
    bool tested_held_demolition = false;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 1U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 4, 0, FACTORY_RESOURCE_IRON, 1U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_SOUTH}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {4, 0, FACTORY_DIRECTION_SOUTH}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {0, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {4, 1, FACTORY_DIRECTION_WEST}}
    });
    submit(simulation, inserter(1, 1, FACTORY_DIRECTION_EAST));
    submit(simulation, inserter(3, 1, FACTORY_DIRECTION_WEST));
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {2, 1, FACTORY_DIRECTION_NORTH}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {2, 0}}
    });
    CHECK(factory_test_submit_power_row(simulation, 5U, 2U));
    factory_simulation_tick(simulation);

    for (uint32_t tick = 0U; tick < 60U; ++tick) {
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_belt(simulation, 7U, &destination));
        CHECK(factory_simulation_get_inserter(simulation, 6U, &right));
        if (destination.item == FACTORY_ITEM_IRON_ORE
            && right.held_item == FACTORY_ITEM_IRON_ORE) {
            saw_backpressure = true;
            if (!tested_held_demolition) {
                submit(simulation, (FactoryCommand){
                    FACTORY_COMMAND_DEMOLISH_ENTITY,
                    {.demolish_entity = {6U}}
                });
                factory_simulation_tick(simulation);
                CHECK(factory_simulation_get_command_result(
                    simulation, 0U
                )->result == FACTORY_RESULT_ENTITY_HAS_MATERIAL);
                tested_held_demolition = true;
            }
        }
    }
    CHECK(factory_simulation_get_belt(simulation, 7U, &destination));
    CHECK(factory_simulation_get_inserter(simulation, 5U, &left));
    CHECK(factory_simulation_get_inserter(simulation, 6U, &right));
    CHECK(factory_simulation_get_storage(simulation, 8U, &storage));
    CHECK(saw_backpressure);
    CHECK(tested_held_demolition);
    CHECK(storage.iron_ore_amount == 2U);
    CHECK(left.held_item == FACTORY_ITEM_NONE);
    CHECK(right.held_item == FACTORY_ITEM_NONE);
    CHECK(right.state == FACTORY_INSERTER_STATE_IDLE);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_pickup_contention_and_determinism(void)
{
    FactoryWorld *world[2];
    FactorySimulation *simulation[2];

    for (size_t run = 0U; run < 2U; ++run) {
        world[run] = factory_world_create(5U, 5U);
        CHECK(factory_world_add_resource(
            world[run], 2, 0, FACTORY_RESOURCE_IRON, 1U
        ) == FACTORY_RESULT_OK);
        simulation[run] = factory_simulation_create_with_construction_units(world[run], UINT32_MAX);
        submit(simulation[run], (FactoryCommand){
            FACTORY_COMMAND_PLACE_EXTRACTOR,
            {.place_extractor = {2, 0, FACTORY_DIRECTION_SOUTH}}
        });
        submit(simulation[run], (FactoryCommand){
            FACTORY_COMMAND_PLACE_BELT,
            {.place_belt = {2, 1, FACTORY_DIRECTION_SOUTH}}
        });
        submit(simulation[run], inserter(1, 1, FACTORY_DIRECTION_WEST));
        submit(simulation[run], inserter(3, 1, FACTORY_DIRECTION_EAST));
        submit(simulation[run], (FactoryCommand){
            FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {0, 1}}
        });
        submit(simulation[run], (FactoryCommand){
            FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {4, 1}}
        });
        CHECK(factory_test_submit_power_row(simulation[run], 5U, 3U));
        factory_simulation_tick(simulation[run]);
    }
    for (uint32_t tick = 0U; tick < 50U; ++tick) {
        FactoryInserter left[2];
        FactoryInserter right[2];
        FactoryStorage left_storage[2];
        FactoryStorage right_storage[2];

        factory_simulation_tick(simulation[0]);
        factory_simulation_tick(simulation[1]);
        for (size_t run = 0U; run < 2U; ++run) {
            CHECK(factory_simulation_get_inserter(
                simulation[run], 3U, &left[run]
            ));
            CHECK(factory_simulation_get_inserter(
                simulation[run], 4U, &right[run]
            ));
            CHECK(factory_simulation_get_storage(
                simulation[run], 5U, &left_storage[run]
            ));
            CHECK(factory_simulation_get_storage(
                simulation[run], 6U, &right_storage[run]
            ));
            CHECK(accounted_iron(
                simulation[run], world[run], 2, 0, 6U
            ) == 1U);
        }
        CHECK(left[0].state == left[1].state);
        CHECK(left[0].held_item == left[1].held_item);
        CHECK(right[0].state == right[1].state);
        CHECK(right[0].held_item == right[1].held_item);
        CHECK(left_storage[0].iron_ore_amount
            == left_storage[1].iron_ore_amount);
        CHECK(right_storage[0].iron_ore_amount
            == right_storage[1].iron_ore_amount);
    }
    {
        FactoryStorage left;
        FactoryStorage right;

        CHECK(factory_simulation_get_storage(simulation[0], 5U, &left));
        CHECK(factory_simulation_get_storage(simulation[0], 6U, &right));
        CHECK(left.iron_ore_amount == 1U);
        CHECK(right.iron_ore_amount == 0U);
    }
    for (size_t run = 0U; run < 2U; ++run) {
        factory_simulation_destroy(simulation[run]);
        factory_world_destroy(world[run]);
    }
}

static void test_splitter_input_and_output(void)
{
    FactoryWorld *world = factory_world_create(5U, 5U);
    FactorySimulation *simulation;
    FactoryStorage storage_state;

    CHECK(factory_world_add_resource(
        world, 0, 1, FACTORY_RESOURCE_IRON, 2U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, inserter(2, 1, FACTORY_DIRECTION_EAST));
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_SPLITTER,
        {.place_splitter = {3, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, inserter(3, 0, FACTORY_DIRECTION_NORTH));
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {3, 2}}
    });
    CHECK(factory_test_submit_power_row(simulation, 5U, 3U));
    factory_simulation_tick(simulation);
    for (uint32_t tick = 0U; tick < 100U; ++tick) {
        factory_simulation_tick(simulation);
        CHECK(accounted_iron(simulation, world, 0, 1, 6U) == 2U);
    }
    CHECK(factory_simulation_get_storage(simulation, 6U, &storage_state));
    CHECK(storage_state.iron_ore_amount == 0U);

    /*
     * The output inserter faces north, so it correctly picks up from the
     * splitter's left (north) output but has no valid destination. Both
     * items remain conserved under backpressure.
     */
    {
        FactoryInserter output;

        CHECK(factory_simulation_get_inserter(simulation, 5U, &output));
        CHECK(output.held_item == FACTORY_ITEM_IRON_ORE);
        CHECK(output.state == FACTORY_INSERTER_STATE_DROPPING);
    }
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_machine_pipeline_and_elemental_conservation(void)
{
    FactoryWorld *world = factory_world_create(6U, 5U);
    FactorySimulation *simulation;
    FactoryStorage storage;
    bool saw_refinery_pickup = false;
    bool saw_assembler_pickup = false;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 2U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 0, 4, FACTORY_RESOURCE_COPPER, 2U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, inserter(2, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            3, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_SOUTH
        }}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 4, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 4, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, inserter(2, 4, FACTORY_DIRECTION_EAST));
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            3, 4, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_NORTH
        }}
    });
    submit(simulation, inserter(3, 1, FACTORY_DIRECTION_SOUTH));
    submit(simulation, inserter(3, 3, FACTORY_DIRECTION_NORTH));
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {3, 2, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, inserter(4, 2, FACTORY_DIRECTION_EAST));
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {5, 2}}
    });
    CHECK(factory_test_submit_power_pair(simulation, 0, 2, 1, 2));
    CHECK(factory_test_submit_power_pair(simulation, 5, 1, 5, 0));
    factory_simulation_tick(simulation);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {4U, FACTORY_RECIPE_IRON_PLATE}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {8U, FACTORY_RECIPE_COPPER_PLATE}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe = {
            11U, FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT
        }}
    });
    factory_simulation_tick(simulation);

    for (uint32_t tick = 0U; tick < 220U; ++tick) {
        FactoryInserter iron_output;
        FactoryInserter assembler_output;

        factory_simulation_tick(simulation);
        CHECK(accounted_element(simulation, world, 13U, false) == 2U);
        CHECK(accounted_element(simulation, world, 13U, true) == 2U);
        CHECK(factory_simulation_get_inserter(
            simulation, 9U, &iron_output
        ));
        CHECK(factory_simulation_get_inserter(
            simulation, 12U, &assembler_output
        ));
        saw_refinery_pickup |=
            iron_output.held_item == FACTORY_ITEM_IRON_PLATE;
        saw_assembler_pickup |= assembler_output.held_item
            == FACTORY_ITEM_ELECTRONIC_COMPONENT;
    }
    CHECK(factory_simulation_get_storage(simulation, 13U, &storage));
    CHECK(storage.electronic_component_amount == 2U);
    CHECK(saw_refinery_pickup);
    CHECK(saw_assembler_pickup);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    test_placement_orientation_and_invalid_sources();
    test_belt_pickup_storage_drop_timing_and_backpressure();
    test_pickup_contention_and_determinism();
    test_drop_contention_lowest_id_wins();
    test_splitter_input_and_output();
    test_machine_pipeline_and_elemental_conservation();

    if (failures != 0) {
        (void)fprintf(stderr, "%d inserter test(s) failed\n", failures);
        return 1;
    }
    (void)printf("inserter tests passed\n");
    return 0;
}
