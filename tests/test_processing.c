#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand extractor(int32_t x, int32_t y, FactoryDirection d)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {x, y, d}}};
    return c;
}
static FactoryCommand belt(int32_t x, int32_t y, FactoryDirection d)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {x, y, d}}};
    return c;
}
static FactoryCommand refinery(
    int32_t x, int32_t y, FactoryDirection input, FactoryDirection output
)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {x, y, input, output}}};
    return c;
}
static FactoryCommand storage(int32_t x, int32_t y)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage = {x, y}}};
    return c;
}
static FactoryCommand set_recipe(FactoryEntityId id, FactoryRecipeId recipe_id)
{
    FactoryCommand c = {FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {id, recipe_id}}};
    return c;
}
static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static uint32_t total_iron(
    FactorySimulation *simulation,
    FactoryWorld *world,
    const FactoryEntityId *ids,
    size_t count
)
{
    uint32_t total = factory_world_get_tile(world, 0, 0)->resource_amount;

    for (size_t index = 0U; index < count; ++index) {
        FactoryExtractor extractor_state;
        FactoryBelt belt_state;
        FactoryRefinery refinery_state;
        FactoryStorage storage_state;

        if (factory_simulation_get_extractor(
                simulation, ids[index], &extractor_state)) {
            total += extractor_state.output_amount;
        } else if (factory_simulation_get_belt(
                simulation, ids[index], &belt_state)) {
            total += belt_state.item == FACTORY_ITEM_NONE ? 0U : 1U;
        } else if (factory_simulation_get_refinery(
                simulation, ids[index], &refinery_state)) {
            total += refinery_state.input_amount;
            total += refinery_state.processing ? 1U : 0U;
            total += refinery_state.output_amount;
        } else if (factory_simulation_get_storage(
                simulation, ids[index], &storage_state)) {
            total += factory_storage_get_total_amount(&storage_state);
        }
    }
    return total;
}

