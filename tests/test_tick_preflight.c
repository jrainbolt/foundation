#include "foundation/snapshot.h"

#include "simulation_internal.h"
#include "tick_preflight_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); ++failures; } } while(false)

static FactorySimulation *make(FactoryWorld **world)
{
    *world=factory_world_create(8U,8U);
    return factory_simulation_create_with_construction_units(*world,1000U);
}

static void queue(FactorySimulation *s,FactoryCommand c)
{
    CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
}

static void verify_failure_and_retry(FactoryCommand command,size_t failures_after,
    FactoryEntityType expected)
{
    FactoryWorld *world;
    FactorySimulation *s=make(&world);
    FactorySnapshotBuffer before={0},after={0};
    FactoryEvent prior;
    size_t prior_count;
    FactoryConstructionMaterial units;
    queue(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={0,0}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    prior_count=factory_simulation_get_event_count(s);
    prior=prior_count==0U?(FactoryEvent){0}:*factory_simulation_get_event(s,0U);
    units=factory_simulation_construction_units(s);
    queue(s,command);
    CHECK(factory_simulation_create_snapshot(s,&before)==FACTORY_RESULT_OK);
    factory_tick_preflight_test_fail_allocations_after(failures_after);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OUT_OF_MEMORY);
    factory_tick_preflight_test_fail_allocations_after(SIZE_MAX);
    CHECK(factory_simulation_get_tick(s)==1U);
    CHECK(factory_simulation_get_pending_command_count(s)==1U);
    CHECK(factory_simulation_get_entity_count(s)==1U);
    CHECK(factory_simulation_construction_units(s)==units);
    CHECK(factory_simulation_get_event_count(s)==prior_count);
    if (prior_count!=0U)
        CHECK(memcmp(factory_simulation_get_event(s,0U),&prior,sizeof(prior))==0);
    CHECK(factory_simulation_create_snapshot(s,&after)==FACTORY_RESULT_OK);
    CHECK(before.size==after.size
        && memcmp(before.data,after.data,before.size)==0);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_pending_command_count(s)==0U);
    CHECK(factory_simulation_get_tick(s)==2U);
    CHECK(factory_simulation_get_entity_count(s)==2U);
    CHECK(factory_simulation_get_command_result(s,0U)->entity_type==expected);
    factory_snapshot_buffer_destroy(&before);
    factory_snapshot_buffer_destroy(&after);
    factory_simulation_destroy(s); factory_world_destroy(world);
}

static void test_each_topology_failure(void)
{
    verify_failure_and_retry((FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={2,2}}},0U,FACTORY_ENTITY_TYPE_POWER_POLE);
    verify_failure_and_retry((FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={2,2}}},9U,FACTORY_ENTITY_TYPE_PIPE);
    verify_failure_and_retry((FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={2,2}}},14U,FACTORY_ENTITY_TYPE_HEAT_CONDUCTOR);
}

static void test_combined_batch_is_all_or_nothing(void)
{
    FactoryWorld *world;
    FactorySimulation *s=make(&world);
    queue(s,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={1,1}}});
    queue(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={2,2}}});
    queue(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={3,3}}});
    factory_tick_preflight_test_fail_allocations_after(15U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OUT_OF_MEMORY);
    factory_tick_preflight_test_fail_allocations_after(SIZE_MAX);
    CHECK(factory_simulation_get_entity_count(s)==0U);
    CHECK(factory_simulation_get_pending_command_count(s)==3U);
    CHECK(factory_simulation_get_tick(s)==0U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_entity_count(s)==3U);
    CHECK(factory_simulation_get_command_result_count(s)==3U);
    factory_simulation_destroy(s); factory_world_destroy(world);
}

static void test_configuration_batch_failure_and_retry(void)
{
    FactoryWorld *world;
    FactorySimulation *s=make(&world);
    FactorySnapshotBuffer before={0},after={0};
    FactoryAssembler assembler;
    FactoryStorage storage;
    queue(s,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
        {.place_assembler={0,0,FACTORY_DIRECTION_EAST}}});
    queue(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={1,0}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    queue(s,(FactoryCommand){FACTORY_COMMAND_SET_ASSEMBLER_RECIPE,
        {.set_assembler_recipe={1U,FACTORY_ASSEMBLER_RECIPE_IRON_GEAR}}});
    queue(s,(FactoryCommand){FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output={2U,FACTORY_ITEM_IRON_GEAR}}});
    CHECK(factory_simulation_create_snapshot(s,&before)==FACTORY_RESULT_OK);
    factory_tick_preflight_test_fail_allocations_after(0U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OUT_OF_MEMORY);
    factory_tick_preflight_test_fail_allocations_after(SIZE_MAX);
    CHECK(factory_simulation_get_tick(s)==1U);
    CHECK(factory_simulation_get_pending_command_count(s)==2U);
    CHECK(factory_simulation_get_assembler(s,1U,&assembler)
        && assembler.recipe_id==FACTORY_ASSEMBLER_RECIPE_NONE);
    CHECK(factory_simulation_get_storage(s,2U,&storage)
        && storage.configured_output_item==FACTORY_ITEM_NONE);
    CHECK(factory_simulation_create_snapshot(s,&after)==FACTORY_RESULT_OK);
    CHECK(before.size==after.size
        && memcmp(before.data,after.data,before.size)==0);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result_count(s)==2U);
    CHECK(factory_simulation_get_event_count(s)==2U);
    CHECK(factory_simulation_get_event(s,0U)->type
        ==FACTORY_EVENT_ASSEMBLER_RECIPE_CHANGED);
    CHECK(factory_simulation_get_event(s,1U)->type
        ==FACTORY_EVENT_STORAGE_OUTPUT_CHANGED);
    factory_snapshot_buffer_destroy(&before);
    factory_snapshot_buffer_destroy(&after);
    factory_simulation_destroy(s); factory_world_destroy(world);
}

int main(void)
{
    test_each_topology_failure();
    test_combined_batch_is_all_or_nothing();
    test_configuration_batch_failure_and_retry();
    return failures==0?0:1;
}
