#include "foundation/foundation.h"
#include "foundation/snapshot.h"

#include "power_fixture.h"
#include "simulation_internal.h"
#include "logistics_endpoint_internal.h"
#include "tick_preflight_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",            \
    __FILE__,__LINE__,#c);++failures;}}while(false)

static void submit(FactorySimulation*s,FactoryCommand c)
{CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);}

static void test_depletion_event_presentation_and_replacement(void)
{
    FactoryWorld*w=factory_world_create(6U,5U);
    FactorySimulation*s;FactoryResourceDepositInspection deposit;
    FactoryPresentationSnapshot*view;const FactoryPresentationResource*r;
    CHECK(factory_world_add_resource(w,0,0,FACTORY_RESOURCE_IRON,1U)
        ==FACTORY_RESULT_OK);
    CHECK(factory_world_get_resource_deposit(w,0,0,&deposit)==FACTORY_RESULT_OK);
    CHECK(deposit.remaining_quantity==1U&&!deposit.depleted
        && deposit.occupying_entity_id==0U);
    CHECK(!factory_resource_deposit_is_depleted(w,0,0));
    s=factory_simulation_create_with_construction_units(w,1000U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor={0,0,FACTORY_DIRECTION_EAST}}});
    CHECK(factory_test_submit_power_pair(s,1,1,1,2));
    for(size_t tick=0U;tick<FACTORY_EXTRACTOR_PRODUCTION_TICKS;++tick)
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_resource_deposit_is_depleted(w,0,0));
    CHECK(factory_simulation_get_event_count(s)==2U);
    CHECK(factory_simulation_get_event(s,0U)->type
        ==FACTORY_EVENT_PRODUCTION_COMPLETED);
    {const FactoryEvent*e=factory_simulation_get_event(s,1U);
        CHECK(e!=NULL&&e->type==FACTORY_EVENT_RESOURCE_DEPLETED
            &&e->entity_id==1U&&e->resource_type==FACTORY_RESOURCE_IRON
            &&e->x==0&&e->y==0
            &&e->tick==FACTORY_EXTRACTOR_PRODUCTION_TICKS-1U);}
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(s)==0U);
    {FactoryExtractor extractor;CHECK(factory_simulation_get_extractor(s,1U,&extractor));
        CHECK(extractor.production_progress==0U&&extractor.output_amount==1U);}
    view=factory_presentation_snapshot_create();
    CHECK(factory_presentation_snapshot_rebuild(view,s)==FACTORY_RESULT_OK);
    r=factory_presentation_snapshot_get_resource(view,0U);
    CHECK(r!=NULL&&r->remaining_quantity==0U&&r->depleted
        &&r->occupying_entity_id==1U);
    CHECK(factory_presentation_snapshot_get_entity(view,0U)->status
        ==FACTORY_PRESENTATION_MACHINE_STATUS_BLOCKED_OUTPUT);
    factory_presentation_snapshot_destroy(view);
    /* Move the final item out before demolition; depletion itself never
       prevents removal or restores the deposit. */
    CHECK(factory_logistics_endpoint_remove(s,(FactoryLogisticsEndpoint){1U,
        FACTORY_LOGISTICS_SLOT_OUTPUT},FACTORY_ITEM_IRON_ORE)
        ==FACTORY_LOGISTICS_RESULT_OK);
    view=factory_presentation_snapshot_create();
    CHECK(factory_presentation_snapshot_rebuild(view,s)==FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_get_entity(view,0U)->status
        ==FACTORY_PRESENTATION_MACHINE_STATUS_DEPLETED_RESOURCE);
    factory_presentation_snapshot_destroy(view);
    submit(s,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_world_get_resource_deposit(w,0,0,&deposit)==FACTORY_RESULT_OK);
    CHECK(deposit.depleted&&deposit.remaining_quantity==0U
        &&deposit.occupying_entity_id==0U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor={0,0,FACTORY_DIRECTION_EAST}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result==FACTORY_RESULT_OK);
    for(size_t i=0U;i<FACTORY_EXTRACTOR_PRODUCTION_TICKS+2U;++i)
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    {FactoryExtractor replacement;CHECK(factory_simulation_get_extractor(s,4U,&replacement));
        CHECK(replacement.production_progress==0U&&replacement.output_amount==0U);}
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_logistics_conservation_and_snapshot(void)
{
    FactoryWorld*w=factory_world_create(8U,5U);FactorySimulation*a,*b=NULL;
    FactorySnapshotBuffer snap={0},x={0},y={0};
    CHECK(factory_world_add_resource(w,0,0,FACTORY_RESOURCE_COPPER,3U)
        ==FACTORY_RESULT_OK);
    a=factory_simulation_create_with_construction_units(w,1000U);
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor={0,0,FACTORY_DIRECTION_EAST}}});
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={1,0,FACTORY_DIRECTION_EAST}}});
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={2,0}}});
    CHECK(factory_test_submit_power_pair(a,1,1,1,2));
    for(size_t i=0U;i<90U;++i)CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    CHECK(factory_world_get_tile(w,0,0)->resource_amount==0U);
    {const FactoryExtractor*e=factory_extractor_store_find(&a->extractors,1U);
     const FactoryBelt*belt=factory_belt_store_find(&a->belts,2U);
     const FactoryStorage*storage=factory_storage_store_find(&a->storages,3U);
     uint32_t total=(e->output_item==FACTORY_ITEM_COPPER_ORE?e->output_amount:0U)
        +(belt->item==FACTORY_ITEM_COPPER_ORE?1U:0U)
        +storage->copper_ore_amount;
     CHECK(total==3U);}
    CHECK(factory_simulation_create_snapshot(a,&snap)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snap.data,snap.size,&b)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(b)==0U);
    for(size_t i=0U;i<5U;++i){CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b)==FACTORY_RESULT_OK);}
    CHECK(factory_simulation_create_snapshot(a,&x)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(b,&y)==FACTORY_RESULT_OK);
    CHECK(x.size==y.size&&memcmp(x.data,y.data,x.size)==0);
    factory_snapshot_buffer_destroy(&y);factory_snapshot_buffer_destroy(&x);
    factory_snapshot_buffer_destroy(&snap);factory_simulation_destroy(b);
    factory_simulation_destroy(a);factory_world_destroy(w);
}

