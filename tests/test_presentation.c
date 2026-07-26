#include "foundation/presentation.h"
#include "foundation/snapshot.h"

#include "assembler_internal.h"
#include "presentation_internal.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static bool entity_equal(
    const FactoryPresentationEntity *a,
    const FactoryPresentationEntity *b
)
{
    size_t i;
    if (a == NULL || b == NULL
        || a->entity_id != b->entity_id
        || a->entity_type != b->entity_type
        || a->x != b->x || a->y != b->y
        || a->direction != b->direction
        || a->status != b->status || a->powered != b->powered) return false;
    switch (a->entity_type) {
        case FACTORY_ENTITY_TYPE_EXTRACTOR:
            return a->data.extractor.resource_type
                    == b->data.extractor.resource_type
                && a->data.extractor.produced_item
                    == b->data.extractor.produced_item
                && a->data.extractor.progress == b->data.extractor.progress
                && a->data.extractor.duration == b->data.extractor.duration
                && a->data.extractor.output_item
                    == b->data.extractor.output_item
                && a->data.extractor.output_quantity
                    == b->data.extractor.output_quantity
                && a->data.extractor.can_progress
                    == b->data.extractor.can_progress;
        case FACTORY_ENTITY_TYPE_BELT:
            return a->data.belt.item == b->data.belt.item
                && a->data.belt.quantity == b->data.belt.quantity
                && a->data.belt.movement_progress
                    == b->data.belt.movement_progress
                && a->data.belt.movement_duration
                    == b->data.belt.movement_duration;
        case FACTORY_ENTITY_TYPE_STORAGE:
            for (i = 0U; i < FACTORY_PRESENTATION_STORAGE_ITEM_COUNT; ++i)
                if (a->data.storage.item_quantities[i]
                    != b->data.storage.item_quantities[i]) return false;
            return a->data.storage.total_capacity
                    == b->data.storage.total_capacity
                && a->data.storage.configured_output_item
                    == b->data.storage.configured_output_item
                && a->data.storage.output_item
                    == b->data.storage.output_item
                && a->data.storage.output_quantity
                    == b->data.storage.output_quantity;
        case FACTORY_ENTITY_TYPE_REFINERY:
            return a->data.refinery.recipe_id == b->data.refinery.recipe_id
                && a->data.refinery.input_item
                    == b->data.refinery.input_item
                && a->data.refinery.input_quantity
                    == b->data.refinery.input_quantity
                && a->data.refinery.output_item
                    == b->data.refinery.output_item
                && a->data.refinery.output_quantity
                    == b->data.refinery.output_quantity
                && a->data.refinery.progress == b->data.refinery.progress
                && a->data.refinery.duration == b->data.refinery.duration
                && a->data.refinery.processing
                    == b->data.refinery.processing;
        case FACTORY_ENTITY_TYPE_ASSEMBLER:
            if (a->data.assembler.recipe_id != b->data.assembler.recipe_id
                || a->data.assembler.output_item
                    != b->data.assembler.output_item
                || a->data.assembler.output_quantity
                    != b->data.assembler.output_quantity
                || a->data.assembler.progress != b->data.assembler.progress
                || a->data.assembler.duration != b->data.assembler.duration
                || a->data.assembler.processing
                    != b->data.assembler.processing) return false;
            for (i = 0U; i < FACTORY_ASSEMBLER_MAX_INPUT_TYPES; ++i)
                if (a->data.assembler.input_slots[i].item
                        != b->data.assembler.input_slots[i].item
                    || a->data.assembler.input_slots[i].count
                        != b->data.assembler.input_slots[i].count
                    || a->data.assembler.input_slots[i].capacity
                        != b->data.assembler.input_slots[i].capacity)
                    return false;
            return true;
        case FACTORY_ENTITY_TYPE_SPLITTER:
            return a->data.splitter.item == b->data.splitter.item
                && a->data.splitter.quantity == b->data.splitter.quantity
                && a->data.splitter.next_output
                    == b->data.splitter.next_output;
        case FACTORY_ENTITY_TYPE_INSERTER:
            return a->data.inserter.held_item
                    == b->data.inserter.held_item
                && a->data.inserter.held_quantity
                    == b->data.inserter.held_quantity
                && a->data.inserter.state == b->data.inserter.state
                && a->data.inserter.progress == b->data.inserter.progress
                && a->data.inserter.source_x == b->data.inserter.source_x
                && a->data.inserter.source_y == b->data.inserter.source_y
                && a->data.inserter.destination_x
                    == b->data.inserter.destination_x
                && a->data.inserter.destination_y
                    == b->data.inserter.destination_y;
        case FACTORY_ENTITY_TYPE_POWER_POLE:
            return a->data.power_pole.network_id
                    == b->data.power_pole.network_id
                && a->data.power_pole.machine_radius
                    == b->data.power_pole.machine_radius
                && a->data.power_pole.wire_radius
                    == b->data.power_pole.wire_radius
                && a->data.power_pole.connected_pole_count
                    == b->data.power_pole.connected_pole_count;
        case FACTORY_ENTITY_TYPE_POWER_GENERATOR:
            return a->data.power_source.available_generation
                    == b->data.power_source.available_generation
                && a->data.power_source.attached_pole_id
                    == b->data.power_source.attached_pole_id
                && a->data.power_source.network_id
                    == b->data.power_source.network_id
                && a->data.power_source.network_allocated_power
                    == b->data.power_source.network_allocated_power
                && a->data.power_source.connected
                    == b->data.power_source.connected;
        case FACTORY_ENTITY_TYPE_NONE:
        default:
            return false;
    }
}

