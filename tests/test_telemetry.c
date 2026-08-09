#include <foundation/foundation.h>
#include <foundation/snapshot.h>

#include "power_fixture.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);++failures;}}while(false)

static FactorySimulation *make_line(FactoryWorld **out_world)
{
    FactoryWorld *w=factory_world_create(8U,5U);FactorySimulation*s;
    FactoryCommand commands[]={
        {FACTORY_COMMAND_PLACE_EXTRACTOR,{.place_extractor={0,0,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,{.place_belt={1,0,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_STORAGE,{.place_storage={2,0}}}};
    CHECK(factory_world_add_resource(w,0,0,FACTORY_RESOURCE_IRON,20U)==FACTORY_RESULT_OK);
    s=factory_simulation_create_with_construction_units(w,1000U);
    for(size_t i=0U;i<3U;++i)CHECK(factory_simulation_submit_command(s,&commands[i])==FACTORY_RESULT_OK);
    CHECK(factory_test_submit_power_pair(s,1,2,1,3));*out_world=w;return s;
}

static bool snapshots_equal(const FactorySimulation *a,const FactorySimulation *b)
{
    FactorySnapshotBuffer x={0},y={0};bool equal=false;
    if(factory_simulation_create_snapshot(a,&x)==FACTORY_RESULT_OK
        &&factory_simulation_create_snapshot(b,&y)==FACTORY_RESULT_OK)
        equal=x.size==y.size&&memcmp(x.data,y.data,x.size)==0;
    factory_snapshot_buffer_destroy(&x);factory_snapshot_buffer_destroy(&y);return equal;
}

static void test_flow_windows_and_clear(void)
{
    FactoryWorld*w;FactorySimulation*s=make_line(&w);FactoryTelemetryConfig c={4U,100U,32U};
    FactoryTelemetry*t=factory_telemetry_create(&c);uint64_t event_produced=0U,event_transferred=0U;
    for(size_t tick=0U;tick<50U;++tick){CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        for(size_t i=0U;i<factory_simulation_get_event_count(s);++i){const FactoryEvent*e=factory_simulation_get_event(s,i);
            if(e->type==FACTORY_EVENT_PRODUCTION_COMPLETED&&e->item_type==FACTORY_ITEM_IRON_ORE)event_produced+=e->quantity;
            if(e->type==FACTORY_EVENT_ITEM_TRANSFERRED&&e->item_type==FACTORY_ITEM_IRON_ORE)event_transferred+=e->quantity;}
        CHECK(factory_telemetry_observe_step(t,s)==FACTORY_TELEMETRY_RESULT_OK);}
    {FactoryTelemetryItemMetrics m;FactoryTelemetryEntityMetrics extractor,belt,storage;
        FactoryTelemetryEntityItemMetrics storage_item;
        CHECK(factory_telemetry_get_item_metrics(t,FACTORY_ITEM_IRON_ORE,FACTORY_TELEMETRY_WINDOW_LONG,&m));
        CHECK(m.produced_quantity==event_produced&&m.extracted_quantity==event_produced&&m.transferred_quantity==event_transferred);
        CHECK(factory_telemetry_get_entity_metrics(t,1U,FACTORY_TELEMETRY_WINDOW_LONG,&extractor));
        CHECK(extractor.completed_cycles==event_produced&&extractor.observed_ticks==50U);
        CHECK(factory_telemetry_get_entity_metrics(t,2U,FACTORY_TELEMETRY_WINDOW_SHORT,&belt));
        CHECK(belt.observed_ticks==4U&&belt.occupied_ticks+belt.empty_ticks==4U);
        CHECK(factory_telemetry_get_entity_metrics(t,3U,FACTORY_TELEMETRY_WINDOW_LONG,&storage));
        CHECK(factory_telemetry_get_entity_item_metrics(t,3U,FACTORY_ITEM_IRON_ORE,FACTORY_TELEMETRY_WINDOW_LONG,&storage_item));
        CHECK(storage.received_quantity==storage_item.received_quantity&&m.storage_inflow==storage_item.received_quantity);
        CHECK(storage.net_flow==(int64_t)(storage.received_quantity-storage.sent_quantity));
        CHECK(storage_item.net_flow==(int64_t)(storage_item.received_quantity-storage_item.sent_quantity));}
    {FactoryTelemetryItemMetrics before,after;
        CHECK(factory_telemetry_get_item_metrics(t,FACTORY_ITEM_IRON_ORE,FACTORY_TELEMETRY_WINDOW_LONG,&before));
        CHECK(factory_telemetry_observe_step(t,s)==FACTORY_TELEMETRY_RESULT_DUPLICATE);
        CHECK(factory_telemetry_get_item_metrics(t,FACTORY_ITEM_IRON_ORE,FACTORY_TELEMETRY_WINDOW_LONG,&after));
        CHECK(before.extracted_quantity==after.extracted_quantity
            &&before.produced_quantity==after.produced_quantity
            &&before.transferred_quantity==after.transferred_quantity
            &&before.storage_inflow==after.storage_inflow
            &&before.storage_outflow==after.storage_outflow
            &&before.observed_ticks==after.observed_ticks
            &&before.saturated==after.saturated);}
    {FactorySnapshotBuffer before={0},after={0};CHECK(factory_simulation_create_snapshot(s,&before)==FACTORY_RESULT_OK);
        factory_telemetry_clear(t);CHECK(factory_telemetry_get_last_observed_tick(t)==0U);
        CHECK(!factory_telemetry_get_item_metrics(t,FACTORY_ITEM_IRON_ORE,FACTORY_TELEMETRY_WINDOW_LONG,&(FactoryTelemetryItemMetrics){0}));
        CHECK(factory_simulation_create_snapshot(s,&after)==FACTORY_RESULT_OK);
        CHECK(before.size==after.size&&memcmp(before.data,after.data,before.size)==0);
        factory_snapshot_buffer_destroy(&before);factory_snapshot_buffer_destroy(&after);}
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);CHECK(factory_telemetry_observe_step(t,s)==FACTORY_TELEMETRY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_telemetry_observe_step(t,s)==FACTORY_TELEMETRY_RESULT_NON_SEQUENTIAL);
    factory_telemetry_destroy(t);factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_simulation_independence(void)
{
    FactoryWorld*wa,*wb;FactorySimulation*a=make_line(&wa),*b=make_line(&wb);
    FactoryTelemetryConfig c;factory_telemetry_default_config(&c);FactoryTelemetry*t=factory_telemetry_create(&c);
    for(size_t tick=0U;tick<40U;++tick){CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);CHECK(factory_simulation_tick(b)==FACTORY_RESULT_OK);
        CHECK(snapshots_equal(a,b));size_t event_count=factory_simulation_get_event_count(b);
        CHECK(factory_telemetry_observe_step(t,b)==FACTORY_TELEMETRY_RESULT_OK);
        CHECK(factory_simulation_get_event_count(b)==event_count);CHECK(snapshots_equal(a,b));}
    factory_telemetry_destroy(t);factory_simulation_destroy(a);factory_simulation_destroy(b);factory_world_destroy(wa);factory_world_destroy(wb);
}

int main(void)
{test_flow_windows_and_clear();test_simulation_independence();return failures==0?0:1;}
