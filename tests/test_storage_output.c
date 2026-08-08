#include "foundation/simulation.h"
#include "power_fixture.h"

#include "logistics_endpoint_internal.h"
#include "simulation_internal.h"

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

static FactoryCommand place_storage(int32_t x, int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {x, y}}
    };
}

static FactoryCommand place_inserter(
    int32_t x,
    int32_t y,
    FactoryDirection facing
)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {x, y, facing}}
    };
}

static FactoryCommand set_output(
    FactoryEntityId storage_id,
    FactoryItemType item
)
{
    return (FactoryCommand){
        FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output = {storage_id, item}}
    };
}

static void test_configuration_and_buffer(void)
{
    FactoryWorld *world = factory_world_create(2U, 1U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryStorage state;
    FactoryCommand command;
    const FactoryCommandResult *result;

    submit(simulation, place_storage(0, 0));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_storage(simulation, 1U, &state));
    CHECK(state.configured_output_item == FACTORY_ITEM_NONE);
    CHECK(state.output_item == FACTORY_ITEM_NONE);
    CHECK(!state.output_occupied);

    simulation->storages.items[0].iron_plate_amount = 2U;
    simulation->storages.items[0].copper_plate_amount = 3U;
    submit(simulation, set_output(1U, FACTORY_ITEM_IRON_PLATE));
    CHECK(factory_simulation_get_pending_command_count(simulation) == 1U);
    CHECK(factory_simulation_get_storage(simulation, 1U, &state));
    CHECK(state.configured_output_item == FACTORY_ITEM_NONE);
    factory_simulation_tick(simulation);
    result = factory_simulation_get_command_result(simulation, 0U);
    CHECK(result->result == FACTORY_RESULT_OK);
    CHECK(result->entity_id == 1U);
    CHECK(result->previous_storage_output == FACTORY_ITEM_NONE);
    CHECK(result->new_storage_output == FACTORY_ITEM_IRON_PLATE);
    CHECK(factory_simulation_get_storage(simulation, 1U, &state));
    CHECK(state.configured_output_item == FACTORY_ITEM_IRON_PLATE);
    CHECK(state.iron_plate_amount == 1U);
    CHECK(state.copper_plate_amount == 3U);
    CHECK(state.output_occupied);
    CHECK(state.output_item == FACTORY_ITEM_IRON_PLATE);
    CHECK(factory_simulation_get_event_count(simulation) == 1U);
    CHECK(factory_simulation_get_event(simulation, 0U)->type
        == FACTORY_EVENT_STORAGE_OUTPUT_CHANGED);
    CHECK(factory_simulation_get_event(simulation, 0U)->entity_id == 1U);
    CHECK(factory_simulation_get_event(simulation, 0U)->item_type
        == FACTORY_ITEM_IRON_PLATE);
    CHECK(factory_simulation_get_event(simulation, 0U)->related_quantity
        == FACTORY_ITEM_NONE);

    submit(simulation, set_output(1U, FACTORY_ITEM_IRON_PLATE));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_storage(simulation, 1U, &state));
    CHECK(state.iron_plate_amount == 1U);
    CHECK(state.output_occupied);

    submit(simulation, set_output(1U, FACTORY_ITEM_COPPER_PLATE));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_STORAGE_OUTPUT_NOT_EMPTY);
    CHECK(factory_simulation_get_storage(simulation, 1U, &state));
    CHECK(state.configured_output_item == FACTORY_ITEM_IRON_PLATE);
    CHECK(state.copper_plate_amount == 3U);

    simulation->storages.items[0].output_item = FACTORY_ITEM_NONE;
    simulation->storages.items[0].output_occupied = false;
    submit(simulation, set_output(1U, FACTORY_ITEM_COPPER_PLATE));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_storage(simulation, 1U, &state));
    CHECK(state.output_item == FACTORY_ITEM_COPPER_PLATE);
    CHECK(state.copper_plate_amount == 2U);

    simulation->storages.items[0].output_item = FACTORY_ITEM_NONE;
    simulation->storages.items[0].output_occupied = false;
    simulation->storages.items[0].copper_plate_amount = 0U;
    submit(simulation, set_output(1U, FACTORY_ITEM_COPPER_PLATE));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_storage(simulation, 1U, &state));
    CHECK(state.configured_output_item == FACTORY_ITEM_COPPER_PLATE);
    CHECK(!state.output_occupied);

    submit(simulation, set_output(1U, FACTORY_ITEM_NONE));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_storage(simulation, 1U, &state));
    CHECK(state.configured_output_item == FACTORY_ITEM_NONE);
    CHECK(state.iron_plate_amount == 1U);

    command = set_output(1U, (FactoryItemType)99);
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    submit(simulation, set_output(99U, FACTORY_ITEM_IRON_GEAR));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_ENTITY_NOT_FOUND);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {1, 0, FACTORY_DIRECTION_EAST}}
    });
    factory_simulation_tick(simulation);
    submit(simulation, set_output(2U, FACTORY_ITEM_COPPER_WIRE));
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_UNSUPPORTED_ENTITY);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_endpoint_and_inserter_timing(void)
{
    FactoryWorld *world = factory_world_create(3U, 3U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryStorage source;
    FactoryStorage destination;
    FactoryInserter inserter;
    FactoryLogisticsEndpoint output = {
        1U, FACTORY_LOGISTICS_SLOT_STORAGE_OUTPUT
    };
    FactoryItemType item = FACTORY_ITEM_NONE;

    submit(simulation, place_storage(0, 0));
    submit(simulation, place_inserter(1, 0, FACTORY_DIRECTION_EAST));
    submit(simulation, place_storage(2, 0));
    CHECK(factory_test_submit_power_row(simulation, 3U, 1U));
    factory_simulation_tick(simulation);
    simulation->storages.items[0].iron_gear_amount = 3U;
    submit(simulation, set_output(1U, FACTORY_ITEM_IRON_GEAR));

    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_storage(simulation, 1U, &source));
    CHECK(source.iron_gear_amount == 2U);
    CHECK(source.output_item == FACTORY_ITEM_IRON_GEAR);
    CHECK(source.output_occupied);
    CHECK(factory_logistics_endpoint_peek(simulation, output, &item)
        == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(item == FACTORY_ITEM_IRON_GEAR);
    CHECK(factory_simulation_get_inserter(simulation, 2U, &inserter));
    CHECK(inserter.state == FACTORY_INSERTER_STATE_PICKING_UP);
    CHECK(inserter.held_item == FACTORY_ITEM_NONE);

    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_inserter(simulation, 2U, &inserter));
    CHECK(inserter.state == FACTORY_INSERTER_STATE_HOLDING);
    CHECK(inserter.held_item == FACTORY_ITEM_IRON_GEAR);
    CHECK(factory_simulation_get_storage(simulation, 1U, &source));
    CHECK(!source.output_occupied);
    CHECK(source.output_item == FACTORY_ITEM_NONE);

    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_storage(simulation, 1U, &source));
    CHECK(source.iron_gear_amount == 1U);
    CHECK(source.output_occupied);
    CHECK(factory_simulation_get_inserter(simulation, 2U, &inserter));
    CHECK(inserter.state == FACTORY_INSERTER_STATE_DROPPING);

    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_storage(simulation, 3U, &destination));
    CHECK(destination.iron_gear_amount == 1U);
    CHECK(factory_simulation_get_inserter(simulation, 2U, &inserter));
    CHECK(inserter.state == FACTORY_INSERTER_STATE_PICKING_UP);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_contention_and_conservation(void)
{
    FactoryWorld *world = factory_world_create(3U, 5U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryStorage storage;
    FactoryInserter low;
    FactoryInserter high;
    uint32_t copper_before = 4U * factory_item_copper_units(
        FACTORY_ITEM_COPPER_WIRE
    );

    submit(simulation, place_storage(1, 1));
    submit(simulation, place_inserter(2, 1, FACTORY_DIRECTION_EAST));
    submit(simulation, place_inserter(1, 2, FACTORY_DIRECTION_SOUTH));
    CHECK(factory_test_submit_power_row(simulation, 3U, 3U));
    factory_simulation_tick(simulation);
    simulation->storages.items[0].copper_wire_amount = 4U;
    submit(simulation, set_output(1U, FACTORY_ITEM_COPPER_WIRE));
    factory_simulation_tick(simulation);
    factory_simulation_tick(simulation);

    CHECK(factory_simulation_get_inserter(simulation, 2U, &low));
    CHECK(factory_simulation_get_inserter(simulation, 3U, &high));
    CHECK(low.held_item == FACTORY_ITEM_COPPER_WIRE);
    CHECK(low.state == FACTORY_INSERTER_STATE_HOLDING);
    CHECK(high.held_item == FACTORY_ITEM_NONE);
    CHECK(high.state == FACTORY_INSERTER_STATE_IDLE);
    CHECK(factory_simulation_get_storage(simulation, 1U, &storage));
    CHECK(storage.copper_wire_amount == 3U);
    CHECK(!storage.output_occupied);
    CHECK(
        storage.copper_wire_amount
            * factory_item_copper_units(FACTORY_ITEM_COPPER_WIRE)
        + (storage.output_occupied
            ? factory_item_copper_units(storage.output_item) : 0U)
        + factory_item_copper_units(low.held_item)
        + factory_item_copper_units(high.held_item)
        == copper_before
    );

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_demolition_requires_empty_buffer(void)
{
    FactoryWorld *world = factory_world_create(1U, 1U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);

    submit(simulation, place_storage(0, 0));
    factory_simulation_tick(simulation);
    simulation->storages.items[0].output_item = FACTORY_ITEM_COPPER_WIRE;
    simulation->storages.items[0].output_occupied = true;
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {1U}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    CHECK(factory_simulation_entity_is_valid(simulation, 1U));

    simulation->storages.items[0].output_item = FACTORY_ITEM_NONE;
    simulation->storages.items[0].output_occupied = false;
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {1U}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->result
        == FACTORY_RESULT_OK);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_duplicate_simulations(void)
{
    FactoryWorld *world_a = factory_world_create(3U, 3U);
    FactoryWorld *world_b = factory_world_create(3U, 3U);
    FactorySimulation *a =
        factory_simulation_create_with_construction_units(world_a, UINT32_MAX);
    FactorySimulation *b =
        factory_simulation_create_with_construction_units(world_b, UINT32_MAX);

    submit(a, place_storage(0, 0));
    submit(a, place_inserter(1, 0, FACTORY_DIRECTION_EAST));
    submit(a, place_storage(2, 0));
    submit(b, place_storage(0, 0));
    submit(b, place_inserter(1, 0, FACTORY_DIRECTION_EAST));
    submit(b, place_storage(2, 0));
    CHECK(factory_test_submit_power_row(a, 3U, 1U));
    CHECK(factory_test_submit_power_row(b, 3U, 1U));
    factory_simulation_tick(a);
    factory_simulation_tick(b);
    a->storages.items[0].electronic_component_amount = 3U;
    b->storages.items[0].electronic_component_amount = 3U;
    submit(a, set_output(1U, FACTORY_ITEM_ELECTRONIC_COMPONENT));
    submit(b, set_output(1U, FACTORY_ITEM_ELECTRONIC_COMPONENT));

    for (uint32_t tick = 0U; tick < 8U; ++tick) {
        FactoryStorage storage_a;
        FactoryStorage storage_b;
        FactoryInserter inserter_a;
        FactoryInserter inserter_b;

        factory_simulation_tick(a);
        factory_simulation_tick(b);
        CHECK(factory_simulation_get_storage(a, 1U, &storage_a));
        CHECK(factory_simulation_get_storage(b, 1U, &storage_b));
        CHECK(storage_a.electronic_component_amount
            == storage_b.electronic_component_amount);
        CHECK(storage_a.configured_output_item
            == storage_b.configured_output_item);
        CHECK(storage_a.output_item == storage_b.output_item);
        CHECK(storage_a.output_occupied == storage_b.output_occupied);
        CHECK(factory_simulation_get_inserter(a, 2U, &inserter_a));
        CHECK(factory_simulation_get_inserter(b, 2U, &inserter_b));
        CHECK(inserter_a.state == inserter_b.state);
        CHECK(inserter_a.held_item == inserter_b.held_item);
        CHECK(inserter_a.progress == inserter_b.progress);
        CHECK(factory_simulation_get_command_result_count(a)
            == factory_simulation_get_command_result_count(b));
        if (factory_simulation_get_command_result_count(a) != 0U) {
            CHECK(factory_simulation_get_command_result(a, 0U)->result
                == factory_simulation_get_command_result(b, 0U)->result);
        }
    }

    factory_simulation_destroy(a);
    factory_simulation_destroy(b);
    factory_world_destroy(world_a);
    factory_world_destroy(world_b);
}

int main(void)
{
    test_configuration_and_buffer();
    test_endpoint_and_inserter_timing();
    test_contention_and_conservation();
    test_demolition_requires_empty_buffer();
    test_duplicate_simulations();

    if (failures != 0) {
        return 1;
    }
    (void)printf("All storage output tests passed.\n");
    return 0;
}
