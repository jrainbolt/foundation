#include "foundation/presentation.h"
#include "foundation/snapshot.h"

#include "fluid_internal.h"
#include "burner_internal.h"
#include "power_internal.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *s, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(s, &command) == FACTORY_RESULT_OK);
}

static FactorySimulation *make(
    FactoryWorld **out_world, uint32_t width, uint32_t height
)
{
    *out_world = factory_world_create(width, height);
    return factory_simulation_create_with_construction_units(
        *out_world, UINT32_MAX);
}

static FactoryFluidStorage *steam(FactorySimulation *s, FactoryEntityId id)
{
    return factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages, id,
        FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT);
}

static const FactoryEvent *generation_event(const FactorySimulation *s)
{
    for (size_t i = 0U; i < factory_simulation_get_event_count(s); ++i) {
        const FactoryEvent *event = factory_simulation_get_event(s, i);
        if (event->type == FACTORY_EVENT_STEAM_ENGINE_GENERATION_COMPLETED)
            return event;
    }
    return NULL;
}

static void test_recipe_construction_input_and_demolition(void)
{
    FactoryWorld *world;
    FactorySimulation *s = make(&world, 4U, 3U);
    FactorySteamEngineInspection engine;
    FactoryFluidStorageInspection storage;
    FactoryFluidPortInspection port;
    const FactorySteamGenerationRecipe *recipe =
        factory_steam_generation_recipe_get(
            FACTORY_STEAM_GENERATION_RECIPE_BASIC);

    CHECK(factory_steam_generation_recipe_count() == 1U);
    CHECK(factory_steam_generation_recipe_is_valid(recipe));
    CHECK(recipe != NULL && recipe->input_fluid == FACTORY_FLUID_STEAM);
    CHECK(recipe != NULL && recipe->input_quantity == 100U);
    CHECK(recipe != NULL && recipe->generated_energy == 100U);
    CHECK(recipe != NULL && recipe->maximum_output_per_tick == 100U);

    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_ENGINE,
        {.place_steam_engine = {1, 1}}});
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_ENGINE,
        {.place_steam_engine = {1, 1}}});
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s, 1U)->result
        == FACTORY_RESULT_TILE_OCCUPIED);
    CHECK(factory_simulation_get_steam_engine(s, 1U, &engine)
        == FACTORY_RESULT_OK);
    CHECK(engine.stored_steam == 0U && engine.steam_capacity == 1000U);
    CHECK(factory_simulation_get_fluid_storage_slot(
        s, 1U, FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT, &storage)
        == FACTORY_RESULT_OK);
    CHECK(storage.accepted_fluid_classes == FACTORY_FLUID_CLASS_VAPOR);
    CHECK(factory_simulation_get_fluid_port_slot(
        s, 1U, FACTORY_FLUID_STORAGE_STEAM_ENGINE_INPUT, &port)
        == FACTORY_RESULT_OK);
    CHECK(port.allowed_directions == FACTORY_FLUID_CONNECTION_WEST);
    CHECK(factory_simulation_get_power_generator(s, 1U, &(FactoryPowerGeneratorInspection){0})
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_burner(s, 1U, &(FactoryBurnerInspection){0})
        == FACTORY_RESULT_ENTITY_NOT_FOUND);

    CHECK(factory_simulation_submit_fluid_insert(
        s, 1U, FACTORY_FLUID_WATER, 10U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s, 0U)->result
        == FACTORY_RESULT_FLUID_INCOMPATIBLE);
    CHECK(steam(s, 1U)->quantity == 0U);
    CHECK(factory_simulation_submit_fluid_insert(
        s, 1U, FACTORY_FLUID_STEAM, 250U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(steam(s, 1U)->quantity == 250U);
    CHECK(generation_event(s) == NULL);

    CHECK(factory_simulation_submit_fluid_remove(s, 1U, 250U)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    submit(s, (FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity = {1U}}});
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s, 0U)->result
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_engine(s, 1U, &engine)
        == FACTORY_RESULT_ENTITY_NOT_FOUND);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

