#include "foundation/simulation.h"
#include "power_fixture.h"

#include "assembler_internal.h"
#include "logistics_endpoint_internal.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryCommand place_assembler(int32_t x, int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {x, y, FACTORY_DIRECTION_EAST}}
    };
}

static FactoryCommand select_recipe(
    FactoryEntityId entity_id,
    FactoryAssemblerRecipeId recipe_id
)
{
    return (FactoryCommand){
        FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe = {entity_id, recipe_id}}
    };
}

static void check_recipe(
    FactoryAssemblerRecipeId id,
    FactoryItemType input_0,
    uint32_t amount_0,
    FactoryItemType input_1,
    uint32_t amount_1,
    FactoryItemType output,
    uint32_t output_amount
)
{
    FactoryAssemblerRecipe recipe;
    uint32_t input_iron;
    uint32_t input_copper;

    CHECK(factory_assembler_recipe_get(id, &recipe));
    CHECK(recipe.recipe_id == id);
    CHECK(recipe.input_items[0] == input_0);
    CHECK(recipe.input_amounts[0] == amount_0);
    CHECK(recipe.input_items[1] == input_1);
    CHECK(recipe.input_amounts[1] == amount_1);
    CHECK(recipe.input_count == (input_1 == FACTORY_ITEM_NONE ? 1U : 2U));
    CHECK(recipe.output_item == output);
    CHECK(recipe.output_amount == output_amount);
    CHECK(recipe.processing_ticks
        == FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS);

    input_iron = factory_item_iron_units(input_0) * amount_0
        + factory_item_iron_units(input_1) * amount_1;
    input_copper = factory_item_copper_units(input_0) * amount_0
        + factory_item_copper_units(input_1) * amount_1;
    CHECK(input_iron == factory_item_iron_units(output) * output_amount);
    CHECK(input_copper == factory_item_copper_units(output) * output_amount);
}

static void test_recipe_catalog(void)
{
    FactoryAssemblerRecipe recipe;

    check_recipe(
        FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT,
        FACTORY_ITEM_IRON_PLATE, 1U,
        FACTORY_ITEM_COPPER_PLATE, 1U,
        FACTORY_ITEM_ELECTRONIC_COMPONENT, 1U
    );
    check_recipe(
        FACTORY_ASSEMBLER_RECIPE_IRON_GEAR,
        FACTORY_ITEM_IRON_PLATE, 2U,
        FACTORY_ITEM_NONE, 0U,
        FACTORY_ITEM_IRON_GEAR, 1U
    );
    check_recipe(
        FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE,
        FACTORY_ITEM_COPPER_PLATE, 1U,
        FACTORY_ITEM_NONE, 0U,
        FACTORY_ITEM_COPPER_WIRE, 2U
    );
    CHECK(!factory_assembler_recipe_get(
        FACTORY_ASSEMBLER_RECIPE_NONE, &recipe
    ));
    CHECK(!factory_assembler_recipe_get(
        FACTORY_ASSEMBLER_RECIPE_COUNT, &recipe
    ));
    CHECK(!factory_assembler_recipe_get(
        FACTORY_ASSEMBLER_RECIPE_IRON_GEAR, NULL
    ));
    CHECK(strcmp(factory_item_name(FACTORY_ITEM_IRON_GEAR), "iron gear") == 0);
    CHECK(strcmp(
        factory_item_name(FACTORY_ITEM_COPPER_WIRE), "copper wire"
    ) == 0);
    CHECK(FACTORY_ELEMENT_UNIT_SCALE == 2U);
}