static bool presentation_equal(
    const FactoryPresentationSnapshot *a,
    const FactoryPresentationSnapshot *b
)
{
    size_t i;
    if (factory_presentation_snapshot_get_tick(a)
            != factory_presentation_snapshot_get_tick(b)
        || factory_presentation_snapshot_get_entity_count(a)
            != factory_presentation_snapshot_get_entity_count(b)
        || factory_presentation_snapshot_get_resource_count(a)
            != factory_presentation_snapshot_get_resource_count(b)
        || factory_presentation_snapshot_get_power_edge_count(a)
            != factory_presentation_snapshot_get_power_edge_count(b))
        return false;
    for (i = 0U;
        i < factory_presentation_snapshot_get_entity_count(a); ++i)
        if (!entity_equal(
                factory_presentation_snapshot_get_entity(a, i),
                factory_presentation_snapshot_get_entity(b, i)))
            return false;
    for (i = 0U;
        i < factory_presentation_snapshot_get_resource_count(a); ++i) {
        const FactoryPresentationResource *x =
            factory_presentation_snapshot_get_resource(a, i);
        const FactoryPresentationResource *y =
            factory_presentation_snapshot_get_resource(b, i);
        if (x->x != y->x || x->y != y->y
            || x->resource_type != y->resource_type
            || x->remaining_quantity != y->remaining_quantity
            || x->occupying_entity_id != y->occupying_entity_id)
            return false;
    }
    for (i = 0U;
        i < factory_presentation_snapshot_get_power_edge_count(a); ++i) {
        const FactoryPresentationPowerEdge *x =
            factory_presentation_snapshot_get_power_edge(a, i);
        const FactoryPresentationPowerEdge *y =
            factory_presentation_snapshot_get_power_edge(b, i);
        if (x->pole_a != y->pole_a || x->pole_b != y->pole_b)
            return false;
    }
    return true;
}

