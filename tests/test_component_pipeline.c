#include "foundation/simulation.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

typedef struct {
    FactoryWorld *world;
    FactorySimulation *simulation;
    FactoryEntityId ids[12];
} Pipeline;

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static FactoryCommand extractor(int32_t x, int32_t y, FactoryDirection d)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {x, y, d}}};
    return c;
}
static FactoryCommand belt(
    int32_t x, int32_t y, FactoryDirection direction
)
{
    FactoryCommand c = {FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {x, y, direction}}};
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

static Pipeline create_pipeline(void)
{
    Pipeline p;
    FactoryCommand command;

    p.world = factory_world_create(7U, 2U);
    CHECK(factory_world_add_resource(
        p.world, 0, 1, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        p.world, 0, 0, FACTORY_RESOURCE_COPPER, 10U
    ) == FACTORY_RESULT_OK);
    p.simulation = factory_simulation_create_with_construction_units(p.world, UINT32_MAX);

    submit(p.simulation, extractor(0, 1, FACTORY_DIRECTION_EAST)); /* 1 */
    submit(p.simulation, belt(1, 1, FACTORY_DIRECTION_EAST));      /* 2 */
    submit(p.simulation, refinery(
        2, 1, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST));   /* 3 */
    submit(p.simulation, belt(3, 1, FACTORY_DIRECTION_EAST));      /* 4 */
    submit(p.simulation, extractor(0, 0, FACTORY_DIRECTION_EAST)); /* 5 */
    submit(p.simulation, belt(1, 0, FACTORY_DIRECTION_EAST));      /* 6 */
    submit(p.simulation, refinery(
        2, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST));   /* 7 */
    submit(p.simulation, belt(3, 0, FACTORY_DIRECTION_EAST));      /* 8 */
    submit(p.simulation, belt(4, 0, FACTORY_DIRECTION_SOUTH));     /* 9 */
    command = (FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {4, 1, FACTORY_DIRECTION_EAST}}};
    submit(p.simulation, command);                                 /* 10 */
    submit(p.simulation, belt(5, 1, FACTORY_DIRECTION_EAST));      /* 11 */
    command = (FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage = {6, 1}}};
    submit(p.simulation, command);                                 /* 12 */
    factory_simulation_tick(p.simulation);
    for (size_t index = 0U; index < 12U; ++index) {
        p.ids[index] = factory_simulation_get_command_result(
            p.simulation, index
        )->entity_id;
    }
    command = (FactoryCommand){FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {p.ids[2], FACTORY_RECIPE_IRON_PLATE}}};
    submit(p.simulation, command);
    command = (FactoryCommand){FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {p.ids[6], FACTORY_RECIPE_COPPER_PLATE}}};
    submit(p.simulation, command);
    factory_simulation_tick(p.simulation);
    return p;
}

static uint32_t total(const Pipeline *p, bool copper)
{
    uint32_t value = factory_world_get_tile(
        p->world, 0, copper ? 0 : 1
    )->resource_amount;
    FactoryItemType ore = copper
        ? FACTORY_ITEM_COPPER_ORE : FACTORY_ITEM_IRON_ORE;
    FactoryItemType plate = copper
        ? FACTORY_ITEM_COPPER_PLATE : FACTORY_ITEM_IRON_PLATE;

    for (size_t index = 0U; index < 12U; ++index) {
        FactoryExtractor ex;
        FactoryBelt belt_state;
        FactoryRefinery refinery_state;
        FactoryAssembler assembler_state;
        FactoryStorage storage_state;

        if (factory_simulation_get_extractor(
                p->simulation, p->ids[index], &ex)) {
            value += ex.output_item == ore ? ex.output_amount : 0U;
        } else if (factory_simulation_get_belt(
                p->simulation, p->ids[index], &belt_state)) {
            value += belt_state.item == ore
                || belt_state.item == plate
                || belt_state.item == FACTORY_ITEM_ELECTRONIC_COMPONENT
                ? 1U : 0U;
        } else if (factory_simulation_get_refinery(
                p->simulation, p->ids[index], &refinery_state)) {
            const FactoryRecipe *recipe =
                factory_recipe_get(refinery_state.recipe_id);
            value += refinery_state.input_item == ore
                ? refinery_state.input_amount : 0U;
            value += refinery_state.processing
                && recipe != NULL && recipe->input_item == ore ? 1U : 0U;
            value += refinery_state.output_item == plate
                ? refinery_state.output_amount : 0U;
        } else if (factory_simulation_get_assembler(
                p->simulation, p->ids[index], &assembler_state)) {
            value += copper
                ? assembler_state.copper_plate_amount
                : assembler_state.iron_plate_amount;
            value += assembler_state.processing ? 1U : 0U;
            value += assembler_state.output_item
                == FACTORY_ITEM_ELECTRONIC_COMPONENT
                ? assembler_state.output_amount : 0U;
        } else if (factory_simulation_get_storage(
                p->simulation, p->ids[index], &storage_state)) {
            value += copper
                ? storage_state.copper_ore_amount
                    + storage_state.copper_plate_amount
                    + storage_state.electronic_component_amount
                : storage_state.iron_ore_amount
                    + storage_state.iron_plate_amount
                    + storage_state.electronic_component_amount;
        }
    }
    return value;
}

