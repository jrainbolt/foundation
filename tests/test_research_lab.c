#include "foundation/foundation.h"
#include "foundation/snapshot.h"

#include "logistics_endpoint_internal.h"
#include "power_fixture.h"

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
{CHECK(factory_simulation_submit_command(s,&command)==FACTORY_RESULT_OK);}

static void place_powered_lab(FactorySimulation *s)
{
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RESEARCH_LAB,
        {.place_research_lab={1,1}}});
    CHECK(factory_test_submit_power_pair(s,2,1,2,2));
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result==FACTORY_RESULT_OK);
}

static void test_inventory_power_progress_and_events(void)
{
    FactoryWorld *world; FactorySimulation *s=make(&world);
    FactoryResearchLabInspection lab;
    place_powered_lab(s);
    CHECK(factory_simulation_get_research_lab(s,1U,&lab)==FACTORY_RESULT_OK);
    CHECK(lab.science_quantity==0U && lab.science_capacity==100U
        && lab.connected && lab.powered
        && lab.activity==FACTORY_RESEARCH_LAB_NO_ACTIVE_RESEARCH);
    CHECK(factory_logistics_endpoint_insert(s,(FactoryLogisticsEndpoint){1U,
        FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},FACTORY_ITEM_IRON_ORE)
        ==FACTORY_LOGISTICS_RESULT_INCOMPATIBLE_ITEM);
    CHECK(factory_logistics_endpoint_insert(s,(FactoryLogisticsEndpoint){1U,
        FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},FACTORY_ITEM_BASIC_SCIENCE)
        ==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_insert(s,(FactoryLogisticsEndpoint){1U,
        FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},FACTORY_ITEM_BASIC_SCIENCE)
        ==FACTORY_LOGISTICS_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(s,1U,&lab)==FACTORY_RESULT_OK);
    CHECK(lab.activity==FACTORY_RESEARCH_LAB_WORKING
        && lab.science_quantity==0U && lab.science_consumed_last_tick==2U
        && lab.work_contributed_last_tick==1U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(s)==1U);
    CHECK(factory_simulation_get_event(s,0U)->type
        ==FACTORY_EVENT_RESEARCH_UNIT_COMPLETED);
    CHECK(factory_simulation_get_event(s,0U)->entity_id==1U);
    CHECK(factory_simulation_get_event(s,0U)->quantity==2U);
    factory_simulation_destroy(s); factory_world_destroy(world);
}

static void test_lowest_id_single_worker_and_snapshot(void)
{
    FactoryWorld *world; FactorySimulation *a=make(&world),*b=NULL;
    FactorySnapshotBuffer snap={0}; FactoryResearchLabInspection first,second;
    place_powered_lab(a);
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_RESEARCH_LAB,
        {.place_research_lab={3,1}}});
    CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    for (size_t n=0U;n<4U;++n) {
        FactoryEntityId id=n<2U?1U:4U;
        CHECK(factory_logistics_endpoint_insert(a,(FactoryLogisticsEndpoint){id,
            FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},
            FACTORY_ITEM_BASIC_SCIENCE)==FACTORY_LOGISTICS_RESULT_OK);
    }
    submit(a,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(a,1U,&first)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(a,4U,&second)==FACTORY_RESULT_OK);
    CHECK(first.activity==FACTORY_RESEARCH_LAB_WORKING
        && second.activity==FACTORY_RESEARCH_LAB_WAITING
        && first.work_contributed_last_tick==1U
        && second.work_contributed_last_tick==0U);
    CHECK(factory_simulation_create_snapshot(a,&snap)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snap.data,snap.size,&b)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(b,1U,&first)==FACTORY_RESULT_OK);
    CHECK(first.science_quantity==0U && first.science_consumed_last_tick==0U
        && first.work_contributed_last_tick==0U);
    for(size_t n=0U;n<5U;++n){
        FactorySnapshotBuffer x={0},y={0};
        CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_create_snapshot(a,&x)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_create_snapshot(b,&y)==FACTORY_RESULT_OK);
        CHECK(x.size==y.size && memcmp(x.data,y.data,x.size)==0);
        factory_snapshot_buffer_destroy(&x);factory_snapshot_buffer_destroy(&y);
    }
    factory_snapshot_buffer_destroy(&snap);factory_simulation_destroy(b);
    factory_simulation_destroy(a);factory_world_destroy(world);
}