static void test_empty_and_full_coverage(void)
{
    FactoryPresentationSnapshot *snapshot =
        factory_presentation_snapshot_create();
    FactoryPresentationSnapshot *same_state =
        factory_presentation_snapshot_create();
    FactoryWorld *world = factory_world_create(12U, 8U);
    FactorySimulation *simulation;
    size_t events;
    const FactoryPresentationEntity *entity;
    const FactoryPresentationResource *resource;
    const FactoryPresentationPowerEdge *edge;

    CHECK(snapshot != NULL);
    CHECK(factory_presentation_snapshot_get_tick(snapshot) == 0U);
    CHECK(factory_presentation_snapshot_get_entity_count(snapshot) == 0U);
    CHECK(factory_presentation_snapshot_get_entity(snapshot, 0U) == NULL);
    CHECK(factory_presentation_snapshot_get_resource(snapshot, 0U) == NULL);
    CHECK(factory_presentation_snapshot_get_power_edge(snapshot, 0U) == NULL);
    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 20U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 11, 7, FACTORY_RESOURCE_COPPER, 30U
    ) == FACTORY_RESULT_OK);
    simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {10, 0}}
    });                                                     /* 1 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 2 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 3 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            3, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
        }}
    });                                                     /* 4 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {5, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 5 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_SPLITTER,
        {.place_splitter = {7, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 6 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {8, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 7 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {3, 3}}
    });                                                     /* 8 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {9, 3}}
    });                                                     /* 9 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {3, 4}}
    });                                                     /* 10 */
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    events = factory_simulation_get_event_count(simulation);
    CHECK(events == 10U);
    CHECK(factory_presentation_snapshot_rebuild(snapshot, simulation)
        == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_rebuild(same_state, simulation)
        == FACTORY_RESULT_OK);
    CHECK(presentation_equal(snapshot, same_state));
    CHECK(factory_simulation_get_event_count(simulation) == events);
    CHECK(factory_presentation_snapshot_get_tick(snapshot) == 1U);
    CHECK(factory_presentation_snapshot_get_entity_count(snapshot) == 10U);
    CHECK(factory_presentation_snapshot_get_resource_count(snapshot) == 2U);
    CHECK(factory_presentation_snapshot_get_power_edge_count(snapshot) == 1U);
    for (size_t i = 0U; i < 10U; ++i)
        CHECK(factory_presentation_snapshot_get_entity(
            snapshot, i)->entity_id == i + 1U);

    entity = factory_presentation_snapshot_get_entity(snapshot, 0U);
    CHECK(entity->entity_type == FACTORY_ENTITY_TYPE_STORAGE);
    CHECK(entity->direction == FACTORY_PRESENTATION_DIRECTION_NONE);
    CHECK(entity->data.storage.total_capacity == FACTORY_STORAGE_CAPACITY);
    entity = factory_presentation_snapshot_get_entity(snapshot, 1U);
    CHECK(entity->entity_type == FACTORY_ENTITY_TYPE_EXTRACTOR);
    CHECK(entity->powered);
    CHECK(entity->status == FACTORY_PRESENTATION_MACHINE_STATUS_WORKING);
    CHECK(entity->data.extractor.progress == 1U);
    CHECK(entity->data.extractor.duration
        == FACTORY_EXTRACTOR_PRODUCTION_TICKS);
    CHECK(entity->data.extractor.can_progress);
    entity = factory_presentation_snapshot_get_entity(snapshot, 2U);
    CHECK(entity->entity_type == FACTORY_ENTITY_TYPE_BELT);
    CHECK(entity->data.belt.quantity == 0U);
    entity = factory_presentation_snapshot_get_entity(snapshot, 3U);
    CHECK(entity->entity_type == FACTORY_ENTITY_TYPE_REFINERY);
    CHECK(entity->status == FACTORY_PRESENTATION_MACHINE_STATUS_IDLE);
    entity = factory_presentation_snapshot_get_entity(snapshot, 4U);
    CHECK(entity->entity_type == FACTORY_ENTITY_TYPE_ASSEMBLER);
    CHECK(entity->data.assembler.recipe_id
        == FACTORY_ASSEMBLER_RECIPE_NONE);
    CHECK(entity->status == FACTORY_PRESENTATION_MACHINE_STATUS_IDLE);
    entity = factory_presentation_snapshot_get_entity(snapshot, 5U);
    CHECK(entity->entity_type == FACTORY_ENTITY_TYPE_SPLITTER);
    CHECK(entity->data.splitter.next_output
        == FACTORY_SPLITTER_OUTPUT_LEFT);
    entity = factory_presentation_snapshot_get_entity(snapshot, 6U);
    CHECK(entity->entity_type == FACTORY_ENTITY_TYPE_INSERTER);
    CHECK(entity->powered);
    CHECK(entity->data.inserter.source_x == 7);
    CHECK(entity->data.inserter.destination_x == 9);
    entity = factory_presentation_snapshot_get_entity(snapshot, 7U);
    CHECK(entity->entity_type == FACTORY_ENTITY_TYPE_POWER_POLE);
    CHECK(entity->data.power_pole.network_id == 8U);
    entity = factory_presentation_snapshot_get_entity(snapshot, 9U);
    CHECK(entity->entity_type == FACTORY_ENTITY_TYPE_POWER_GENERATOR);
    CHECK(entity->data.power_source.available_generation == 100U);
    CHECK(entity->data.power_source.network_id == 8U);
    CHECK(entity->data.power_source.network_allocated_power == 60U);

    resource = factory_presentation_snapshot_get_resource(snapshot, 0U);
    CHECK(resource->x == 0 && resource->y == 0);
    CHECK(resource->resource_type == FACTORY_RESOURCE_IRON);
    CHECK(resource->remaining_quantity == 20U);
    CHECK(resource->occupying_entity_id == 2U);
    resource = factory_presentation_snapshot_get_resource(snapshot, 1U);
    CHECK(resource->x == 11 && resource->y == 7);
    CHECK(resource->resource_type == FACTORY_RESOURCE_COPPER);
    edge = factory_presentation_snapshot_get_power_edge(snapshot, 0U);
    CHECK(edge->pole_a == 8U && edge->pole_b == 9U);

    /* Snapshot data remains unchanged while simulation-owned state advances. */
    entity = factory_presentation_snapshot_get_entity(snapshot, 1U);
    CHECK(entity->data.extractor.progress == 1U);
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(entity->data.extractor.progress == 1U);
    factory_simulation_clear_events(simulation);
    CHECK(entity->data.extractor.progress == 1U);
    CHECK(factory_presentation_snapshot_rebuild(snapshot, simulation)
        == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 1U)->data.extractor.progress == 2U);
    CHECK(factory_simulation_get_event_count(simulation) == 0U);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {9U}}
    });
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_rebuild(snapshot, simulation)
        == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_get_entity_count(snapshot) == 9U);
    for (size_t i = 0U; i < 8U; ++i)
        CHECK(factory_presentation_snapshot_get_entity(
            snapshot, i)->entity_id == i + 1U);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 8U)->entity_id == 10U);
    CHECK(factory_presentation_snapshot_get_power_edge_count(snapshot) == 0U);

    factory_presentation_snapshot_clear(snapshot);
    CHECK(factory_presentation_snapshot_get_tick(snapshot) == 0U);
    CHECK(factory_presentation_snapshot_get_entity_count(snapshot) == 0U);
    factory_presentation_snapshot_destroy(same_state);
    factory_presentation_snapshot_destroy(snapshot);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_statuses_logistics_and_transactionality(void)
{
    FactoryWorld *world = factory_world_create(8U, 5U);
    FactorySimulation *simulation;
    FactoryPresentationSnapshot *snapshot =
        factory_presentation_snapshot_create();
    FactorySnapshotBuffer before = {0};
    FactorySnapshotBuffer after = {0};
    FactoryPresentationEntity saved;
    uint64_t saved_tick;

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 10U
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
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {2, 0}}
    });                                                     /* 3 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            3, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_EAST
        }}
    });                                                     /* 4 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {4, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 5 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_SPLITTER,
        {.place_splitter = {5, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 6 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {6, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 7 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {3, 2}}
    });                                                     /* 8 */
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {3, 3}}
    });                                                     /* 9 */
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);

    simulation->extractors.items[0].output_item = FACTORY_ITEM_IRON_ORE;
    simulation->extractors.items[0].output_amount = 1U;
    simulation->belts.items[0].item = FACTORY_ITEM_COPPER_ORE;
    simulation->belts.items[0].movement_progress = 3U;
    simulation->storages.items[0].iron_plate_amount = 4U;
    simulation->storages.items[0].configured_output_item =
        FACTORY_ITEM_IRON_PLATE;
    simulation->storages.items[0].output_item = FACTORY_ITEM_IRON_PLATE;
    simulation->storages.items[0].output_occupied = true;
    simulation->refineries.items[0].recipe_id = FACTORY_RECIPE_IRON_PLATE;
    simulation->refineries.items[0].processing = true;
    simulation->refineries.items[0].processing_progress = 4U;
    CHECK(factory_assembler_configure_recipe(
        &simulation->assemblers.items[0],
        FACTORY_ASSEMBLER_RECIPE_ELECTRONIC_COMPONENT
    ));
    simulation->assemblers.items[0].processing = true;
    simulation->assemblers.items[0].processing_progress = 3U;
    simulation->splitters.items[0].item = FACTORY_ITEM_COPPER_PLATE;
    simulation->splitters.items[0].next_output =
        FACTORY_SPLITTER_OUTPUT_RIGHT;
    simulation->inserters.items[0].held_item = FACTORY_ITEM_IRON_GEAR;
    simulation->inserters.items[0].held_amount = 1U;
    simulation->inserters.items[0].state = FACTORY_INSERTER_STATE_DROPPING;

    CHECK(factory_presentation_snapshot_rebuild(snapshot, simulation)
        == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 0U)->status
        == FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 1U)->data.belt.quantity == 1U);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 2U)->data.storage.item_quantities[1] == 4U);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 2U)->data.storage.output_quantity == 1U);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 3U)->status
        == FACTORY_PRESENTATION_MACHINE_STATUS_WORKING);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 3U)->data.refinery.duration
        == FACTORY_IRON_PLATE_PROCESSING_TICKS);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 4U)->data.assembler.progress == 3U);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 5U)->data.splitter.next_output
        == FACTORY_SPLITTER_OUTPUT_RIGHT);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 6U)->data.inserter.held_item == FACTORY_ITEM_IRON_GEAR);

    saved_tick = factory_presentation_snapshot_get_tick(snapshot);
    saved = *factory_presentation_snapshot_get_entity(snapshot, 0U);
    CHECK(factory_simulation_create_snapshot(simulation, &before)
        == FACTORY_RESULT_OK);
    factory_presentation_test_fail_allocations_after(0U);
    CHECK(factory_presentation_snapshot_rebuild(snapshot, simulation)
        == FACTORY_RESULT_OUT_OF_MEMORY);
    factory_presentation_test_fail_allocations_after(SIZE_MAX);
    CHECK(factory_presentation_snapshot_get_tick(snapshot) == saved_tick);
    CHECK(entity_equal(
        &saved, factory_presentation_snapshot_get_entity(snapshot, 0U)));
    CHECK(factory_simulation_create_snapshot(simulation, &after)
        == FACTORY_RESULT_OK);
    CHECK(before.size == after.size);
    CHECK(memcmp(before.data, after.data, before.size) == 0);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {9U}}
    });
    CHECK(factory_simulation_tick(simulation) == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_rebuild(snapshot, simulation)
        == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_get_entity(
        snapshot, 0U)->status
        == FACTORY_PRESENTATION_MACHINE_STATUS_UNPOWERED);

    factory_snapshot_buffer_destroy(&after);
    factory_snapshot_buffer_destroy(&before);
    factory_presentation_snapshot_destroy(snapshot);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_save_load_and_continuation(void)
{
    FactoryWorld *world = factory_world_create(6U, 4U);
    FactorySimulation *a;
    FactorySimulation *b = NULL;
    FactoryPresentationSnapshot *pa = factory_presentation_snapshot_create();
    FactoryPresentationSnapshot *pb = factory_presentation_snapshot_create();
    FactorySnapshotBuffer saved = {0};

    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 10U
    ) == FACTORY_RESULT_OK);
    a = factory_simulation_create_with_construction_units(world, UINT32_MAX);
    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 1 */
    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });                                                     /* 2 */
    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {2, 0}}
    });                                                     /* 3 */
    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE, {.place_power_pole = {1, 2}}
    });                                                     /* 4 */
    submit(a, (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {1, 3}}
    });                                                     /* 5 */
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    for (uint32_t i = 0U; i < 7U; ++i)
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_rebuild(pa, a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(a, &saved) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        saved.data, saved.size, &b
    ) == FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_rebuild(pb, b) == FACTORY_RESULT_OK);
    CHECK(presentation_equal(pa, pb));

    for (uint32_t step = 0U; step < 45U; ++step) {
        CHECK(factory_simulation_tick(a) == factory_simulation_tick(b));
        CHECK(factory_simulation_get_event_count(a)
            == factory_simulation_get_event_count(b));
        for (size_t i = 0U;
            i < factory_simulation_get_event_count(a); ++i) {
            const FactoryEvent *x = factory_simulation_get_event(a, i);
            const FactoryEvent *y = factory_simulation_get_event(b, i);
            CHECK(x->type == y->type && x->tick == y->tick);
            CHECK(x->entity_id == y->entity_id
                && x->related_entity_id == y->related_entity_id);
            CHECK(x->entity_type == y->entity_type
                && x->item_type == y->item_type
                && x->quantity == y->quantity);
        }
        CHECK(factory_presentation_snapshot_rebuild(pa, a)
            == FACTORY_RESULT_OK);
        CHECK(factory_presentation_snapshot_rebuild(pb, b)
            == FACTORY_RESULT_OK);
        CHECK(presentation_equal(pa, pb));
    }

    factory_snapshot_buffer_destroy(&saved);
    factory_presentation_snapshot_destroy(pb);
    factory_presentation_snapshot_destroy(pa);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world);
}

int main(void)
{
    test_empty_and_full_coverage();
    test_statuses_logistics_and_transactionality();
    test_save_load_and_continuation();
    if (failures != 0) return 1;
    (void)printf("All presentation tests passed.\n");
    return 0;
}
