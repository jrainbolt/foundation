#include "logistics_endpoint_internal.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static FactoryLogisticsEndpoint endpoint(
    FactoryEntityId id,
    FactoryLogisticsSlot slot
)
{
    FactoryLogisticsEndpoint value = {id, slot};
    return value;
}

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

typedef struct {
    FactoryWorld *world;
    FactorySimulation *simulation;
} Fixture;

static Fixture create_fixture(void)
{
    Fixture fixture;

    fixture.world = factory_world_create(7U, 2U);
    CHECK(factory_world_add_resource(
        fixture.world, 0, 0, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    fixture.simulation = factory_simulation_create_with_construction_units(fixture.world, UINT32_MAX);
    submit(fixture.simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_SOUTH}}
    });
    submit(fixture.simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(fixture.simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_SPLITTER,
        {.place_splitter = {2, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(fixture.simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            3, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
        }}
    });
    submit(fixture.simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {4, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(fixture.simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {5, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(fixture.simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage = {6, 0}}
    });
    factory_simulation_tick(fixture.simulation);
    fixture.simulation->refineries.items[0].recipe_id =
        FACTORY_RECIPE_IRON_PLATE;
    CHECK(factory_assembler_configure_recipe(
        &fixture.simulation->assemblers.items[0],
        FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT
    ));
    return fixture;
}

static void destroy_fixture(Fixture *fixture)
{
    factory_simulation_destroy(fixture->simulation);
    factory_world_destroy(fixture->world);
}

static void clear_items(Fixture *fixture)
{
    FactorySimulation *simulation = fixture->simulation;

    simulation->extractors.items[0].output_item = FACTORY_ITEM_NONE;
    simulation->extractors.items[0].output_amount = 0U;
    simulation->belts.items[0].item = FACTORY_ITEM_NONE;
    simulation->belts.items[0].movement_progress = 0U;
    simulation->splitters.items[0].item = FACTORY_ITEM_NONE;
    simulation->refineries.items[0].input_item = FACTORY_ITEM_NONE;
    simulation->refineries.items[0].input_amount = 0U;
    simulation->refineries.items[0].output_item = FACTORY_ITEM_NONE;
    simulation->refineries.items[0].output_amount = 0U;
    simulation->assemblers.items[0].input_slots[0].count = 0U;
    simulation->assemblers.items[0].input_slots[1].count = 0U;
    simulation->assemblers.items[0].output_item = FACTORY_ITEM_NONE;
    simulation->assemblers.items[0].output_amount = 0U;
    simulation->inserters.items[0].held_item = FACTORY_ITEM_NONE;
    simulation->inserters.items[0].held_amount = 0U;
    simulation->inserters.items[0].state = FACTORY_INSERTER_STATE_IDLE;
    simulation->storages.items[0].iron_ore_amount = 0U;
    simulation->storages.items[0].iron_plate_amount = 0U;
    simulation->storages.items[0].copper_ore_amount = 0U;
    simulation->storages.items[0].copper_plate_amount = 0U;
    simulation->storages.items[0].electronic_component_amount = 0U;
}

static void test_validation_and_read_only_queries(void)
{
    Fixture fixture = create_fixture();
    FactorySimulation *simulation = fixture.simulation;
    FactoryItemType item = FACTORY_ITEM_NONE;
    FactoryEntityId unknown;

    clear_items(&fixture);
    simulation->extractors.items[0].output_item = FACTORY_ITEM_IRON_ORE;
    simulation->extractors.items[0].output_amount = 1U;
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(1U, FACTORY_LOGISTICS_SLOT_OUTPUT), &item
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(item == FACTORY_ITEM_IRON_ORE);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(1U, FACTORY_LOGISTICS_SLOT_OUTPUT), &item
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(simulation->extractors.items[0].output_amount == 1U);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(1U, FACTORY_LOGISTICS_SLOT_INPUT), &item
    ) == FACTORY_LOGISTICS_RESULT_INVALID_SLOT);

    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN), &item
    ) == FACTORY_LOGISTICS_RESULT_EMPTY);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);

    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(3U, FACTORY_LOGISTICS_SLOT_SPLITTER_INPUT),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_peek(
        simulation,
        endpoint(3U, FACTORY_LOGISTICS_SLOT_SPLITTER_LEFT_OUTPUT),
        &item
    ) == FACTORY_LOGISTICS_RESULT_EMPTY);
    CHECK(factory_logistics_endpoint_peek(
        simulation,
        endpoint(3U, FACTORY_LOGISTICS_SLOT_SPLITTER_RIGHT_OUTPUT),
        &item
    ) == FACTORY_LOGISTICS_RESULT_EMPTY);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(3U, FACTORY_LOGISTICS_SLOT_OUTPUT), &item
    ) == FACTORY_LOGISTICS_RESULT_INVALID_SLOT);

    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(4U, FACTORY_LOGISTICS_SLOT_INPUT),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(4U, FACTORY_LOGISTICS_SLOT_INPUT),
        FACTORY_ITEM_COPPER_ORE
    ) == FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(4U, FACTORY_LOGISTICS_SLOT_OUTPUT), &item
    ) == FACTORY_LOGISTICS_RESULT_EMPTY);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(4U, FACTORY_LOGISTICS_SLOT_MAIN), &item
    ) == FACTORY_LOGISTICS_RESULT_INVALID_SLOT);

    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0),
        FACTORY_ITEM_IRON_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1),
        FACTORY_ITEM_COPPER_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(5U, FACTORY_LOGISTICS_SLOT_OUTPUT), &item
    ) == FACTORY_LOGISTICS_RESULT_EMPTY);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0),
        FACTORY_ITEM_COPPER_PLATE
    ) == FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM);

    CHECK(factory_logistics_endpoint_peek(
        simulation,
        endpoint(6U, FACTORY_LOGISTICS_SLOT_INSERTER_HELD),
        &item
    ) == FACTORY_LOGISTICS_RESULT_EMPTY);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(7U, FACTORY_LOGISTICS_SLOT_STORAGE_INPUT),
        FACTORY_ITEM_ELECTRONIC_COMPONENT
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(7U, FACTORY_LOGISTICS_SLOT_OUTPUT), &item
    ) == FACTORY_LOGISTICS_RESULT_INVALID_SLOT);
    CHECK(factory_logistics_endpoint_can_accept(
        simulation,
        endpoint(7U, FACTORY_LOGISTICS_SLOT_STORAGE_INPUT),
        FACTORY_ITEM_NONE
    ) == FACTORY_LOGISTICS_RESULT_INVALID_ITEM);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(999U, FACTORY_LOGISTICS_SLOT_MAIN), &item
    ) == FACTORY_LOGISTICS_RESULT_INVALID_ENTITY);

    unknown = factory_entity_create(simulation->entities);
    CHECK(unknown != 0U);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(unknown, FACTORY_LOGISTICS_SLOT_MAIN), &item
    ) == FACTORY_LOGISTICS_RESULT_INVALID_SLOT);
    factory_entity_destroy(simulation->entities, unknown);
    CHECK(factory_logistics_endpoint_peek(
        simulation, endpoint(unknown, FACTORY_LOGISTICS_SLOT_MAIN), &item
    ) == FACTORY_LOGISTICS_RESULT_INVALID_ENTITY);

    destroy_fixture(&fixture);
}