static void test_fifo_selection_and_safe_switching(void)
{
    FactoryWorld *world = factory_world_create(3U, 3U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryCommand place = place_assembler(0, 0);
    FactoryCommand select = select_recipe(
        1U, FACTORY_ASSEMBLER_RECIPE_IRON_GEAR
    );
    FactoryAssembler state;
    const FactoryCommandResult *result;

    CHECK(factory_simulation_submit_command(simulation, &place)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &select)
        == FACTORY_RESULT_OK);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result_count(simulation) == 2U);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    result = factory_simulation_get_command_result(simulation, 1U);
    CHECK(result->result == FACTORY_RESULT_OK);
    CHECK(result->previous_assembler_recipe
        == FACTORY_ASSEMBLER_RECIPE_NONE);
    CHECK(result->new_assembler_recipe
        == FACTORY_ASSEMBLER_RECIPE_IRON_GEAR);
    CHECK(factory_simulation_get_assembler(simulation, 1U, &state));
    CHECK(state.input_slots[0].item == FACTORY_ITEM_IRON_PLATE);
    CHECK(state.input_slots[0].capacity == 2U);
    CHECK(state.input_slots[1].item == FACTORY_ITEM_NONE);
    CHECK(state.input_slots[1].capacity == 0U);

    simulation->assemblers.items[0].input_slots[0].count = 1U;
    select = select_recipe(1U, FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE);
    CHECK(factory_simulation_submit_command(simulation, &select)
        == FACTORY_RESULT_OK);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_ASSEMBLER_NOT_EMPTY);
    CHECK(factory_simulation_get_assembler(simulation, 1U, &state));
    CHECK(state.recipe_id == FACTORY_ASSEMBLER_RECIPE_IRON_GEAR);
    CHECK(state.input_slots[0].count == 1U);

    simulation->assemblers.items[0].input_slots[0].count = 0U;
    CHECK(factory_simulation_submit_command(simulation, &select)
        == FACTORY_RESULT_OK);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_assembler(simulation, 1U, &state));
    CHECK(state.recipe_id == FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE);

    select = select_recipe(1U, FACTORY_ASSEMBLER_RECIPE_NONE);
    CHECK(factory_simulation_submit_command(simulation, &select)
        == FACTORY_RESULT_OK);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_assembler(simulation, 1U, &state));
    CHECK(state.recipe_id == FACTORY_ASSEMBLER_RECIPE_NONE);
    CHECK(state.input_slots[0].item == FACTORY_ITEM_NONE);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void run_recipe(
    FactoryAssemblerRecipeId recipe_id,
    FactoryItemType first_item,
    uint32_t first_count,
    FactoryItemType second_item,
    FactoryItemType expected_output,
    uint32_t expected_count
)
{
    FactoryWorld *world = factory_world_create(2U, 3U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryCommand place = place_assembler(0, 0);
    FactoryCommand select = select_recipe(1U, recipe_id);
    FactoryLogisticsEndpoint input_0 = {
        1U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0
    };
    FactoryLogisticsEndpoint input_1 = {
        1U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1
    };
    FactoryLogisticsEndpoint output = {
        1U, FACTORY_LOGISTICS_SLOT_OUTPUT
    };
    FactoryAssembler state;

    CHECK(factory_simulation_submit_command(simulation, &place)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(simulation, &select)
        == FACTORY_RESULT_OK);
    CHECK(factory_test_submit_power_row(simulation, 2U, 1U));
    factory_simulation_tick(simulation);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation, input_0, first_item
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation, input_0, FACTORY_ITEM_COPPER_WIRE
    ) == FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM);
    for (uint32_t index = 0U; index < first_count; ++index) {
        CHECK(factory_logistics_endpoint_insert(
            simulation, input_0, first_item
        ) == FACTORY_LOGISTICS_RESULT_OK);
    }
    CHECK(factory_logistics_endpoint_can_accept(
        simulation, input_0, first_item
    ) == FACTORY_LOGISTICS_RESULT_BLOCKED);
    if (second_item != FACTORY_ITEM_NONE) {
        CHECK(factory_logistics_endpoint_insert(
            simulation, input_1, second_item
        ) == FACTORY_LOGISTICS_RESULT_OK);
    } else {
        CHECK(factory_logistics_endpoint_can_accept(
            simulation, input_1, first_item
        ) == FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM);
    }

    for (uint32_t tick = 0U;
        tick < FACTORY_ASSEMBLER_ELECTRONIC_COMPONENT_TICKS;
        ++tick) {
        factory_assembler_store_update(&simulation->assemblers, simulation);
    }
    CHECK(factory_simulation_get_assembler(simulation, 1U, &state));
    CHECK(!state.processing);
    CHECK(state.output_item == expected_output);
    CHECK(state.output_amount == expected_count);
    CHECK(factory_logistics_endpoint_remove(
        simulation, output, expected_output
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_simulation_get_assembler(simulation, 1U, &state));
    CHECK(state.output_amount == expected_count - 1U);
    if (expected_count == 1U) {
        CHECK(state.output_item == FACTORY_ITEM_NONE);
    } else {
        CHECK(state.output_item == expected_output);
        CHECK(factory_logistics_endpoint_remove(
            simulation, output, expected_output
        ) == FACTORY_LOGISTICS_RESULT_OK);
        CHECK(factory_simulation_get_assembler(simulation, 1U, &state));
        CHECK(state.output_item == FACTORY_ITEM_NONE);
        CHECK(state.output_amount == 0U);
    }

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_storage_new_items(void)
{
    FactoryStorage storage = {0};
    uint32_t amount;

    storage.iron_gear_amount = 3U;
    storage.copper_wire_amount = 4U;
    CHECK(factory_storage_get_item_amount(
        &storage, FACTORY_ITEM_IRON_GEAR, &amount
    ));
    CHECK(amount == 3U);
    CHECK(factory_storage_get_item_amount(
        &storage, FACTORY_ITEM_COPPER_WIRE, &amount
    ));
    CHECK(amount == 4U);
    CHECK(factory_storage_get_total_amount(&storage) == 7U);
}

int main(void)
{
    test_recipe_catalog();
    test_fifo_selection_and_safe_switching();
    run_recipe(
        FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT,
        FACTORY_ITEM_IRON_PLATE, 1U,
        FACTORY_ITEM_COPPER_PLATE,
        FACTORY_ITEM_ELECTRONIC_COMPONENT, 1U
    );
    run_recipe(
        FACTORY_ASSEMBLER_RECIPE_IRON_GEAR,
        FACTORY_ITEM_IRON_PLATE, 2U,
        FACTORY_ITEM_NONE,
        FACTORY_ITEM_IRON_GEAR, 1U
    );
    run_recipe(
        FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE,
        FACTORY_ITEM_COPPER_PLATE, 1U,
        FACTORY_ITEM_NONE,
        FACTORY_ITEM_COPPER_WIRE, 2U
    );
    test_storage_new_items();

    if (failures != 0) {
        return 1;
    }
    (void)printf("All assembler recipe tests passed.\n");
    return 0;
}
