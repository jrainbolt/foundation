#include "foundation/presentation.h"
#include "foundation/snapshot.h"

#include "logistics_endpoint_internal.h"
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

static void submit(FactorySimulation *s,FactoryCommand command)
{ CHECK(factory_simulation_submit_command(s,&command)==FACTORY_RESULT_OK); }

static void test_definitions(void)
{
    const FactoryTechnologyDefinition *root=
        factory_technology_definition_get(FACTORY_TECHNOLOGY_BASIC_AUTOMATION);
    const FactoryTechnologyDefinition *dependent=
        factory_technology_definition_at(1U);
    FactoryTechnologyDefinition cyclic[2]={
        {1U,{2U,0U},1U,FACTORY_ITEM_BASIC_SCIENCE,1U,1U,1U,
            FACTORY_UNLOCK_AUTOMATION},
        {2U,{1U,0U},1U,FACTORY_ITEM_BASIC_SCIENCE,1U,1U,1U,
            FACTORY_UNLOCK_FLUID_HANDLING}};
    CHECK(factory_technology_definition_count()==2U);
    CHECK(root!=NULL && root->prerequisite_count==0U
        && root->science_quantity_per_unit==2U
        && root->required_science_units==2U && root->work_ticks_per_unit==3U);
    CHECK(dependent!=NULL && dependent->id==FACTORY_TECHNOLOGY_FLUID_HANDLING
        && dependent->prerequisites[0]==FACTORY_TECHNOLOGY_BASIC_AUTOMATION);
    CHECK(factory_technology_definition_get(99U)==NULL);
    CHECK(factory_technology_definition_at(2U)==NULL);
    CHECK(!factory_technology_definitions_validate(cyclic,2U));
}

static void test_selection_progress_completion_and_prerequisites(void)
{
    FactoryWorld *world; FactorySimulation *s=make(&world);
    FactoryTechnologyProgressInspection progress;
    CHECK(factory_simulation_get_active_research(s)==FACTORY_TECHNOLOGY_NONE);
    CHECK(factory_simulation_get_completed_technology_count(s)==0U);
    CHECK(factory_simulation_get_technology_progress(s,99U,&progress)
        ==FACTORY_RESULT_TECHNOLOGY_INVALID);
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={99U}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_FLUID_HANDLING}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_INSERT_RESEARCH_SCIENCE,
        {.insert_research_science={4U}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_TECHNOLOGY_INVALID);
    CHECK(factory_simulation_get_command_result(s,1U)->result
        ==FACTORY_RESULT_TECHNOLOGY_PREREQUISITES_MISSING);
    CHECK(factory_simulation_get_command_result(s,3U)->result==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(s)==1U);
    CHECK(factory_simulation_get_event(s,0U)->type==FACTORY_EVENT_RESEARCH_SELECTED);
    CHECK(factory_simulation_get_event(s,0U)->technology_id
        ==FACTORY_TECHNOLOGY_BASIC_AUTOMATION);
    CHECK(factory_simulation_get_research_science_quantity(s)==2U);
    CHECK(factory_simulation_get_technology_progress(s,
        FACTORY_TECHNOLOGY_BASIC_AUTOMATION,&progress)==FACTORY_RESULT_OK);
    CHECK(progress.work_ticks_in_current_unit==1U
        && progress.science_committed_for_current_unit);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(s)==1U
        && factory_simulation_get_event(s,0U)->type
            ==FACTORY_EVENT_RESEARCH_UNIT_COMPLETED);
    CHECK(factory_simulation_get_event(s,0U)->related_quantity==1U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_science_quantity(s)==0U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(s)==2U);
    CHECK(factory_simulation_get_event(s,0U)->type
        ==FACTORY_EVENT_RESEARCH_UNIT_COMPLETED);
    CHECK(factory_simulation_get_event(s,1U)->type
        ==FACTORY_EVENT_TECHNOLOGY_COMPLETED);
    CHECK(factory_simulation_get_active_research(s)==FACTORY_TECHNOLOGY_NONE);
    CHECK(factory_simulation_is_technology_completed(s,
        FACTORY_TECHNOLOGY_BASIC_AUTOMATION));
    CHECK(factory_simulation_has_unlock(s,FACTORY_UNLOCK_AUTOMATION));
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_FLUID_HANDLING}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_TECHNOLOGY_ALREADY_COMPLETED);
    CHECK(factory_simulation_get_command_result(s,1U)->result==FACTORY_RESULT_OK);
    factory_simulation_destroy(s); factory_world_destroy(world);
}

