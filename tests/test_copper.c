#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand extractor(int32_t x, int32_t y)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {x, y, FACTORY_DIRECTION_EAST}}};
    return c;
}
static FactoryCommand belt(int32_t x, int32_t y)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {x, y, FACTORY_DIRECTION_EAST}}};
    return c;
}
static FactoryCommand refinery(int32_t x, int32_t y)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            x, y, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
        }}};
    return c;
}
static FactoryCommand storage(int32_t x, int32_t y)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage = {x, y}}};
    return c;
}
static FactoryCommand select(FactoryEntityId id, FactoryRecipeId recipe)
{
    FactoryCommand c = {FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {id, recipe}}};
    return c;
}
static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

typedef struct {
    FactoryWorld *world;
    FactorySimulation *simulation;
    FactoryEntityId ids[12];
} Mixed;

static Mixed mixed_create(void)
{
    Mixed mixed;

    mixed.world = factory_world_create(6U, 2U);
    CHECK(factory_world_add_resource(
        mixed.world, 0, 0, FACTORY_RESOURCE_IRON, 20U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        mixed.world, 0, 1, FACTORY_RESOURCE_COPPER, 20U
    ) == FACTORY_RESULT_OK);
    mixed.simulation = factory_simulation_create_with_construction_units(mixed.world, UINT32_MAX);
    for (int32_t y = 0; y < 2; ++y) {
        submit(mixed.simulation, extractor(0, y));
        submit(mixed.simulation, belt(1, y));
        submit(mixed.simulation, refinery(2, y));
        submit(mixed.simulation, belt(3, y));
        submit(mixed.simulation, belt(4, y));
        submit(mixed.simulation, storage(5, y));
    }
    factory_simulation_tick(mixed.simulation);
    for (size_t index = 0U; index < 12U; ++index) {
        mixed.ids[index] = factory_simulation_get_command_result(
            mixed.simulation, index
        )->entity_id;
    }
    submit(mixed.simulation, select(
        mixed.ids[2], FACTORY_RECIPE_IRON_PLATE
    ));
    submit(mixed.simulation, select(
        mixed.ids[8], FACTORY_RECIPE_COPPER_PLATE
    ));
    factory_simulation_tick(mixed.simulation);
    return mixed;
}

static uint32_t element_total(const Mixed *mixed, bool copper)
{
    uint32_t total = factory_world_get_tile(
        mixed->world, 0, copper ? 1 : 0
    )->resource_amount;
    FactoryItemType ore = copper
        ? FACTORY_ITEM_COPPER_ORE : FACTORY_ITEM_IRON_ORE;
    FactoryItemType plate = copper
        ? FACTORY_ITEM_COPPER_PLATE : FACTORY_ITEM_IRON_PLATE;

    for (size_t index = 0U; index < 12U; ++index) {
        FactoryExtractor ex;
        FactoryBelt b;
        FactoryRefinery r;
        FactoryStorage s;

        if (factory_simulation_get_extractor(
                mixed->simulation, mixed->ids[index], &ex)) {
            total += ex.output_item == ore ? ex.output_amount : 0U;
        } else if (factory_simulation_get_belt(
                mixed->simulation, mixed->ids[index], &b)) {
            total += b.item == ore || b.item == plate ? 1U : 0U;
        } else if (factory_simulation_get_refinery(
                mixed->simulation, mixed->ids[index], &r)) {
            total += r.input_item == ore ? r.input_amount : 0U;
            total += r.processing
                && factory_recipe_get(r.recipe_id)->input_item == ore ? 1U : 0U;
            total += r.output_item == plate ? r.output_amount : 0U;
        } else if (factory_simulation_get_storage(
                mixed->simulation, mixed->ids[index], &s)) {
            total += copper
                ? s.copper_ore_amount + s.copper_plate_amount
                : s.iron_ore_amount + s.iron_plate_amount;
        }
    }
    return total;
}

static void test_resources_and_mixed_pipeline(void)
{
    Mixed mixed = mixed_create();
    FactoryExtractor iron_extractor;
    FactoryExtractor copper_extractor;
    FactoryStorage iron_storage;
    FactoryStorage copper_storage;

    CHECK(strcmp(factory_resource_name(FACTORY_RESOURCE_IRON), "iron") == 0);
    CHECK(strcmp(factory_resource_name(FACTORY_RESOURCE_COPPER), "copper") == 0);
    CHECK(strcmp(factory_resource_name((FactoryResourceType)99),
        "invalid resource") == 0);
    CHECK(factory_world_get_tile(mixed.world, 0, 0)->resource
        == FACTORY_RESOURCE_IRON);
    CHECK(factory_world_get_tile(mixed.world, 0, 1)->resource
        == FACTORY_RESOURCE_COPPER);
    CHECK(factory_simulation_get_extractor(
        mixed.simulation, mixed.ids[0], &iron_extractor
    ));
    CHECK(factory_simulation_get_extractor(
        mixed.simulation, mixed.ids[6], &copper_extractor
    ));
    CHECK(iron_extractor.resource_type == FACTORY_RESOURCE_IRON);
    CHECK(iron_extractor.produced_item == FACTORY_ITEM_IRON_ORE);
    CHECK(copper_extractor.resource_type == FACTORY_RESOURCE_COPPER);
    CHECK(copper_extractor.produced_item == FACTORY_ITEM_COPPER_ORE);

    while (factory_simulation_get_tick(mixed.simulation) < 80U) {
        factory_simulation_tick(mixed.simulation);
        CHECK(element_total(&mixed, false) == 20U);
        CHECK(element_total(&mixed, true) == 20U);
    }
    CHECK(factory_world_get_tile(mixed.world, 0, 0)->resource_amount < 20U);
    CHECK(factory_world_get_tile(mixed.world, 0, 1)->resource_amount < 20U);
    CHECK(factory_simulation_get_storage(
        mixed.simulation, mixed.ids[5], &iron_storage
    ));
    CHECK(factory_simulation_get_storage(
        mixed.simulation, mixed.ids[11], &copper_storage
    ));
    CHECK(iron_storage.iron_plate_amount > 0U);
    CHECK(iron_storage.copper_plate_amount == 0U);
    CHECK(copper_storage.copper_plate_amount > 0U);
    CHECK(copper_storage.iron_plate_amount == 0U);
    factory_simulation_destroy(mixed.simulation);
    factory_world_destroy(mixed.world);
}