static void test_processing_boundaries_and_blocking(void)
{
    FactoryWorld *world = factory_world_create(3U, 1U);
    FactorySimulation *simulation;
    FactoryEntityId ids[3];
    FactoryRefinery state;
    FactoryBelt input;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 3U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, extractor(0, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(1, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, refinery(
        2, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
    ));
    factory_simulation_tick(simulation);
    for (size_t index = 0U; index < 3U; ++index) {
        ids[index] =
            factory_simulation_get_command_result(simulation, index)->entity_id;
    }
    submit(simulation, set_recipe(ids[2], FACTORY_RECIPE_IRON_PLATE));
    factory_simulation_tick(simulation);
    while (factory_simulation_get_tick(simulation) < 24U) {
        factory_simulation_tick(simulation);
        CHECK(total_iron(simulation, world, ids, 3U) == 3U);
    }
    CHECK(factory_simulation_get_refinery(simulation, ids[2], &state));
    CHECK(state.processing);
    CHECK(state.processing_progress == 1U);
    CHECK(state.input_item == FACTORY_ITEM_NONE && state.input_amount == 0U);
    CHECK(state.output_item == FACTORY_ITEM_NONE);

    submit(simulation, set_recipe(ids[2], FACTORY_RECIPE_COPPER_PLATE));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_INVALID_STATE);
    CHECK(factory_simulation_get_refinery(simulation, ids[2], &state));
    CHECK(state.recipe_id == FACTORY_RECIPE_IRON_PLATE);

    while (factory_simulation_get_tick(simulation) < 32U) {
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_refinery(simulation, ids[2], &state));
        CHECK(state.processing);
        CHECK(state.output_item == FACTORY_ITEM_NONE);
        CHECK(total_iron(simulation, world, ids, 3U) == 3U);
    }
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_tick(simulation) == 33U);
    CHECK(factory_simulation_get_refinery(simulation, ids[2], &state));
    CHECK(!state.processing);
    CHECK(state.processing_progress == 0U);
    CHECK(state.output_item == FACTORY_ITEM_IRON_PLATE);
    CHECK(state.output_amount == 1U);
    CHECK(total_iron(simulation, world, ids, 3U) == 3U);

    while (factory_simulation_get_tick(simulation) < 44U) {
        factory_simulation_tick(simulation);
    }
    CHECK(factory_simulation_get_refinery(simulation, ids[2], &state));
    CHECK(state.output_item == FACTORY_ITEM_IRON_PLATE);
    CHECK(state.input_item == FACTORY_ITEM_IRON_ORE);
    CHECK(state.input_amount == 1U);
    CHECK(!state.processing);
    CHECK(factory_simulation_get_belt(simulation, ids[1], &input));
    CHECK(input.item == FACTORY_ITEM_NONE);
    CHECK(total_iron(simulation, world, ids, 3U) == 3U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_complete_pipeline_and_storage_items(void)
{
    FactoryWorld *world = factory_world_create(6U, 1U);
    FactorySimulation *simulation;
    FactoryEntityId ids[6];
    FactoryRefinery refinery_state;
    FactoryBelt output;
    FactoryStorage storage_state;
    uint32_t amount;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 5U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, extractor(0, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(1, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, refinery(
        2, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
    ));
    submit(simulation, belt(3, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, belt(4, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, storage(5, 0));
    factory_simulation_tick(simulation);
    for (size_t index = 0U; index < 6U; ++index) {
        ids[index] =
            factory_simulation_get_command_result(simulation, index)->entity_id;
    }
    submit(simulation, set_recipe(ids[2], FACTORY_RECIPE_IRON_PLATE));
    factory_simulation_tick(simulation);
    while (factory_simulation_get_tick(simulation) < 33U) {
        factory_simulation_tick(simulation);
        CHECK(total_iron(simulation, world, ids, 6U) == 5U);
    }
    CHECK(factory_simulation_get_refinery(simulation, ids[2], &refinery_state));
    CHECK(refinery_state.output_item == FACTORY_ITEM_IRON_PLATE);
    CHECK(factory_simulation_get_belt(simulation, ids[3], &output));
    CHECK(output.item == FACTORY_ITEM_NONE);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_refinery(simulation, ids[2], &refinery_state));
    CHECK(refinery_state.output_item == FACTORY_ITEM_NONE);
    CHECK(factory_simulation_get_belt(simulation, ids[3], &output));
    CHECK(output.item == FACTORY_ITEM_IRON_PLATE);
    CHECK(output.movement_progress == 1U);
    while (factory_simulation_get_tick(simulation) < 43U) {
        factory_simulation_tick(simulation);
        CHECK(total_iron(simulation, world, ids, 6U) == 5U);
    }
    CHECK(factory_simulation_get_storage(simulation, ids[5], &storage_state));
    CHECK(storage_state.iron_plate_amount == 1U);
    CHECK(storage_state.iron_ore_amount == 0U);
    CHECK(factory_storage_get_total_amount(&storage_state) == 1U);
    CHECK(factory_storage_get_item_amount(
        &storage_state, FACTORY_ITEM_IRON_PLATE, &amount
    ));
    CHECK(amount == 1U);
    CHECK(!factory_storage_get_item_amount(
        &storage_state, FACTORY_ITEM_NONE, &amount
    ));
    CHECK(!factory_storage_get_item_amount(
        &storage_state, (FactoryItemType)99, &amount
    ));
    CHECK(total_iron(simulation, world, ids, 6U) == 5U);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void setup_pipeline(
    FactoryWorld **out_world,
    FactorySimulation **out_simulation
)
{
    *out_world = factory_world_create(6U, 1U);
    CHECK(factory_world_add_resource(
        *out_world, 0, 0, FACTORY_RESOURCE_IRON, 8U
    ) == FACTORY_RESULT_OK);
    *out_simulation = factory_simulation_create_with_construction_units(*out_world, UINT32_MAX);
    submit(*out_simulation, extractor(0, 0, FACTORY_DIRECTION_EAST));
    submit(*out_simulation, belt(1, 0, FACTORY_DIRECTION_EAST));
    submit(*out_simulation, refinery(
        2, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
    ));
    submit(*out_simulation, belt(3, 0, FACTORY_DIRECTION_EAST));
    submit(*out_simulation, belt(4, 0, FACTORY_DIRECTION_EAST));
    submit(*out_simulation, storage(5, 0));
}

static void test_processing_determinism(void)
{
    FactoryWorld *world_a;
    FactoryWorld *world_b;
    FactorySimulation *a;
    FactorySimulation *b;

    setup_pipeline(&world_a, &a);
    setup_pipeline(&world_b, &b);
    factory_simulation_tick(a);
    factory_simulation_tick(b);
    submit(a, set_recipe(3U, FACTORY_RECIPE_IRON_PLATE));
    submit(b, set_recipe(3U, FACTORY_RECIPE_IRON_PLATE));
    for (uint32_t tick = 1U; tick < 120U; ++tick) {
        factory_simulation_tick(a);
        factory_simulation_tick(b);
        CHECK(factory_simulation_get_tick(a) == factory_simulation_get_tick(b));
        CHECK(factory_world_get_tile(world_a, 0, 0)->resource_amount
            == factory_world_get_tile(world_b, 0, 0)->resource_amount);
        for (FactoryEntityId id = 1U; id <= 6U; ++id) {
            FactoryExtractor extractor_a;
            FactoryExtractor extractor_b;
            FactoryBelt belt_a;
            FactoryBelt belt_b;
            FactoryRefinery refinery_a;
            FactoryRefinery refinery_b;
            FactoryStorage storage_a;
            FactoryStorage storage_b;

            CHECK(factory_simulation_entity_is_valid(a, id)
                == factory_simulation_entity_is_valid(b, id));
            if (factory_simulation_get_extractor(a, id, &extractor_a)) {
                CHECK(factory_simulation_get_extractor(b, id, &extractor_b));
                CHECK(extractor_a.production_progress
                    == extractor_b.production_progress);
                CHECK(extractor_a.output_item == extractor_b.output_item);
            } else if (factory_simulation_get_belt(a, id, &belt_a)) {
                CHECK(factory_simulation_get_belt(b, id, &belt_b));
                CHECK(belt_a.item == belt_b.item);
                CHECK(belt_a.movement_progress == belt_b.movement_progress);
            } else if (factory_simulation_get_refinery(a, id, &refinery_a)) {
                CHECK(factory_simulation_get_refinery(b, id, &refinery_b));
                CHECK(refinery_a.input_item == refinery_b.input_item);
                CHECK(refinery_a.output_item == refinery_b.output_item);
                CHECK(refinery_a.processing == refinery_b.processing);
                CHECK(refinery_a.processing_progress
                    == refinery_b.processing_progress);
            } else if (factory_simulation_get_storage(a, id, &storage_a)) {
                CHECK(factory_simulation_get_storage(b, id, &storage_b));
                CHECK(storage_a.iron_ore_amount == storage_b.iron_ore_amount);
                CHECK(storage_a.iron_plate_amount
                    == storage_b.iron_plate_amount);
            }
        }
    }
    factory_simulation_destroy(a);
    factory_simulation_destroy(b);
    factory_world_destroy(world_a);
    factory_world_destroy(world_b);
}

int main(void)
{
    test_processing_boundaries_and_blocking();
    test_complete_pipeline_and_storage_items();
    test_processing_determinism();
    if (failures != 0) {
        return 1;
    }
    (void)printf("All processing tests passed.\n");
    return 0;
}
