#include "foundation/snapshot.h"

#include "simulation_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *simulation, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(simulation, &command)
        == FACTORY_RESULT_OK);
}

static void put_u32_le(uint8_t *data, size_t offset, uint32_t value)
{
    data[offset] = (uint8_t)value;
    data[offset + 1U] = (uint8_t)(value >> 8U);
    data[offset + 2U] = (uint8_t)(value >> 16U);
    data[offset + 3U] = (uint8_t)(value >> 24U);
}

static bool snapshots_equal(
    const FactorySimulation *left,
    const FactorySimulation *right
)
{
    FactorySnapshotBuffer a = {0};
    FactorySnapshotBuffer b = {0};
    bool equal = false;

    if (factory_simulation_create_snapshot(left, &a) == FACTORY_RESULT_OK
        && factory_simulation_create_snapshot(right, &b)
            == FACTORY_RESULT_OK) {
        equal = a.size == b.size
            && memcmp(a.data, b.data, a.size) == 0;
    }
    factory_snapshot_buffer_destroy(&a);
    factory_snapshot_buffer_destroy(&b);
    return equal;
}

static void test_empty_and_header(void)
{
    FactoryWorld *world = factory_world_create(2U, 2U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, 77U);
    FactorySnapshotBuffer snapshot = {0};
    FactorySnapshotBuffer repeated = {0};
    FactorySimulation *loaded = NULL;
    uint8_t *mutated;
    size_t required = 0U;
    size_t written = 99U;

    CHECK(factory_simulation_snapshot_size(simulation, &required)
        == FACTORY_RESULT_OK);
    CHECK(required > 48U);
    CHECK(factory_simulation_snapshot_size(NULL, &required)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_simulation_snapshot_size(simulation, NULL)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_simulation_save_snapshot(
        simulation, NULL, 0U, &written
    ) == FACTORY_RESULT_SNAPSHOT_BUFFER_TOO_SMALL);
    CHECK(written == 0U);
    mutated = malloc(required);
    CHECK(mutated != NULL);
    CHECK(factory_simulation_save_snapshot(
        simulation, mutated, required - 1U, &written
    ) == FACTORY_RESULT_SNAPSHOT_BUFFER_TOO_SMALL);
    CHECK(factory_simulation_save_snapshot(
        simulation, mutated, required, &written
    ) == FACTORY_RESULT_OK);
    CHECK(written == required);
    CHECK(memcmp(mutated, "FOUNDATN", 8U) == 0);
    CHECK(mutated[8] == 1U && mutated[9] == 0U);
    CHECK(factory_simulation_create_snapshot(simulation, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(simulation, &repeated)
        == FACTORY_RESULT_OK);
    CHECK(snapshot.size == repeated.size);
    CHECK(memcmp(snapshot.data, repeated.data, snapshot.size) == 0);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &loaded
    ) == FACTORY_RESULT_OK);
    CHECK(loaded != NULL);
    CHECK(factory_simulation_get_tick(loaded) == 0U);
    CHECK(factory_simulation_construction_units(loaded) == 77U);
    CHECK(snapshots_equal(simulation, loaded));
    factory_simulation_destroy(loaded);
    loaded = NULL;

    (void)memcpy(mutated, snapshot.data, snapshot.size);
    mutated[0] ^= 0xffU;
    CHECK(factory_simulation_load_snapshot(
        mutated, snapshot.size, &loaded
    ) == FACTORY_RESULT_SNAPSHOT_INVALID_MAGIC);
    CHECK(loaded == NULL);
    (void)memcpy(mutated, snapshot.data, snapshot.size);
    put_u32_le(mutated, 8U, 0U);
    CHECK(factory_simulation_load_snapshot(
        mutated, snapshot.size, &loaded
    ) == FACTORY_RESULT_SNAPSHOT_UNSUPPORTED_VERSION);
    put_u32_le(mutated, 8U, FACTORY_SNAPSHOT_VERSION + 1U);
    CHECK(factory_simulation_load_snapshot(
        mutated, snapshot.size, &loaded
    ) == FACTORY_RESULT_SNAPSHOT_UNSUPPORTED_VERSION);
    (void)memcpy(mutated, snapshot.data, snapshot.size);
    put_u32_le(mutated, 12U, 44U);
    CHECK(factory_simulation_load_snapshot(
        mutated, snapshot.size, &loaded
    ) == FACTORY_RESULT_SNAPSHOT_CORRUPT);
    (void)memcpy(mutated, snapshot.data, snapshot.size);
    mutated[44] = 1U;
    CHECK(factory_simulation_load_snapshot(
        mutated, snapshot.size, &loaded
    ) == FACTORY_RESULT_SNAPSHOT_CORRUPT);
    (void)memcpy(mutated, snapshot.data, snapshot.size);
    put_u32_le(mutated, 48U, 99U);
    CHECK(factory_simulation_load_snapshot(
        mutated, snapshot.size, &loaded
    ) == FACTORY_RESULT_SNAPSHOT_CORRUPT);
    CHECK(factory_simulation_load_snapshot(NULL, 0U, &loaded)
        == FACTORY_RESULT_INVALID_ARGUMENT);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, NULL
    ) == FACTORY_RESULT_INVALID_ARGUMENT);
    for (size_t size = 1U; size < snapshot.size; ++size) {
        CHECK(factory_simulation_load_snapshot(
            snapshot.data, size, &loaded
        ) != FACTORY_RESULT_OK);
        CHECK(loaded == NULL);
    }
    free(mutated);
    factory_snapshot_buffer_destroy(&repeated);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static FactorySimulation *create_complex(FactoryWorld **out_world)
{
    FactoryWorld *world = factory_world_create(7U, 2U);
    FactorySimulation *simulation;

    CHECK(world != NULL);
    CHECK(factory_world_add_resource(
        world, 0, 0, FACTORY_RESOURCE_IRON, 20U
    ) == FACTORY_RESULT_OK);
    CHECK(factory_world_add_resource(
        world, 6, 1, FACTORY_RESOURCE_COPPER, 12U
    ) == FACTORY_RESULT_OK);
    simulation = factory_simulation_create_with_construction_units(
        world, 1000U
    );
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {1, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery = {
            2, 0, FACTORY_DIRECTION_WEST, FACTORY_DIRECTION_SOUTH
        }}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler = {3, 0, FACTORY_DIRECTION_SOUTH}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_SPLITTER,
        {.place_splitter = {4, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter = {5, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {6, 0}}
    });
    factory_simulation_tick(simulation);
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_REFINERY_RECIPE,
        {.set_refinery_recipe = {3U, FACTORY_RECIPE_IRON_PLATE}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe = {
            4U, FACTORY_ASSEMBLER_RECIPE_COPPER_WIRE
        }}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output = {7U, FACTORY_ITEM_IRON_GEAR}}
    });
    factory_simulation_tick(simulation);
    for (uint32_t tick = 0U; tick < 13U; ++tick) {
        factory_simulation_tick(simulation);
    }
    simulation->storages.items[0].iron_gear_amount = 3U;
    simulation->storages.items[0].copper_wire_amount = 4U;
    simulation->splitters.items[0].next_output =
        FACTORY_SPLITTER_OUTPUT_RIGHT;
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_GRANT_CONSTRUCTION_UNITS,
        {.grant_construction_units = {9U}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {0, 1, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output = {7U, FACTORY_ITEM_COPPER_WIRE}}
    });
    *out_world = world;
    return simulation;
}