static void test_mixed_determinism(void)
{
    Mixed a = mixed_create();
    Mixed b = mixed_create();

    for (uint32_t tick = 2U; tick < 150U; ++tick) {
        factory_simulation_tick(a.simulation);
        factory_simulation_tick(b.simulation);
        CHECK(factory_simulation_get_tick(a.simulation)
            == factory_simulation_get_tick(b.simulation));
        CHECK(element_total(&a, false) == 20U);
        CHECK(element_total(&a, true) == 20U);
        for (size_t index = 0U; index < 12U; ++index) {
            FactoryBelt belt_a;
            FactoryBelt belt_b;
            FactoryRefinery refinery_a;
            FactoryRefinery refinery_b;

            CHECK(a.ids[index] == b.ids[index]);
            if (factory_simulation_get_belt(
                    a.simulation, a.ids[index], &belt_a)) {
                CHECK(factory_simulation_get_belt(
                    b.simulation, b.ids[index], &belt_b));
                CHECK(belt_a.item == belt_b.item);
                CHECK(belt_a.movement_progress == belt_b.movement_progress);
            }
            if (factory_simulation_get_refinery(
                    a.simulation, a.ids[index], &refinery_a)) {
                CHECK(factory_simulation_get_refinery(
                    b.simulation, b.ids[index], &refinery_b));
                CHECK(refinery_a.recipe_id == refinery_b.recipe_id);
                CHECK(refinery_a.input_item == refinery_b.input_item);
                CHECK(refinery_a.output_item == refinery_b.output_item);
                CHECK(refinery_a.processing == refinery_b.processing);
                CHECK(refinery_a.processing_progress
                    == refinery_b.processing_progress);
            }
        }
    }
    factory_simulation_destroy(a.simulation);
    factory_simulation_destroy(b.simulation);
    factory_world_destroy(a.world);
    factory_world_destroy(b.world);
}

static void test_copper_ore_storage_and_recipe_mismatch(void)
{
    FactoryWorld *world = factory_world_create(3U, 2U);
    FactorySimulation *simulation;
    FactoryStorage stored;
    FactoryBelt blocked;
    FactoryRefinery wrong_recipe;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_COPPER, 2U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 0, 1, FACTORY_RESOURCE_COPPER, 2U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 1, 0, (FactoryResourceType)99, 1U
    ) == FACTORY_RESULT_INVALID_ARGUMENT);
    simulation = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, extractor(0, 0));
    submit(simulation, belt(1, 0));
    submit(simulation, storage(2, 0));
    submit(simulation, extractor(0, 1));
    submit(simulation, belt(1, 1));
    submit(simulation, refinery(2, 1));
    factory_simulation_tick(simulation);
    submit(simulation, select(6U, FACTORY_RECIPE_IRON_PLATE));
    factory_simulation_tick(simulation);
    while (factory_simulation_get_tick(simulation) < 24U) {
        factory_simulation_tick(simulation);
    }
    CHECK(factory_simulation_get_storage(simulation, 3U, &stored));
    CHECK(stored.copper_ore_amount == 1U);
    CHECK(factory_simulation_get_belt(simulation, 5U, &blocked));
    CHECK(blocked.item == FACTORY_ITEM_COPPER_ORE);
    CHECK(blocked.movement_progress == FACTORY_BELT_TRANSFER_TICKS);
    CHECK(factory_simulation_get_refinery(simulation, 6U, &wrong_recipe));
    CHECK(wrong_recipe.recipe_id == FACTORY_RECIPE_IRON_PLATE);
    CHECK(wrong_recipe.input_item == FACTORY_ITEM_NONE);
    CHECK(!wrong_recipe.processing);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    test_resources_and_mixed_pipeline();
    test_mixed_determinism();
    test_copper_ore_storage_and_recipe_mismatch();
    if (failures != 0) {
        return 1;
    }
    (void)printf("All copper tests passed.\n");
    return 0;
}