static void test_demolition_inventory_guard(void)
{
    FactoryWorld *world; FactorySimulation *s=make(&world);
    place_powered_lab(s);
    CHECK(factory_logistics_endpoint_insert(s,(FactoryLogisticsEndpoint){1U,
        FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},FACTORY_ITEM_BASIC_SCIENCE)
        ==FACTORY_LOGISTICS_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    CHECK(factory_simulation_entity_is_valid(s,1U));
    factory_simulation_destroy(s);factory_world_destroy(world);
}

static void test_global_commitment_and_eligibility(void)
{
    FactoryWorld *world; FactorySimulation *s=make(&world);
    FactoryResearchLabInspection low,high;
    FactoryTechnologyProgressInspection progress;
    place_powered_lab(s);
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RESEARCH_LAB,
        {.place_research_lab={3,1}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    for(size_t i=0U;i<2U;++i) CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){4U,FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},
        FACTORY_ITEM_BASIC_SCIENCE)==FACTORY_LOGISTICS_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(s,1U,&low)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(s,4U,&high)==FACTORY_RESULT_OK);
    CHECK(low.activity==FACTORY_RESEARCH_LAB_NO_SCIENCE);
    CHECK(high.activity==FACTORY_RESEARCH_LAB_WORKING
        && high.science_consumed_last_tick==2U);
    CHECK(factory_simulation_get_technology_progress(s,
        FACTORY_TECHNOLOGY_BASIC_AUTOMATION,&progress)==FACTORY_RESULT_OK);
    CHECK(progress.science_committed_for_current_unit
        && progress.work_ticks_in_current_unit==1U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(s,1U,&low)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_research_lab(s,4U,&high)==FACTORY_RESULT_OK);
    CHECK(low.activity==FACTORY_RESEARCH_LAB_WORKING
        && low.science_consumed_last_tick==0U);
    CHECK(high.activity==FACTORY_RESEARCH_LAB_WAITING
        && high.science_consumed_last_tick==0U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(s)>=2U);
    CHECK(factory_simulation_get_event(s,
        factory_simulation_get_event_count(s)-1U)->type
        ==FACTORY_EVENT_RESEARCH_UNIT_COMPLETED);
    CHECK(factory_simulation_get_event(s,
        factory_simulation_get_event_count(s)-1U)->entity_id==4U);
    factory_simulation_destroy(s);factory_world_destroy(world);
}

static uint32_t science_total(FactorySimulation *s,FactoryEntityId storage_id,
    FactoryEntityId inserter_id,FactoryEntityId lab_id)
{
    FactoryResearchLabInspection lab={0};
    const FactoryStorage *storage=factory_storage_store_find(&s->storages,
        storage_id);
    const FactoryInserter *inserter=factory_inserter_store_find(&s->inserters,
        inserter_id);
    (void)factory_simulation_get_research_lab(s,lab_id,&lab);
    return (storage==NULL?0U:storage->basic_science_amount
            +(storage->output_occupied?storage->output_item
                ==FACTORY_ITEM_BASIC_SCIENCE?1U:0U:0U))
        +(inserter!=NULL&&inserter->held_item==FACTORY_ITEM_BASIC_SCIENCE
            ?inserter->held_amount:0U)+lab.science_quantity;
}

