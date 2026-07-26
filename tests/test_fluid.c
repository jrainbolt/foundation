#include "foundation/presentation.h"
#include "foundation/snapshot.h"
#include "fluid_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static void place_tank(FactorySimulation *simulation, int32_t x, int32_t y)
{
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_FLUID_TANK, {.place_fluid_tank = {x, y}}
    });
}

static FactoryResult command_result(const FactorySimulation *simulation)
{
    const FactoryCommandResult *result =
        factory_simulation_get_command_result(simulation, 0U);
    CHECK(result != NULL);
    return result == NULL ? FACTORY_RESULT_INVALID_ARGUMENT : result->result;
}

static void inspect(
    const FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryFluidType type,
    FactoryFluidQuantity quantity
)
{
    FactoryFluidStorageInspection storage = {0};
    CHECK(factory_simulation_get_fluid_storage(simulation, id, &storage)
        == FACTORY_RESULT_OK);
    CHECK(storage.owner_entity_id == id);
    CHECK(storage.fluid_type == type);
    CHECK(storage.quantity == quantity);
    CHECK(storage.capacity == FACTORY_FLUID_TANK_CAPACITY);
    CHECK(storage.free_capacity == storage.capacity - storage.quantity);
}

static void test_definitions_and_transactional_primitives(void)
{
    const FactoryFluidDefinition *water;
    FactoryFluidDefinition invalid = {
        FACTORY_FLUID_WATER, "", FACTORY_FLUID_CLASS_AQUEOUS
    };
    FactoryFluidStorage source = {
        1U, 0, 0, FACTORY_FLUID_CLASS_AQUEOUS,
        FACTORY_FLUID_WATER, 8U, 10U
    };
    FactoryFluidStorage destination = {
        2U, 1, 0, FACTORY_FLUID_CLASS_AQUEOUS,
        FACTORY_FLUID_NONE, 0U, 5U
    };
    FactoryFluidType moved = FACTORY_FLUID_NONE;

    CHECK(factory_fluid_definition_count() == 1U);
    water = factory_fluid_definition_at(0U);
    CHECK(water != NULL && factory_fluid_definition_is_valid(water));
    CHECK(water != NULL && water->fluid_type == FACTORY_FLUID_WATER);
    CHECK(strcmp(factory_fluid_name(FACTORY_FLUID_WATER), "water") == 0);
    CHECK(factory_fluid_definition_at(1U) == NULL);
    CHECK(factory_fluid_definition_get(FACTORY_FLUID_NONE) == NULL);
    CHECK(!factory_fluid_definition_is_valid(&invalid));

    CHECK(factory_fluid_storage_transfer(
        &source, &destination, 6U, &moved)
        == FACTORY_RESULT_FLUID_CAPACITY_EXCEEDED);
    CHECK(source.quantity == 8U && destination.quantity == 0U);
    CHECK(factory_fluid_storage_transfer(
        &source, &destination, 5U, &moved) == FACTORY_RESULT_OK);
    CHECK(moved == FACTORY_FLUID_WATER);
    CHECK(source.quantity == 3U && destination.quantity == 5U);
    CHECK(factory_fluid_storage_transfer(
        &source, &destination, 1U, &moved)
        == FACTORY_RESULT_FLUID_CAPACITY_EXCEEDED);
    CHECK(source.quantity == 3U && destination.quantity == 5U);
    CHECK(factory_fluid_storage_remove(&source, 4U, &moved)
        == FACTORY_RESULT_INSUFFICIENT_FLUID);
    CHECK(source.quantity == 3U);
    CHECK(factory_fluid_storage_remove(&source, 3U, &moved)
        == FACTORY_RESULT_OK);
    CHECK(source.quantity == 0U && source.fluid_type == FACTORY_FLUID_NONE);
}