static void test_demand_aware_generation_and_limits(void)
{
    FactoryWorld *world;
    FactorySimulation *s = make(&world, 8U, 8U);
    FactorySteamEngineInspection engine;
    FactoryPowerNetworkInspection network;

    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_ENGINE,
        {.place_steam_engine = {1, 1}}});                 /* 1 */
    CHECK(factory_simulation_submit_fluid_insert(
        s, 1U, FACTORY_FLUID_STEAM, 500U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(steam(s, 1U)->quantity == 500U); /* disconnected */

    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {2, 2}}});                   /* 2 */
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(steam(s, 1U)->quantity == 500U); /* zero demand */

    CHECK(factory_world_add_resource(
        world, 3, 2, FACTORY_RESOURCE_IRON, 100U) == FACTORY_RESULT_OK);
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {3, 2, FACTORY_DIRECTION_EAST}}}); /* 3 */
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(steam(s, 1U)->quantity == 490U);
    CHECK(factory_simulation_get_steam_engine(s, 1U, &engine)
        == FACTORY_RESULT_OK);
    CHECK(engine.generated_last_tick == 10U && engine.active);
    {
        const FactoryEvent *event = generation_event(s);
        CHECK(event != NULL && event->entity_id == 1U);
        CHECK(event != NULL && event->fluid_type == FACTORY_FLUID_STEAM);
        CHECK(event != NULL && event->quantity == 10U);
        CHECK(event != NULL && event->related_quantity == 10U);
        CHECK(event != NULL && event->tick == 2U);
    }

    const int32_t positions[4][2] = {{3,3}, {3,4}, {4,3}, {4,4}};
    for (size_t i = 0U; i < 4U; ++i)
        submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler = {
                positions[i][0], positions[i][1], FACTORY_DIRECTION_EAST}}});
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_power_network(s, 0U, &network)
        == FACTORY_RESULT_OK);
    CHECK(network.total_demand == 110U);
    CHECK(network.total_generation == 100U);
    CHECK(network.allocated_power == 85U);
    CHECK(steam(s, 1U)->quantity == 405U);
    CHECK(factory_simulation_get_steam_engine(s, 1U, &engine)
        == FACTORY_RESULT_OK);
    CHECK(engine.generated_last_tick == 85U);
    CHECK(engine.generated_last_tick <= FACTORY_STEAM_ENGINE_MAX_OUTPUT);

    steam(s, 1U)->quantity = 5U;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(engine.generated_last_tick <= FACTORY_STEAM_ENGINE_MAX_OUTPUT);
    CHECK(steam(s, 1U)->quantity == 5U);
    CHECK(factory_simulation_get_steam_engine(s, 1U, &engine)
        == FACTORY_RESULT_OK);
    CHECK(engine.generated_last_tick == 0U);
    CHECK(generation_event(s) == NULL);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

static void test_pipe_delivery_presentation_and_snapshot(void)
{
    FactoryWorld *world;
    FactorySimulation *a = make(&world, 7U, 4U);
    FactorySimulation *b = NULL;
    FactorySnapshotBuffer snapshot = {0}, repeated = {0};
    FactoryPresentationSnapshot *view =
        factory_presentation_snapshot_create();

    submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_FLUID_TANK,
        {.place_fluid_tank = {0, 1}}});                  /* 1 */
    submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe = {1, 1}}});                        /* 2 */
    submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_ENGINE,
        {.place_steam_engine = {2, 1}}});                /* 3 */
    submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {3, 1}}});                  /* 4 */
    CHECK(factory_world_add_resource(
        world, 4, 1, FACTORY_RESOURCE_IRON, 100U) == FACTORY_RESULT_OK);
    submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {4, 1, FACTORY_DIRECTION_EAST}}}); /* 5 */
    CHECK(factory_simulation_submit_fluid_insert(
        a, 1U, FACTORY_FLUID_STEAM, 500U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(steam(a, 3U)->quantity == 90U);
    CHECK(generation_event(a) != NULL);

    CHECK(factory_presentation_snapshot_rebuild(view, a)
        == FACTORY_RESULT_OK);
    {
        const FactoryPresentationEntity *entity =
            factory_presentation_snapshot_get_entity(view, 2U);
        CHECK(entity != NULL
            && entity->entity_type == FACTORY_ENTITY_TYPE_STEAM_ENGINE);
        CHECK(entity != NULL && entity->data.steam_engine.stored_steam == 90U);
        CHECK(entity != NULL
            && entity->data.steam_engine.steam_network_id == 2U);
        CHECK(entity != NULL
            && entity->data.steam_engine.power_network_id == 4U);
        CHECK(entity != NULL
            && entity->data.steam_engine.generated_last_tick == 10U);
        CHECK(entity != NULL && entity->data.steam_engine.active);
    }

    CHECK(factory_simulation_create_snapshot(a, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &b) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(b, &repeated)
        == FACTORY_RESULT_OK);
    CHECK(snapshot.size == repeated.size);
    CHECK(snapshot.size == repeated.size
        && memcmp(snapshot.data, repeated.data, snapshot.size) == 0);
    for (uint32_t tick = 0U; tick < 5U; ++tick) {
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
        CHECK(steam(a, 3U)->quantity == steam(b, 3U)->quantity);
        const FactoryEvent *x = generation_event(a);
        const FactoryEvent *y = generation_event(b);
        CHECK((x == NULL) == (y == NULL));
        if (x != NULL && y != NULL)
            CHECK(x->tick == y->tick && x->quantity == y->quantity
                && x->related_quantity == y->related_quantity);
    }

    factory_snapshot_buffer_destroy(&repeated);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_presentation_snapshot_destroy(view);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world);
}

