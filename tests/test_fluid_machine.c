#include "foundation/presentation.h"
#include "foundation/snapshot.h"

#include "burner_internal.h"
#include "fluid_internal.h"
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

static FactorySimulation *make_simulation(
    FactoryWorld **out_world, uint32_t width, uint32_t height
)
{
    *out_world = factory_world_create(width, height);
    return factory_simulation_create_with_construction_units(
        *out_world, UINT32_MAX);
}

static FactoryFluidStorage *slot(
    FactorySimulation *s, FactoryEntityId id, FactoryFluidStorageSlot storage
)
{
    return factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages, id, storage);
}

static size_t event_count(
    const FactorySimulation *s, FactoryEventType type
)
{
    size_t count = 0U;
    for (size_t i = 0U; i < factory_simulation_get_event_count(s); ++i)
        if (factory_simulation_get_event(s, i)->type == type) ++count;
    return count;
}

static void test_recipe_and_water_extractor(void)
{
    FactoryWorld *world;
    FactorySimulation *s = make_simulation(&world, 3U, 2U);
    FactoryWaterExtractorInspection machine;
    const FactoryFluidConversionRecipe *recipe =
        factory_fluid_conversion_recipe_get(
            FACTORY_FLUID_RECIPE_BOIL_WATER);

    CHECK(factory_fluid_conversion_recipe_count() == 1U);
    CHECK(factory_fluid_conversion_recipe_is_valid(recipe));
    CHECK(recipe != NULL && recipe->input_fluid == FACTORY_FLUID_WATER);
    CHECK(recipe != NULL && recipe->input_quantity == 100U);
    CHECK(recipe != NULL && recipe->energy == 100U);
    CHECK(recipe != NULL && recipe->output_fluid == FACTORY_FLUID_STEAM);
    CHECK(recipe != NULL && recipe->output_quantity == 100U);

    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_WATER_EXTRACTOR,
        {.place_water_extractor = {0, 0}}});
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_water_extractor(s, 1U, &machine)
        == FACTORY_RESULT_OK);
    CHECK(machine.stored_water == 0U && machine.progress == 1U);
    for (uint32_t i = 0U; i < 3U; ++i)
        CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(event_count(s, FACTORY_EVENT_WATER_PRODUCED) == 0U);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_water_extractor(s, 1U, &machine)
        == FACTORY_RESULT_OK);
    CHECK(machine.stored_water == 100U && machine.progress == 0U);
    CHECK(event_count(s, FACTORY_EVENT_WATER_PRODUCED) == 1U);
    {
        const FactoryEvent *event = factory_simulation_get_event(s, 0U);
        CHECK(event != NULL && event->tick == 4U);
        CHECK(event != NULL && event->entity_id == 1U);
        CHECK(event != NULL && event->fluid_type == FACTORY_FLUID_WATER);
        CHECK(event != NULL && event->quantity == 100U);
    }

    slot(s, 1U, FACTORY_FLUID_STORAGE_DEFAULT)->quantity =
        FACTORY_WATER_EXTRACTOR_CAPACITY;
    for (uint32_t i = 0U; i < FACTORY_WATER_EXTRACTOR_CYCLE_TICKS; ++i)
        CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_water_extractor(s, 1U, &machine)
        == FACTORY_RESULT_OK);
    CHECK(machine.stored_water == FACTORY_WATER_EXTRACTOR_CAPACITY);
    CHECK(machine.progress == FACTORY_WATER_EXTRACTOR_CYCLE_TICKS - 1U);
    CHECK(event_count(s, FACTORY_EVENT_WATER_PRODUCED) == 0U);

    slot(s, 1U, FACTORY_FLUID_STORAGE_DEFAULT)->quantity = 900U;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_water_extractor(s, 1U, &machine)
        == FACTORY_RESULT_OK);
    CHECK(machine.stored_water == 1000U && machine.progress == 0U);
    CHECK(event_count(s, FACTORY_EVENT_WATER_PRODUCED) == 1U);

    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