static void test_commands_events_presentation_and_demolition(void)
{
    FactoryWorld *world = factory_world_create(4U, 2U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactoryPresentationSnapshot *presentation =
        factory_presentation_snapshot_create();
    const FactoryEvent *event;
    const FactoryPresentationEntity *entity;

    place_tank(simulation, 0, 0);
    place_tank(simulation, 1, 0);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    inspect(simulation, 1U, FACTORY_FLUID_NONE, 0U);
    CHECK(factory_simulation_submit_fluid_insert(
        simulation, 1U, FACTORY_FLUID_WATER, 4000U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(command_result(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(simulation) == 1U);
    event = factory_simulation_get_event(simulation, 0U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_FLUID_INSERTED);
    CHECK(event != NULL && event->tick == 1U && event->entity_id == 1U);
    CHECK(event != NULL && event->fluid_type == FACTORY_FLUID_WATER);
    CHECK(event != NULL && event->quantity == 4000U);

    CHECK(factory_simulation_submit_fluid_transfer(
        simulation, 1U, 2U, 1000U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    inspect(simulation, 1U, FACTORY_FLUID_WATER, 3000U);
    inspect(simulation, 2U, FACTORY_FLUID_WATER, 1000U);
    event = factory_simulation_get_event(simulation, 0U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_FLUID_TRANSFERRED);
    CHECK(event != NULL && event->entity_id == 1U
        && event->related_entity_id == 2U && event->quantity == 1000U);

    CHECK(factory_simulation_submit_fluid_remove(simulation, 2U, 250U)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    event = factory_simulation_get_event(simulation, 0U);
    CHECK(event != NULL && event->type == FACTORY_EVENT_FLUID_REMOVED);
    CHECK(event != NULL && event->fluid_type == FACTORY_FLUID_WATER
        && event->quantity == 250U);

    CHECK(factory_simulation_submit_fluid_transfer(
        simulation, 2U, 1U, 5000U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(command_result(simulation) == FACTORY_RESULT_INSUFFICIENT_FLUID);
    CHECK(factory_simulation_get_event_count(simulation) == 0U);
    inspect(simulation, 1U, FACTORY_FLUID_WATER, 3000U);
    inspect(simulation, 2U, FACTORY_FLUID_WATER, 750U);

    CHECK(factory_presentation_snapshot_rebuild(presentation, simulation)
        == FACTORY_RESULT_OK);
    entity = factory_presentation_snapshot_get_entity(presentation, 0U);
    CHECK(entity != NULL && entity->entity_type
        == FACTORY_ENTITY_TYPE_FLUID_TANK);
    CHECK(entity != NULL
        && entity->data.fluid_storage.fluid_type == FACTORY_FLUID_WATER
        && entity->data.fluid_storage.quantity == 3000U
        && entity->data.fluid_storage.capacity
            == FACTORY_FLUID_TANK_CAPACITY);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {1U}}
    });
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(command_result(simulation) == FACTORY_RESULT_ENTITY_HAS_MATERIAL);

    factory_presentation_snapshot_destroy(presentation);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static bool event_equal(const FactoryEvent *a, const FactoryEvent *b)
{
    return a != NULL && b != NULL && a->type == b->type
        && a->tick == b->tick && a->entity_id == b->entity_id
        && a->related_entity_id == b->related_entity_id
        && a->entity_type == b->entity_type
        && a->item_type == b->item_type
        && a->fluid_type == b->fluid_type && a->quantity == b->quantity;
}

static void test_snapshot_continuation(void)
{
    FactoryWorld *world = factory_world_create(3U, 1U);
    FactorySimulation *a =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    FactorySimulation *b = NULL;
    FactorySnapshotBuffer checkpoint = {0};
    FactorySnapshotBuffer after_a = {0};
    FactorySnapshotBuffer after_b = {0};

    place_tank(a, 0, 0);
    place_tank(a, 1, 0);
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_fluid_insert(
        a, 1U, FACTORY_FLUID_WATER, 5000U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(a, &checkpoint)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        checkpoint.data, checkpoint.size, &b) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(b) == 0U);
    inspect(b, 1U, FACTORY_FLUID_WATER, 5000U);

    CHECK(factory_simulation_submit_fluid_transfer(a, 1U, 2U, 1250U)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_fluid_transfer(b, 1U, 2U, 1250U)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(a) == 1U);
    CHECK(event_equal(
        factory_simulation_get_event(a, 0U),
        factory_simulation_get_event(b, 0U)));
    CHECK(factory_simulation_create_snapshot(a, &after_a)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(b, &after_b)
        == FACTORY_RESULT_OK);
    CHECK(after_a.size == after_b.size);
    CHECK(after_a.size == after_b.size
        && memcmp(after_a.data, after_b.data, after_a.size) == 0);

    factory_snapshot_buffer_destroy(&after_b);
    factory_snapshot_buffer_destroy(&after_a);
    factory_snapshot_buffer_destroy(&checkpoint);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world);
}

int main(void)
{
    test_definitions_and_transactional_primitives();
    test_commands_events_presentation_and_demolition();
    test_snapshot_continuation();
    if (failures != 0) {
        (void)fprintf(stderr, "%d fluid test(s) failed\n", failures);
        return 1;
    }
    (void)puts("fluid tests passed");
    return 0;
}