static void test_preflight_preserves_final_unit(void)
{
    FactoryWorld*w=factory_world_create(5U,5U);FactorySimulation*s;
    FactorySnapshotBuffer before={0},after={0};FactoryExtractor extractor;
    CHECK(factory_world_add_resource(w,0,0,FACTORY_RESOURCE_IRON,1U)
        ==FACTORY_RESULT_OK);
    s=factory_simulation_create_with_construction_units(w,1000U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor={0,0,FACTORY_DIRECTION_EAST}}});
    CHECK(factory_test_submit_power_pair(s,1,1,1,2));
    for(size_t i=0U;i<FACTORY_EXTRACTOR_PRODUCTION_TICKS-1U;++i)
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_extractor(s,1U,&extractor));
    CHECK(extractor.production_progress==FACTORY_EXTRACTOR_PRODUCTION_TICKS-1U);
    CHECK(factory_simulation_create_snapshot(s,&before)==FACTORY_RESULT_OK);
    factory_tick_preflight_test_fail_allocations_after(0U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OUT_OF_MEMORY);
    factory_tick_preflight_test_fail_allocations_after(SIZE_MAX);
    CHECK(factory_world_get_tile(w,0,0)->resource_amount==1U);
    CHECK(factory_simulation_get_extractor(s,1U,&extractor));
    CHECK(extractor.production_progress==FACTORY_EXTRACTOR_PRODUCTION_TICKS-1U);
    CHECK(factory_simulation_create_snapshot(s,&after)==FACTORY_RESULT_OK);
    CHECK(before.size==after.size&&memcmp(before.data,after.data,before.size)==0);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_world_get_tile(w,0,0)->resource_amount==0U);
    factory_snapshot_buffer_destroy(&after);factory_snapshot_buffer_destroy(&before);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_generated_deposit_depletes_normally(void)
{
    FactoryWorldGenerationConfig config;FactoryWorld*w;
    FactorySimulation*s;int32_t rx=-1,ry=-1,px=-1,py=-1,gx=-1,gy=-1;
    factory_world_generation_default_config(&config);
    config.starter_quantity=1U;config.remote_patch_count=0U;
    w=factory_world_create_with_seed(64U,48U,UINT64_C(42));
    CHECK(factory_world_generate(w,&config)==FACTORY_RESULT_OK);
    for(int32_t y=0;y<48&&rx<0;++y)for(int32_t x=0;x<64;++x)
        if(factory_world_get_tile(w,x,y)->resource==FACTORY_RESOURCE_IRON){rx=x;ry=y;break;}
    for(int32_t y=0;y<48&&gx<0;++y)for(int32_t x=0;x<64;++x){
        const FactoryTile*t=factory_world_get_tile(w,x,y);
        if(t->terrain!=FACTORY_TERRAIN_GROUND||t->resource!=FACTORY_RESOURCE_NONE)continue;
        if(px<0&&abs(x-rx)<=2&&abs(y-ry)<=2){px=x;py=y;continue;}
        if(px>=0&&abs(x-px)<=2&&abs(y-py)<=2){gx=x;gy=y;break;}
    }
    CHECK(rx>=0&&px>=0&&gx>=0);
    s=factory_simulation_create_with_construction_units(w,1000U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor={rx,ry,FACTORY_DIRECTION_EAST}}});
    CHECK(factory_test_submit_power_pair(s,px,py,gx,gy));
    for(size_t i=0U;i<FACTORY_EXTRACTOR_PRODUCTION_TICKS;++i)
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_world_get_tile(w,rx,ry)->resource_amount==0U);
    {FactoryExtractor e;CHECK(factory_simulation_get_extractor(s,1U,&e));
        CHECK(e.output_item==FACTORY_ITEM_IRON_ORE&&e.output_amount==1U);}
    CHECK(factory_simulation_get_event_count(s)==2U);
    CHECK(factory_simulation_get_event(s,1U)->type==FACTORY_EVENT_RESOURCE_DEPLETED);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

int main(void)
{
    test_depletion_event_presentation_and_replacement();
    test_logistics_conservation_and_snapshot();
    test_preflight_preserves_final_unit();
    test_generated_deposit_depletes_normally();
    return failures==0?0:1;
}