static void test_complex_round_trip_and_continuation(void)
{
    FactoryWorld *world;
    FactorySimulation *control = create_complex(&world);
    FactorySnapshotBuffer snapshot = {0};
    FactorySnapshotBuffer chain = {0};
    FactorySimulation *loaded = NULL;

    CHECK(factory_simulation_create_snapshot(control, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &loaded
    ) == FACTORY_RESULT_OK);
    CHECK(loaded != NULL);
    CHECK(snapshots_equal(control, loaded));
    CHECK(factory_simulation_get_pending_command_count(loaded) == 3U);
    CHECK(factory_simulation_get_command_result_count(loaded) == 0U);
    CHECK(factory_simulation_create_snapshot(loaded, &chain)
        == FACTORY_RESULT_OK);
    CHECK(snapshot.size == chain.size);
    CHECK(memcmp(snapshot.data, chain.data, snapshot.size) == 0);

    for (uint32_t tick = 0U; tick < 40U; ++tick) {
        factory_simulation_tick(control);
        factory_simulation_tick(loaded);
        CHECK(snapshots_equal(control, loaded));
    }
    CHECK(factory_simulation_get_tick(control)
        == factory_simulation_get_tick(loaded));
    CHECK(factory_simulation_get_entity_count(control)
        == factory_simulation_get_entity_count(loaded));

    factory_snapshot_buffer_destroy(&chain);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(control);
    factory_world_destroy(world);
}

static void test_invalid_internal_state_rejected(void)
{
    FactoryWorld *world = factory_world_create(1U, 1U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, UINT32_MAX);
    size_t size;

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {0, 0}}
    });
    factory_simulation_tick(simulation);
    simulation->storages.items[0].output_occupied = true;
    simulation->storages.items[0].output_item = FACTORY_ITEM_NONE;
    CHECK(factory_simulation_snapshot_size(simulation, &size)
        == FACTORY_RESULT_SNAPSHOT_CORRUPT);

    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

static void test_command_results_and_next_id(void)
{
    FactoryWorld *world = factory_world_create(3U, 1U);
    FactorySimulation *simulation =
        factory_simulation_create_with_construction_units(world, 20U);
    FactorySnapshotBuffer snapshot = {0};
    FactorySimulation *loaded = NULL;
    const FactoryCommandResult *result;

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {0, 0, FACTORY_DIRECTION_EAST}}
    });
    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_BELT,
        {.place_belt = {0, 0, FACTORY_DIRECTION_WEST}}
    });
    factory_simulation_tick(simulation);
    CHECK(factory_simulation_get_command_result_count(simulation) == 2U);
    CHECK(factory_simulation_create_snapshot(simulation, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &loaded
    ) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_world(loaded) != NULL);
    CHECK(factory_world_get_width(
        factory_simulation_get_world(loaded)) == 3U);
    CHECK(factory_simulation_get_command_result_count(loaded) == 2U);
    result = factory_simulation_get_command_result(loaded, 0U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_OK);
    CHECK(result->entity_id == 1U);
    result = factory_simulation_get_command_result(loaded, 1U);
    CHECK(result != NULL && result->result == FACTORY_RESULT_TILE_OCCUPIED);

    submit(simulation, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {1, 0}}
    });
    submit(loaded, (FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE, {.place_storage = {1, 0}}
    });
    factory_simulation_tick(simulation);
    factory_simulation_tick(loaded);
    CHECK(factory_simulation_get_command_result(simulation, 0U)->entity_id
        == factory_simulation_get_command_result(loaded, 0U)->entity_id);
    CHECK(factory_simulation_get_command_result(loaded, 0U)->entity_id == 2U);
    CHECK(snapshots_equal(simulation, loaded));

    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(simulation);
    factory_world_destroy(world);
}

int main(void)
{
    test_empty_and_header();
    test_complex_round_trip_and_continuation();
    test_invalid_internal_state_rejected();
    test_command_results_and_next_id();

    if (failures != 0) {
        return 1;
    }
    (void)printf("All snapshot tests passed.\n");
    return 0;
}