static void test_full_water_boiler_steam_power_chain(void)
{
    FactoryWorld *world;
    FactorySimulation *s = make(&world, 8U, 3U);
    bool generated = false;

    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_WATER_EXTRACTOR,
        {.place_water_extractor = {0, 1}}});             /* 1 */
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe = {1, 1}}});                        /* 2 */
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_BOILER,
        {.place_boiler = {2, 1}}});                      /* 3 */
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe = {3, 1}}});                        /* 4 */
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_ENGINE,
        {.place_steam_engine = {4, 1}}});                /* 5 */
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {5, 1}}});                  /* 6 */
    CHECK(factory_world_add_resource(
        world, 6, 1, FACTORY_RESOURCE_IRON, 100U) == FACTORY_RESULT_OK);
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor = {6, 1, FACTORY_DIRECTION_EAST}}}); /* 7 */
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    factory_burner_store_find_mutable(
        &s->burners, 3U)->released_energy = 1000U;

    for (uint32_t tick = 0U; tick < 30U && !generated; ++tick) {
        CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
        generated = generation_event(s) != NULL;
    }
    CHECK(generated);
    CHECK(steam(s, 5U)->fluid_type == FACTORY_FLUID_STEAM
        || steam(s, 5U)->quantity == 0U);
    CHECK(factory_simulation_get_power_consumer(
        s, 7U, &(FactoryPowerConsumerInspection){0}) == FACTORY_RESULT_OK);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

static void test_multiple_generator_source_priority(void)
{
    FactoryWorld *world;
    FactorySimulation *s = make(&world, 8U, 8U);
    const int32_t positions[5][2] = {
        {3,2}, {3,3}, {3,4}, {4,2}, {4,3}
    };

    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_ENGINE,
        {.place_steam_engine = {0, 0}}});                /* 1 */
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_ENGINE,
        {.place_steam_engine = {0, 1}}});                /* 2 */
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator = {1, 0}}});             /* 3 */
    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole = {2, 1}}});                  /* 4 */
    for (size_t i = 0U; i < 5U; ++i)
        submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler = {
                positions[i][0], positions[i][1], FACTORY_DIRECTION_EAST}}});
    CHECK(factory_simulation_submit_fluid_insert(
        s, 1U, FACTORY_FLUID_STEAM, 500U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_fluid_insert(
        s, 2U, FACTORY_FLUID_STEAM, 500U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    factory_burner_store_find_mutable(
        &s->burners, 3U)->released_energy = 100U;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(steam(s, 1U)->quantity == 300U);
    CHECK(steam(s, 2U)->quantity == 450U);
    CHECK(factory_burner_store_find(
        &s->burners, 3U)->released_energy == 100U);
    CHECK(factory_simulation_get_steam_engine(
        s, 1U, &(FactorySteamEngineInspection){0}) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_engine(
        s, 2U, &(FactorySteamEngineInspection){0}) == FACTORY_RESULT_OK);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

int main(void)
{
    test_recipe_construction_input_and_demolition();
    test_demand_aware_generation_and_limits();
    test_pipe_delivery_presentation_and_snapshot();
    test_full_water_boiler_steam_power_chain();
    test_multiple_generator_source_priority();
    if (failures != 0)
        (void)fprintf(stderr, "%d steam engine test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
