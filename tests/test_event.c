#include "foundation/snapshot.h"
#include "logistics_endpoint_internal.h"
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

static bool event_equal(const FactoryEvent *a, const FactoryEvent *b)
{
    return a != NULL && b != NULL
        && a->type == b->type && a->tick == b->tick
        && a->entity_id == b->entity_id
        && a->related_entity_id == b->related_entity_id
        && a->entity_type == b->entity_type
        && a->item_type == b->item_type
        && a->fluid_type == b->fluid_type
        && a->related_fluid_type == b->related_fluid_type
        && a->quantity == b->quantity
        && a->related_quantity == b->related_quantity;
}

static bool batch_equal(
    const FactorySimulation *a, const FactorySimulation *b
)
{
    size_t count = factory_simulation_get_event_count(a);
    if (count != factory_simulation_get_event_count(b)) return false;
    for (size_t i = 0U; i < count; ++i)
        if (!event_equal(
                factory_simulation_get_event(a, i),
                factory_simulation_get_event(b, i))) return false;
    return true;
}

static bool snapshot_equal(
    const FactorySimulation *a, const FactorySimulation *b
)
{
    FactorySnapshotBuffer x = {0};
    FactorySnapshotBuffer y = {0};
    bool equal = false;
    if (factory_simulation_create_snapshot(a, &x) == FACTORY_RESULT_OK
        && factory_simulation_create_snapshot(b, &y) == FACTORY_RESULT_OK)
        equal = x.size == y.size && memcmp(x.data, y.data, x.size) == 0;
    factory_snapshot_buffer_destroy(&y);
    factory_snapshot_buffer_destroy(&x);
    return equal;
}