static void destroy_pipeline(Pipeline *p)
{
    factory_simulation_destroy(p->simulation);
    factory_world_destroy(p->world);
}

static void test_pipeline_boundaries(void)
{
    Pipeline p = create_pipeline();
    FactoryAssembler assembler_state;
    FactoryBelt output;
    FactoryStorage storage_state;

    while (factory_simulation_get_tick(p.simulation) < 38U) {
        factory_simulation_tick(p.simulation);
        CHECK(total(&p, false) == 10U);
        CHECK(total(&p, true) == 10U);
    }
    CHECK(factory_simulation_get_assembler(
        p.simulation, p.ids[9], &assembler_state
    ));
    CHECK(assembler_state.iron_plate_amount == 1U);
    CHECK(assembler_state.copper_plate_amount == 0U);
    CHECK(!assembler_state.processing);

    while (factory_simulation_get_tick(p.simulation) < 43U) {
        factory_simulation_tick(p.simulation);
    }
    CHECK(factory_simulation_get_assembler(
        p.simulation, p.ids[9], &assembler_state
    ));
    CHECK(assembler_state.iron_plate_amount == 0U);
    CHECK(assembler_state.copper_plate_amount == 0U);
    CHECK(assembler_state.processing);
    CHECK(assembler_state.processing_progress == 1U);
    CHECK(total(&p, false) == 10U);
    CHECK(total(&p, true) == 10U);

    while (factory_simulation_get_tick(p.simulation) < 56U) {
        factory_simulation_tick(p.simulation);
        CHECK(total(&p, false) == 10U);
        CHECK(total(&p, true) == 10U);
    }
    CHECK(factory_simulation_get_assembler(
        p.simulation, p.ids[9], &assembler_state
    ));
    CHECK(assembler_state.processing);
    CHECK(assembler_state.output_item == FACTORY_ITEM_NONE);
    factory_simulation_tick(p.simulation);
    CHECK(factory_simulation_get_tick(p.simulation) == 57U);
    CHECK(factory_simulation_get_assembler(
        p.simulation, p.ids[9], &assembler_state
    ));
    CHECK(!assembler_state.processing);
    CHECK(assembler_state.processing_progress == 0U);
    CHECK(assembler_state.output_item
        == FACTORY_ITEM_ELECTRONIC_COMPONENT);
    CHECK(factory_simulation_get_belt(p.simulation, p.ids[10], &output));
    CHECK(output.item == FACTORY_ITEM_NONE);

    factory_simulation_tick(p.simulation);
    CHECK(factory_simulation_get_assembler(
        p.simulation, p.ids[9], &assembler_state
    ));
    CHECK(assembler_state.output_item == FACTORY_ITEM_NONE);
    CHECK(factory_simulation_get_belt(p.simulation, p.ids[10], &output));
    CHECK(output.item == FACTORY_ITEM_ELECTRONIC_COMPONENT);
    CHECK(output.movement_progress == 1U);
    while (factory_simulation_get_tick(p.simulation) < 62U) {
        factory_simulation_tick(p.simulation);
    }
    CHECK(factory_simulation_get_storage(
        p.simulation, p.ids[11], &storage_state
    ));
    CHECK(storage_state.electronic_component_amount == 1U);
    CHECK(total(&p, false) == 10U);
    CHECK(total(&p, true) == 10U);
    destroy_pipeline(&p);
}

static void test_pipeline_determinism(void)
{
    Pipeline a = create_pipeline();
    Pipeline b = create_pipeline();

    for (uint32_t tick = 2U; tick < 140U; ++tick) {
        FactoryAssembler aa;
        FactoryAssembler ab;
        FactoryStorage sa;
        FactoryStorage sb;

        factory_simulation_tick(a.simulation);
        factory_simulation_tick(b.simulation);
        CHECK(factory_simulation_get_tick(a.simulation)
            == factory_simulation_get_tick(b.simulation));
        CHECK(total(&a, false) == total(&b, false));
        CHECK(total(&a, true) == total(&b, true));
        CHECK(factory_simulation_get_assembler(
            a.simulation, a.ids[9], &aa));
        CHECK(factory_simulation_get_assembler(
            b.simulation, b.ids[9], &ab));
        CHECK(aa.iron_plate_amount == ab.iron_plate_amount);
        CHECK(aa.copper_plate_amount == ab.copper_plate_amount);
        CHECK(aa.processing == ab.processing);
        CHECK(aa.processing_progress == ab.processing_progress);
        CHECK(aa.output_item == ab.output_item);
        CHECK(factory_simulation_get_storage(
            a.simulation, a.ids[11], &sa));
        CHECK(factory_simulation_get_storage(
            b.simulation, b.ids[11], &sb));
        CHECK(sa.electronic_component_amount
            == sb.electronic_component_amount);
    }
    destroy_pipeline(&a);
    destroy_pipeline(&b);
}

int main(void)
{
    test_pipeline_boundaries();
    test_pipeline_determinism();
    if (failures != 0) {
        return 1;
    }
    (void)printf("All component pipeline tests passed.\n");
    return 0;
}