static void test_boiler_atomic_conversion_and_presentation(void)
{
    FactoryWorld *world;
    FactorySimulation *s = make_simulation(&world, 3U, 2U);
    FactoryFluidStorage *water;
    FactoryFluidStorage *steam;
    FactoryBurner *burner;
    FactoryBoilerInspection machine;
    FactoryPresentationSnapshot *view =
        factory_presentation_snapshot_create();

    submit(s, (FactoryCommand){FACTORY_COMMAND_PLACE_BOILER,
        {.place_boiler = {1, 0}}});
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    water = slot(s, 1U, FACTORY_FLUID_STORAGE_BOILER_INPUT);
    steam = slot(s, 1U, FACTORY_FLUID_STORAGE_BOILER_OUTPUT);
    burner = factory_burner_store_find_mutable(&s->burners, 1U);
    CHECK(water != NULL && steam != NULL && burner != NULL);

    water->fluid_type = FACTORY_FLUID_WATER;
    water->quantity = 99U;
    burner->released_energy = 100U;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(water->quantity == 99U && steam->quantity == 0U);
    CHECK(burner->released_energy == 100U);
    CHECK(event_count(s, FACTORY_EVENT_BOILER_CONVERSION_COMPLETED) == 0U);

    water->quantity = 100U;
    burner->released_energy = 99U;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(water->quantity == 100U && steam->quantity == 0U);
    CHECK(burner->released_energy == 99U);

    burner->released_energy = 100U;
    steam->fluid_type = FACTORY_FLUID_STEAM;
    steam->quantity = steam->capacity;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(water->quantity == 100U && steam->quantity == steam->capacity);
    CHECK(burner->released_energy == 100U);

    steam->quantity = 900U;
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(water->quantity == 0U && water->fluid_type == FACTORY_FLUID_NONE);
    CHECK(steam->quantity == 1000U);
    CHECK(burner->released_energy == 0U);
    CHECK(event_count(s, FACTORY_EVENT_BOILER_CONVERSION_COMPLETED) == 1U);
    {
        const FactoryEvent *event = factory_simulation_get_event(s, 0U);
        CHECK(event != NULL && event->entity_id == 1U);
        CHECK(event != NULL && event->fluid_type == FACTORY_FLUID_WATER);
        CHECK(event != NULL
            && event->related_fluid_type == FACTORY_FLUID_STEAM);
        CHECK(event != NULL && event->quantity == 100U);
        CHECK(event != NULL && event->related_quantity == 100U);
    }
    CHECK(factory_simulation_get_boiler(s, 1U, &machine)
        == FACTORY_RESULT_OK);
    CHECK(machine.conversion_active);
    CHECK(factory_presentation_snapshot_rebuild(view, s)
        == FACTORY_RESULT_OK);
    {
        const FactoryPresentationEntity *entity =
            factory_presentation_snapshot_get_entity(view, 0U);
        CHECK(entity != NULL
            && entity->entity_type == FACTORY_ENTITY_TYPE_BOILER);
        CHECK(entity != NULL && entity->data.boiler.stored_water == 0U);
        CHECK(entity != NULL && entity->data.boiler.stored_steam == 1000U);
        CHECK(entity != NULL && entity->data.boiler.conversion_active);
        CHECK(entity != NULL
            && entity->data.boiler.burner.released_energy == 0U);
    }

    factory_presentation_snapshot_destroy(view);
    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

static void test_network_integration_and_snapshot_continuation(void)
{
    FactoryWorld *world;
    FactorySimulation *a = make_simulation(&world, 5U, 2U);
    FactorySimulation *b = NULL;
    FactorySnapshotBuffer snapshot = {0};
    FactorySnapshotBuffer loaded_snapshot = {0};
    FactoryFluidStorageInspection a_boiler, b_boiler;

    submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_FLUID_TANK,
        {.place_fluid_tank = {0, 0}}});                    /* 1 */
    submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe = {1, 0}}});                          /* 2 */
    submit(a, (FactoryCommand){FACTORY_COMMAND_PLACE_BOILER,
        {.place_boiler = {2, 0}}});                        /* 3 */
    CHECK(factory_simulation_submit_fluid_insert(
        a, 1U, FACTORY_FLUID_WATER, 1000U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    factory_burner_store_find_mutable(
        &a->burners, 3U)->released_energy = 300U;
    CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_fluid_storage_slot(
        a, 3U, FACTORY_FLUID_STORAGE_BOILER_OUTPUT, &a_boiler)
        == FACTORY_RESULT_OK);
    CHECK(a_boiler.fluid_type == FACTORY_FLUID_STEAM);
    CHECK(a_boiler.quantity == 100U);

    CHECK(factory_simulation_create_snapshot(a, &snapshot)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        snapshot.data, snapshot.size, &b) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(b, &loaded_snapshot)
        == FACTORY_RESULT_OK);
    CHECK(snapshot.size == loaded_snapshot.size);
    CHECK(snapshot.size == loaded_snapshot.size
        && memcmp(snapshot.data, loaded_snapshot.data, snapshot.size) == 0);
    factory_snapshot_buffer_destroy(&loaded_snapshot);
    for (uint32_t tick = 0U; tick < 3U; ++tick) {
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_event_count(a)
            == factory_simulation_get_event_count(b));
        for (size_t i = 0U;
             i < factory_simulation_get_event_count(a); ++i) {
            const FactoryEvent *x = factory_simulation_get_event(a, i);
            const FactoryEvent *y = factory_simulation_get_event(b, i);
            CHECK(x->type == y->type && x->tick == y->tick);
            CHECK(x->entity_id == y->entity_id);
            CHECK(x->fluid_type == y->fluid_type);
            CHECK(x->related_fluid_type == y->related_fluid_type);
            CHECK(x->quantity == y->quantity);
            CHECK(x->related_quantity == y->related_quantity);
        }
    }
    CHECK(factory_simulation_get_fluid_storage_slot(
        a, 3U, FACTORY_FLUID_STORAGE_BOILER_OUTPUT, &a_boiler)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_fluid_storage_slot(
        b, 3U, FACTORY_FLUID_STORAGE_BOILER_OUTPUT, &b_boiler)
        == FACTORY_RESULT_OK);
    CHECK(a_boiler.quantity == b_boiler.quantity);
    CHECK(a_boiler.fluid_type == b_boiler.fluid_type);

    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(world);
}

int main(void)
{
    test_recipe_and_water_extractor();
    test_boiler_atomic_conversion_and_presentation();
    test_network_integration_and_snapshot_continuation();
    if (failures != 0)
        (void)fprintf(stderr, "%d fluid machine test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