static void test_api_construction_and_demolition(void)
{
    FactoryWorld *world = factory_world_create(4U, 1U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    const FactoryEvent *event;

    CHECK(factory_simulation_get_event_count(simulation) == 0U);
    CHECK(factory_simulation_get_event(simulation, 0U) == NULL);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {1, 0}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 2U);
    event = factory_simulation_get_event(simulation, 0U);
    CHECK(event != NULL
        && event->type == FACTORY_EVENT_ENTITY_CONSTRUCTED);
    CHECK(event != NULL && event->tick == 0U && event->entity_id == 1U);
    CHECK(event != NULL && event->entity_type == FACTORY_ENTITY_TYPE_BELT);
    event = factory_simulation_get_event(simulation, 1U);
    CHECK(event != NULL && event->entity_id == 2U);
    CHECK(event != NULL && event->entity_type == FACTORY_ENTITY_TYPE_STORAGE);
    CHECK(factory_simulation_get_event(simulation, 2U) == NULL);
    factory_simulation_clear_events(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 0U);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {1U}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {99U}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 1U);
    event = factory_simulation_get_event(simulation, 0U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_ENTITY_DEMOLISHED);
    CHECK(event != NULL && event->tick == 1U && event->entity_id == 1U);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 0U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_extractor_production_transfer_and_blocking(void)
{
    FactoryWorld *world = factory_world_create(5U, 3U);
    FactorySimulation *simulation;
    const FactoryEvent *event;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 3U
    ) == FACTORY_RESULT_OK);
    simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 1 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 2 */
    factory_simulation_tick(simulation);
    for (uint32_t step = 0U; step < 20U; ++step) {
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_event_count(simulation) == 0U);
    }
    submit(simulation, pole(1, 1));                         /* 3 */
    submit(simulation, generator(1, 2));                    /* 4 */
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 4U);
    CHECK(factory_simulation_get_event(simulation, 2U)->type
        == FACTORY_EVENT_FUEL_IGNITED);
    CHECK(factory_simulation_get_event(simulation, 3U)->type
        == FACTORY_EVENT_POWER_GAINED);
    for (uint32_t step = 1U; step < 20U; ++step) {
        factory_simulation_tick(simulation);
        if (step < 19U)
            CHECK(factory_simulation_get_event_count(simulation) == 0U);
    }
    CHECK(factory_simulation_get_event_count(simulation) == 2U);
    event = factory_simulation_get_event(simulation, 0U);
    CHECK(event != NULL
        && event->type == FACTORY_EVENT_PRODUCTION_COMPLETED);
    CHECK(event != NULL && event->entity_id == 1U);
    CHECK(event != NULL && event->item_type == FACTORY_ITEM_IRON_ORE);
    CHECK(event != NULL && event->quantity == 1U);
    event = factory_simulation_get_event(simulation, 1U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_ITEM_TRANSFERRED);
    CHECK(event != NULL && event->entity_id == 1U);
    CHECK(event != NULL && event->related_entity_id == 2U);
    CHECK(event != NULL && event->item_type == FACTORY_ITEM_IRON_ORE);
    CHECK(event != NULL && event->quantity == 1U);

    for (uint32_t step = 0U; step < 20U; ++step) {
        factory_simulation_tick(simulation);
        if (step < 19U)
            CHECK(factory_simulation_get_event_count(simulation) == 0U);
    }
    CHECK(factory_simulation_get_event_count(simulation) == 1U);
    event = factory_simulation_get_event(simulation, 0U);
    CHECK(event != NULL
        && event->type == FACTORY_EVENT_PRODUCTION_COMPLETED);
    for (uint32_t step = 0U; step < 20U; ++step) {
        factory_simulation_tick(simulation);
        CHECK(factory_simulation_get_event_count(simulation) == 0U);
    }
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_refinery_and_assembler_completion(void)
{
    FactoryWorld *world = factory_world_create(7U, 4U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    bool saw_refinery = false;
    bool saw_assembler = false;

    submit(simulation, pole(3, 2));                         /* 1 */
    submit(simulation, generator(3, 3));                    /* 2 */
    submit(simulation, (FactoryCommand){                    /* 3 */
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            1, 1, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
        }}
    });
    submit(simulation, (FactoryCommand){                    /* 4 */
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {5, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {3U, FACTORY_RECIPE_IRON_PLATE}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe = {
            4U, FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT
        }}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_logistics_endpoint_insert(
        simulation,
        (FactoryLogisticsEndpoint){3U, FACTORY_LOGISTICS_SLOT_INPUT},
        FACTORY_ITEM_IRON_ORE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(
        simulation,
        (FactoryLogisticsEndpoint){
            4U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_0
        }, FACTORY_ITEM_IRON_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(
        simulation,
        (FactoryLogisticsEndpoint){
            4U, FACTORY_LOGISTICS_SLOT_ASSEMBLER_INPUT_1
        }, FACTORY_ITEM_COPPER_PLATE
    ) == FACTORY_LOGISTICS_RESULT_OK);
    for (uint32_t step = 0U; step < 30U; ++step) {
        factory_simulation_tick(simulation);
        for (size_t i = 0U;
            i < factory_simulation_get_event_count(simulation); ++i) {
            const FactoryEvent *event =
                factory_simulation_get_event(simulation, i);
            if (event->type != FACTORY_EVENT_PRODUCTION_COMPLETED) continue;
            if (event->entity_id == 3U) {
                CHECK(event->item_type == FACTORY_ITEM_IRON_PLATE);
                CHECK(event->quantity == 1U);
                saw_refinery = true;
            } else if (event->entity_id == 4U) {
                CHECK(event->item_type
                    == FACTORY_ITEM_ELECTRONIC_COMPONENT);
                CHECK(event->quantity == 1U);
                saw_assembler = true;
            }
        }
    }
    CHECK(saw_refinery && saw_assembler);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_multiple_transfer_order(void)
{
    FactoryWorld *world = factory_world_create(4U, 5U);
    FactorySimulation *simulation;
    const FactoryEvent *event;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 1U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 0, 2, FACTORY_RESOURCE_COPPER, 1U
    ) == FACTORY_RESULT_OK);
    simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, (FactoryCommand){                    /* 1 */
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){                    /* 2 */
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){                    /* 3 */
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 2, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){                    /* 4 */
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 2, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, pole(1, 3));                         /* 5 */
    submit(simulation, generator(1, 4));                    /* 6 */
    for (uint32_t step = 0U; step < 20U; ++step)
        factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 4U);
    event = factory_simulation_get_event(simulation, 0U);
    CHECK(event != NULL && event->type
        == FACTORY_EVENT_PRODUCTION_COMPLETED && event->entity_id == 1U);
    event = factory_simulation_get_event(simulation, 1U);
    CHECK(event != NULL && event->type
        == FACTORY_EVENT_PRODUCTION_COMPLETED && event->entity_id == 3U);
    event = factory_simulation_get_event(simulation, 2U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_ITEM_TRANSFERRED);
    CHECK(event != NULL && event->entity_id == 1U
        && event->related_entity_id == 2U);
    event = factory_simulation_get_event(simulation, 3U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_ITEM_TRANSFERRED);
    CHECK(event != NULL && event->entity_id == 3U
        && event->related_entity_id == 4U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_power_transitions(void)
{
    FactoryWorld *world = factory_world_create(8U, 4U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    const FactoryEvent *event;

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {2, 1, FACTORY_DIRECTION_EAST}}
    });                                                     /* 1 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {3, 1, FACTORY_DIRECTION_EAST}}
    });                                                     /* 2 */
    submit(simulation, pole(2, 2));                         /* 3 */
    submit(simulation, generator(3, 2));                    /* 4 */
    factory_simulation_tick(simulation);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 0U);
    {
        FactoryBurner *burner =
            factory_burner_store_find_mutable(&simulation->burners, 4U);
        burner->inventory_item = FACTORY_ITEM_NONE;
        burner->inventory_quantity = 0U;
        burner->current_fuel_item = FACTORY_ITEM_NONE;
        burner->remaining_burn_ticks = 0U;
        burner->released_energy = 0U;
    }
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {4U}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 3U);
    event = factory_simulation_get_event(simulation, 1U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_POWER_LOST);
    CHECK(event != NULL && event->entity_id == 1U);
    event = factory_simulation_get_event(simulation, 2U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_POWER_LOST);
    CHECK(event != NULL && event->entity_id == 2U);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 0U);
    submit(simulation, generator(3, 2));                    /* 5 */
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 4U);
    event = factory_simulation_get_event(simulation, 2U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_POWER_GAINED);
    CHECK(event != NULL && event->entity_id == 1U);
    event = factory_simulation_get_event(simulation, 3U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_POWER_GAINED);
    CHECK(event != NULL && event->entity_id == 2U);
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_event_count(simulation) == 0U);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_snapshot_exclusion_and_continuation(void)
{
    FactoryWorld *world = factory_world_create(5U, 3U);
    FactorySimulation *a;
    FactorySimulation *b = NULL;
    FactorySimulation *rejected = NULL;
    FactorySnapshotBuffer before_clear = {0};
    FactorySnapshotBuffer after_clear = {0};
    uint8_t saved;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 5U
    ) == FACTORY_RESULT_OK);
    a = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(a, pole(1, 1));
    submit(a, generator(1, 2));
    factory_simulation_tick(a);
    CHECK(factory_simulation_get_event_count(a) == 4U);
    CHECK(factory_simulation_create_snapshot(a, &before_clear)
        == FACTORY_RESULT_OK);
    factory_simulation_clear_events(a);
    CHECK(factory_simulation_create_snapshot(a, &after_clear)
        == FACTORY_RESULT_OK);
    CHECK(before_clear.size == after_clear.size);
    CHECK(memcmp(
        before_clear.data, after_clear.data, before_clear.size
    ) == 0);
    CHECK(factory_simulation_load_snapshot(
        before_clear.data, before_clear.size, &b
    ) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(b) == 0U);

    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {4, 0, FACTORY_DIRECTION_WEST}}
    });
    submit(b, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {4, 0, FACTORY_DIRECTION_WEST}}
    });
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(a) == 1U);
    CHECK(batch_equal(a, b));
    saved = before_clear.data[0];
    before_clear.data[0] ^= 0xffU;
    CHECK(factory_simulation_load_snapshot(
        before_clear.data, before_clear.size, &rejected
    ) == FACTORY_RESULT_SNAPSHOT_INVALID_MAGIC);
    before_clear.data[0] = saved;
    CHECK(rejected == NULL);
    CHECK(factory_simulation_get_event_count(a) == 1U);
    CHECK(batch_equal(a, b));

    for (uint32_t step = 0U; step < 45U; ++step) {
        factory_simulation_tick(a);
        factory_simulation_tick(b);
        CHECK(batch_equal(a, b));
        CHECK(snapshot_equal(a, b));
    }

    factory_snapshot_buffer_destroy(&after_clear);
    factory_snapshot_buffer_destroy(&before_clear);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world);
}

int main(void)
{
    test_api_construction_and_demolition();
    test_extractor_production_transfer_and_blocking();
    test_refinery_and_assembler_completion();
    test_multiple_transfer_order();
    test_power_transitions();
    test_snapshot_exclusion_and_continuation();
    if (failures != 0) return 1;
    (void)printf("All event tests passed.\n");
    return 0;
}
