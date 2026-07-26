#include "foundation/presentation.h"
#include "foundation/snapshot.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *s, FactoryCommand c)
{
    CHECK(factory_simulation_submit_command(s, &c) == FACTORY_RESULT_OK);
}

static FactoryCommand tank(int32_t x, int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_FLUID_TANK, {.place_fluid_tank = {x, y}}};
}

static FactoryCommand pipe_at(int32_t x, int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_PIPE, {.place_pipe = {x, y}}};
}

static size_t event_type_count(
    const FactorySimulation *s, FactoryEventType type
)
{
    size_t count = 0U;
    for (size_t i = 0U; i < factory_simulation_get_event_count(s); ++i)
        if (factory_simulation_get_event(s, i)->type == type) ++count;
    return count;
}

static void build_line(FactorySimulation *s)
{
    submit(s, tank(0, 0));       /* 1 */
    submit(s, pipe_at(1, 0));    /* 2 */
    submit(s, pipe_at(2, 0));    /* 3 */
    submit(s, pipe_at(3, 0));    /* 4 */
    submit(s, tank(4, 0));       /* 5 */
    CHECK(factory_simulation_submit_fluid_insert(
        s, 1U, FACTORY_FLUID_WATER, 1000U) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
}

static void test_line_transfer_presentation_and_split(void)
{
    FactoryWorld *w = factory_world_create(6U, 2U);
    FactorySimulation *s =
        factory_simulation_create_with_construction_units(w, UINT32_MAX);
    FactoryFluidStorageInspection a, b;
    FactoryPipeInspection p;
    FactoryFluidNetworkInspection n;
    FactoryPresentationSnapshot *view =
        factory_presentation_snapshot_create();
    size_t pipe_views = 0U;

    build_line(s);
    CHECK(event_type_count(s, FACTORY_EVENT_FLUID_NETWORK_CREATED) == 1U);
    CHECK(event_type_count(s, FACTORY_EVENT_PIPE_CONNECTED) == 1U);
    CHECK(factory_simulation_get_entity_count(s) == 5U);
    CHECK(factory_simulation_get_fluid_network_count(s) == 1U);
    CHECK(factory_simulation_get_fluid_network(s, 0U, &n)
        == FACTORY_RESULT_OK);
    CHECK(n.network_id == 2U && n.pipe_count == 3U && n.port_count == 2U);
    CHECK(factory_simulation_get_pipe(s, 3U, &p) == FACTORY_RESULT_OK);
    CHECK(p.connection_mask
        == (FACTORY_FLUID_CONNECTION_EAST
            | FACTORY_FLUID_CONNECTION_WEST));
    CHECK(p.network_id == 2U);
    CHECK(factory_simulation_get_fluid_storage(s, 1U, &a)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_fluid_storage(s, 5U, &b)
        == FACTORY_RESULT_OK);
    CHECK(a.quantity == 900U && b.quantity == 100U);
    CHECK(a.quantity + b.quantity == 1000U);
    CHECK(a.network_id == 2U && b.network_id == 2U);

    CHECK(factory_presentation_snapshot_rebuild(view, s)
        == FACTORY_RESULT_OK);
    for (size_t i = 0U;
        i < factory_presentation_snapshot_get_entity_count(view); ++i) {
        const FactoryPresentationEntity *e =
            factory_presentation_snapshot_get_entity(view, i);
        if (e->entity_type == FACTORY_ENTITY_TYPE_PIPE) {
            ++pipe_views;
            CHECK(e->data.pipe.network_id == 2U);
        }
    }
    CHECK(pipe_views == 3U);

    submit(s, (FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY, {.demolish_entity = {3U}}});
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(event_type_count(s, FACTORY_EVENT_FLUID_NETWORK_SPLIT) == 1U);
    CHECK(event_type_count(s, FACTORY_EVENT_PIPE_DISCONNECTED) == 1U);
    CHECK(factory_simulation_get_fluid_network_count(s) == 2U);
    CHECK(factory_simulation_get_fluid_storage(s, 1U, &a)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_fluid_storage(s, 5U, &b)
        == FACTORY_RESULT_OK);
    CHECK(a.quantity + b.quantity == 1000U);

    factory_presentation_snapshot_destroy(view);
    factory_simulation_destroy(s);
    factory_world_destroy(w);
}

static void test_snapshot_continuation(void)
{
    FactoryWorld *w = factory_world_create(6U, 2U);
    FactorySimulation *a =
        factory_simulation_create_with_construction_units(w, UINT32_MAX);
    FactorySimulation *b = NULL;
    FactorySnapshotBuffer snap = {0}, x = {0}, y = {0};
    FactoryFluidStorageInspection ax, bx;
    build_line(a);
    CHECK(factory_simulation_create_snapshot(a, &snap) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snap.data, snap.size, &b)
        == FACTORY_RESULT_OK);
    for (size_t tick = 0U; tick < 5U; ++tick) {
        CHECK(factory_simulation_tick(a) == FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b) == FACTORY_RESULT_OK);
    }
    CHECK(factory_simulation_get_fluid_storage(a, 1U, &ax)
        == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_fluid_storage(b, 1U, &bx)
        == FACTORY_RESULT_OK);
    CHECK(ax.quantity == bx.quantity && ax.network_id == bx.network_id);
    CHECK(factory_simulation_create_snapshot(a, &x) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(b, &y) == FACTORY_RESULT_OK);
    CHECK(x.size == y.size && memcmp(x.data, y.data, x.size) == 0);
    factory_snapshot_buffer_destroy(&y);
    factory_snapshot_buffer_destroy(&x);
    factory_snapshot_buffer_destroy(&snap);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(w);
}

static void test_masks_and_merge(void)
{
    FactoryWorld *w = factory_world_create(7U, 7U);
    FactorySimulation *s =
        factory_simulation_create_with_construction_units(w, UINT32_MAX);
    FactoryPipeInspection center;
    submit(s, pipe_at(3, 3)); /* 1 center */
    submit(s, pipe_at(3, 2)); /* 2 north */
    submit(s, pipe_at(4, 3)); /* 3 east */
    submit(s, pipe_at(3, 4)); /* 4 south */
    submit(s, pipe_at(2, 3)); /* 5 west */
    submit(s, pipe_at(0, 0)); /* 6 isolated */
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_pipe(s, 1U, &center) == FACTORY_RESULT_OK);
    CHECK(center.connection_mask == FACTORY_FLUID_CONNECTION_ALL);
    CHECK(factory_simulation_get_fluid_network_count(s) == 2U);

    submit(s, pipe_at(1, 0)); /* 7 extends isolated component */
    submit(s, pipe_at(2, 0)); /* 8 */
    submit(s, pipe_at(2, 1)); /* 9 bridges to west arm at (2,3) next */
    submit(s, pipe_at(2, 2)); /* 10 merge bridge */
    CHECK(factory_simulation_tick(s) == FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_fluid_network_count(s) == 1U);
    CHECK(event_type_count(s, FACTORY_EVENT_FLUID_NETWORK_MERGED) == 1U);
    factory_simulation_destroy(s);
    factory_world_destroy(w);
}

int main(void)
{
    test_line_transfer_presentation_and_split();
    test_snapshot_continuation();
    test_masks_and_merge();
    if (failures != 0) return 1;
    (void)puts("fluid network tests passed");
    return 0;
}