static void test_item_logistics_snapshot_presentation_and_continuation(void)
{
    FactoryWorld *world; FactorySimulation *a=make(&world),*b=NULL;
    FactorySnapshotBuffer snapshot={0}; FactoryPresentationSnapshot *view;
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={1,1}}});
    CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(a,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_STORAGE_INPUT},
        FACTORY_ITEM_BASIC_SCIENCE)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(a->storages.items[0].basic_science_amount==1U);
    submit(a,(FactoryCommand){FACTORY_COMMAND_INSERT_RESEARCH_SCIENCE,
        {.insert_research_science={4U}}});
    submit(a,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(a,&snapshot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snapshot.data,snapshot.size,&b)
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_active_research(b)
        ==FACTORY_TECHNOLOGY_BASIC_AUTOMATION);
    for (size_t i=0U;i<5U;++i) {
        FactorySnapshotBuffer sa={0},sb={0};
        CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_event_count(a)==
            factory_simulation_get_event_count(b));
        CHECK(factory_simulation_create_snapshot(a,&sa)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_create_snapshot(b,&sb)==FACTORY_RESULT_OK);
        CHECK(sa.size==sb.size && memcmp(sa.data,sb.data,sa.size)==0);
        factory_snapshot_buffer_destroy(&sa); factory_snapshot_buffer_destroy(&sb);
    }
    view=factory_presentation_snapshot_create();
    CHECK(factory_presentation_snapshot_rebuild(view,b)==FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_get_completed_technology_count(view)==1U);
    CHECK(factory_presentation_snapshot_get_research_science_quantity(view)==0U);
    factory_presentation_snapshot_destroy(view);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(b); factory_simulation_destroy(a);
    factory_world_destroy(world);
}

static void test_preflight_preserves_research_commands(void)
{
    FactoryWorld *world; FactorySimulation *s=make(&world);
    FactorySnapshotBuffer before={0},after={0};
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={1,1}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_INSERT_RESEARCH_SCIENCE,
        {.insert_research_science={4U}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    CHECK(factory_simulation_create_snapshot(s,&before)==FACTORY_RESULT_OK);
    factory_tick_preflight_test_fail_allocations_after(0U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OUT_OF_MEMORY);
    factory_tick_preflight_test_fail_allocations_after(SIZE_MAX);
    CHECK(factory_simulation_get_tick(s)==0U
        && factory_simulation_get_pending_command_count(s)==3U
        && factory_simulation_get_active_research(s)==FACTORY_TECHNOLOGY_NONE
        && factory_simulation_get_research_science_quantity(s)==0U);
    CHECK(factory_simulation_create_snapshot(s,&after)==FACTORY_RESULT_OK);
    CHECK(before.size==after.size && memcmp(before.data,after.data,before.size)==0);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_active_research(s)
        ==FACTORY_TECHNOLOGY_BASIC_AUTOMATION);
    factory_snapshot_buffer_destroy(&before); factory_snapshot_buffer_destroy(&after);
    factory_simulation_destroy(s); factory_world_destroy(world);
}

int main(void)
{
    test_definitions();
    test_selection_progress_completion_and_prerequisites();
    test_item_logistics_snapshot_presentation_and_continuation();
    test_preflight_preserves_research_commands();
    return failures==0?0:1;
}