static void test_storage_inserter_lab_timing(void)
{
    FactoryWorld *world; FactorySimulation *s=make(&world);
    FactoryResearchLabInspection lab;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={1,1}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter={2,1,FACTORY_DIRECTION_EAST}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RESEARCH_LAB,
        {.place_research_lab={3,1}}});
    CHECK(factory_test_submit_power_pair(s,2,2,2,3));
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    for(size_t i=0U;i<2U;++i) CHECK(factory_logistics_endpoint_insert(s,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_STORAGE_INPUT},
        FACTORY_ITEM_BASIC_SCIENCE)==FACTORY_LOGISTICS_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output={1U,FACTORY_ITEM_BASIC_SCIENCE}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    for(size_t tick=0U;tick<12U;++tick){
        FactoryTechnologyProgressInspection p;
        uint32_t before=science_total(s,1U,2U,3U);
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_technology_progress(s,
            FACTORY_TECHNOLOGY_BASIC_AUTOMATION,&p)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_research_lab(s,3U,&lab)==FACTORY_RESULT_OK);
        if(!p.science_committed_for_current_unit) CHECK(science_total(s,1U,2U,3U)==before);
        else {CHECK(before==2U && science_total(s,1U,2U,3U)==0U
                && lab.science_consumed_last_tick==2U);break;}
    }
    factory_simulation_destroy(s);factory_world_destroy(world);
}

static void test_power_loss_and_committed_snapshot_continuation(void)
{
    FactoryWorld *world=factory_world_create(20U,8U);
    FactorySimulation *a=factory_simulation_create_with_construction_units(
        world,1000U),*b=NULL;
    FactorySnapshotBuffer checkpoint={0};
    FactoryResearchLabInspection second;
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_RESEARCH_LAB,
        {.place_research_lab={1,1}}});
    CHECK(factory_test_submit_power_pair(a,2,1,2,2));
    CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_RESEARCH_LAB,
        {.place_research_lab={16,1}}});
    CHECK(factory_test_submit_power_pair(a,17,1,17,2));
    CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    for(size_t i=0U;i<2U;++i) CHECK(factory_logistics_endpoint_insert(a,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_RESEARCH_LAB_INPUT},
        FACTORY_ITEM_BASIC_SCIENCE)==FACTORY_LOGISTICS_RESULT_OK);
    submit(a,(FactoryCommand){FACTORY_COMMAND_SELECT_RESEARCH,
        {.select_research={FACTORY_TECHNOLOGY_BASIC_AUTOMATION}}});
    CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(a,&checkpoint)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(checkpoint.data,checkpoint.size,&b)
        ==FACTORY_RESULT_OK);
    for(size_t run=0U;run<2U;++run){
        FactorySimulation *s=run==0U?a:b;
        FactoryResearchLabInspection first;
        submit(s,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
            {.demolish_entity={2U}}});
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_research_lab(s,1U,&first)
            ==FACTORY_RESULT_OK);
        CHECK(first.activity==FACTORY_RESEARCH_LAB_UNPOWERED);
        CHECK(factory_simulation_get_research_lab(s,4U,&second)
            ==FACTORY_RESULT_OK);
        CHECK(second.activity==FACTORY_RESEARCH_LAB_WORKING
            && second.science_quantity==0U
            && second.science_consumed_last_tick==0U);
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_event_count(s)==1U
            && factory_simulation_get_event(s,0U)->type
                ==FACTORY_EVENT_RESEARCH_UNIT_COMPLETED
            && factory_simulation_get_event(s,0U)->entity_id==4U);
    }
    {FactorySnapshotBuffer x={0},y={0};
        CHECK(factory_simulation_create_snapshot(a,&x)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_create_snapshot(b,&y)==FACTORY_RESULT_OK);
        CHECK(x.size==y.size&&memcmp(x.data,y.data,x.size)==0);
        factory_snapshot_buffer_destroy(&x);factory_snapshot_buffer_destroy(&y);}
    factory_snapshot_buffer_destroy(&checkpoint);
    factory_simulation_destroy(b);factory_simulation_destroy(a);
    factory_world_destroy(world);
}

int main(void)
{
    test_inventory_power_progress_and_events();
    test_lowest_id_single_worker_and_snapshot();
    test_demolition_inventory_guard();
    test_global_commitment_and_eligibility();
    test_storage_inserter_lab_timing();
    test_power_loss_and_committed_snapshot_continuation();
    return failures==0?0:1;
}