static void test_remove_insert_and_failure_atomicity(void)
{
    Fixture fixture = create_fixture();
    FactorySimulation *simulation = fixture.simulation;

    clear_items(&fixture);
    simulation->extractors.items[0].output_item = FACTORY_ITEM_IRON_ORE;
    simulation->extractors.items[0].output_amount = 1U;
    CHECK(factory_logistics_endpoint_remove(
        simulation,
        endpoint(1U, FACTORY_LOGISTICS_SLOT_OUTPUT),
        FACTORY_ITEM_COPPER_ORE
    ) == FACTORY_LOGISTICS_RESULT_STATE_MISMATCH);
    CHECK(simulation->extractors.items[0].output_amount == 1U);
    CHECK(factory_logistics_endpoint_remove(
        simulation,
        endpoint(1U, FACTORY_LOGISTICS_SLOT_OUTPUT),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(simulation->extractors.items[0].output_item == FACTORY_ITEM_NONE);
    CHECK(factory_logistics_endpoint_remove(
        simulation,
        endpoint(1U, FACTORY_LOGISTICS_SLOT_OUTPUT),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_EMPTY);

    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(simulation->belts.items[0].movement_progress == 0U);
    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        FACTORY_ITEM_COPPER_ORE
    ) == FACTORY_LOGISTICS_RESULT_BLOCKED);
    CHECK(simulation->belts.items[0].item == FACTORY_ITEM_IRON_ORE);
    CHECK(factory_logistics_endpoint_remove(
        simulation,
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);

    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(3U, FACTORY_LOGISTICS_SLOT_SPLITTER_INPUT),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_remove(
        simulation,
        endpoint(3U, FACTORY_LOGISTICS_SLOT_SPLITTER_LEFT_OUTPUT),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);

    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(4U, FACTORY_LOGISTICS_SLOT_INPUT),
        FACTORY_ITEM_COPPER_ORE
    ) == FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM);
    CHECK(simulation->refineries.items[0].input_item == FACTORY_ITEM_NONE);
    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(4U, FACTORY_LOGISTICS_SLOT_INPUT),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    simulation->refineries.items[0].output_item =
        FACTORY_ITEM_IRON_PLATE;
    simulation->refineries.items[0].output_amount = 1U;
    CHECK(factory_logistics_endpoint_remove(
        simulation,
        endpoint(4U, FACTORY_LOGISTICS_SLOT_OUTPUT),
        FACTORY_ITEM_IRON_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);

    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0),
        FACTORY_ITEM_IRON_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1),
        FACTORY_ITEM_COPPER_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(simulation->assemblers.items[0].input_slots[0].count == 1U);
    CHECK(simulation->assemblers.items[0].input_slots[1].count == 1U);
    simulation->assemblers.items[0].output_item =
        FACTORY_ITEM_ELECTRONIC_COMPONENT;
    simulation->assemblers.items[0].output_amount = 1U;
    CHECK(factory_logistics_endpoint_remove(
        simulation,
        endpoint(5U, FACTORY_LOGISTICS_SLOT_OUTPUT),
        FACTORY_ITEM_ELECTRONIC_COMPONENT
    ) == FACTORY_LOGISTICS_RESULT_OK);

    simulation->inserters.items[0].state =
        FACTORY_INSERTER_STATE_PICKING_UP;
    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(6U, FACTORY_LOGISTICS_SLOT_INSERTER_HELD),
        FACTORY_ITEM_IRON_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    simulation->inserters.items[0].state = FACTORY_INSERTER_STATE_DROPPING;
    CHECK(factory_logistics_endpoint_remove(
        simulation,
        endpoint(6U, FACTORY_LOGISTICS_SLOT_INSERTER_HELD),
        FACTORY_ITEM_IRON_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);

    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(7U, FACTORY_LOGISTICS_SLOT_STORAGE_INPUT),
        FACTORY_ITEM_ELECTRONIC_COMPONENT
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(simulation->storages.items[0].electronic_component_amount == 1U);
    simulation->storages.items[0].total_capacity = 1U;
    CHECK(factory_logistics_endpoint_insert(
        simulation,
        endpoint(7U, FACTORY_LOGISTICS_SLOT_STORAGE_INPUT),
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_BLOCKED);
    CHECK(simulation->storages.items[0].iron_ore_amount == 0U);

    clear_items(&fixture);
    simulation->belts.items[0].item = FACTORY_ITEM_IRON_PLATE;
    simulation->assemblers.items[0].input_slots[0].count = 1U;
    CHECK(factory_logistics_endpoint_transfer(
        simulation,
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        endpoint(5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0),
        FACTORY_ITEM_IRON_PLATE
    ) == FACTORY_LOGISTICS_RESULT_BLOCKED);
    CHECK(simulation->belts.items[0].item == FACTORY_ITEM_IRON_PLATE);
    CHECK(simulation->assemblers.items[0].input_slots[0].count == 1U);

    simulation->assemblers.items[0].input_slots[0].count = 0U;
    CHECK(factory_logistics_endpoint_transfer(
        simulation,
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        endpoint(5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0),
        FACTORY_ITEM_COPPER_PLATE
    ) == FACTORY_LOGISTICS_RESULT_STATE_MISMATCH);
    CHECK(simulation->belts.items[0].item == FACTORY_ITEM_IRON_PLATE);
    CHECK(simulation->assemblers.items[0].input_slots[0].count == 0U);
    CHECK(factory_logistics_endpoint_transfer(
        simulation,
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        endpoint(5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0),
        FACTORY_ITEM_IRON_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(simulation->belts.items[0].item == FACTORY_ITEM_NONE);
    CHECK(simulation->assemblers.items[0].input_slots[0].count == 1U);

    destroy_fixture(&fixture);
}

static void test_endpoint_conflict_identity(void)
{
    FactoryLogisticsEndpoint iron = endpoint(
        5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0
    );
    FactoryLogisticsEndpoint copper = endpoint(
        5U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1
    );

    CHECK(factory_logistics_endpoint_equal(iron, iron));
    CHECK(!factory_logistics_endpoint_equal(iron, copper));
    CHECK(factory_logistics_endpoint_equal(
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN)
    ));
    CHECK(!factory_logistics_endpoint_equal(
        endpoint(2U, FACTORY_LOGISTICS_SLOT_MAIN),
        endpoint(3U, FACTORY_LOGISTICS_SLOT_SPLITTER_INPUT)
    ));
}

int main(void)
{
    test_validation_and_read_only_queries();
    test_remove_insert_and_failure_atomicity();
    test_endpoint_conflict_identity();

    if (failures != 0) {
        (void)fprintf(stderr, "%d endpoint test(s) failed\n", failures);
        return 1;
    }
    (void)printf("logistics endpoint tests passed\n");
    return 0;
}
