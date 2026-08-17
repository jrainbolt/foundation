#include <foundation/foundation.h>
#include <foundation/snapshot.h>

#include "../src/simulation_internal.h"
#include "../src/logistics_endpoint_internal.h"
#include "power_fixture.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);++failures;}}while(0)

static const FactoryCommandResult *result_at(const FactorySimulation *s,size_t i)
{return factory_simulation_get_command_result(s,i);}

static uint32_t distance_u32(int32_t ax,int32_t ay,int32_t bx,int32_t by)
{
    int32_t dx=ax-bx,dy=ay-by;
    return (uint32_t)(dx<0?-dx:dx)+(uint32_t)(dy<0?-dy:dy);
}

static uint32_t construction_material_in_transit(const FactorySimulation *s)
{
    uint32_t total=0U;
    for(size_t i=0U;i<s->belts.count;++i)
        if(s->belts.items[i].item==FACTORY_ITEM_CONSTRUCTION_MATERIAL)++total;
    for(size_t i=0U;i<s->inserters.count;++i)
        if(s->inserters.items[i].held_item==FACTORY_ITEM_CONSTRUCTION_MATERIAL)
            total+=s->inserters.items[i].held_amount;
    return total;
}

static void test_radius_source_fifo_and_refund(void)
{
    FactoryWorld *w=factory_world_create(24U,8U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(w,100U);
    FactoryCommand depot={FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={1,1}}};
    CHECK(factory_simulation_submit_command(s,&depot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK);
    CHECK(factory_simulation_construction_units(s)==0U);
    s->construction_depots.items[0].material_quantity=3U;
    FactoryCommand boundary={FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={9,1,FACTORY_DIRECTION_EAST}}};
    FactoryCommand beyond={FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={10,1,FACTORY_DIRECTION_EAST}}};
    CHECK(factory_simulation_submit_command(s,&boundary)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(s,&beyond)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->construction_depot_id==1U);
    CHECK(result_at(s,1)->result==FACTORY_RESULT_NO_CONSTRUCTION_SUPPLY);
    CHECK(s->construction_depots.items[0].material_quantity==2U);
    FactoryCommand a={FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={2,1,FACTORY_DIRECTION_EAST}}};
    FactoryCommand b={FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={3,1,FACTORY_DIRECTION_EAST}}};
    FactoryCommand c={FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={4,1,FACTORY_DIRECTION_EAST}}};
    CHECK(factory_simulation_submit_command(s,&a)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(s,&b)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK);
    CHECK(result_at(s,1)->result==FACTORY_RESULT_OK);
    CHECK(result_at(s,2)->result==FACTORY_RESULT_CONSTRUCTION_SUPPLY_INSUFFICIENT);
    FactoryCommand demolish={FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={result_at(s,0)->entity_id}}};
    CHECK(factory_simulation_submit_command(s,&demolish)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->construction_depot_id==1U);
    CHECK(s->construction_depots.items[0].material_quantity==1U);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_physical_supply_snapshot_and_telemetry(void)
{
    FactoryWorld *w=factory_world_create(16U,6U);
    CHECK(factory_world_add_resource(w,5,0,FACTORY_RESOURCE_IRON,50U)
        ==FACTORY_RESULT_OK);
    FactorySimulation *s=factory_simulation_create_with_construction_units(w,72U);
    FactoryCommand commands[]={
        {FACTORY_COMMAND_PLACE_STORAGE,{.place_storage={0,0}}},
        {FACTORY_COMMAND_PLACE_INSERTER,{.place_inserter={1,0,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,{.place_belt={2,0,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,{.place_belt={3,0,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_POWER_POLE,{.place_power_pole={4,2}}},
        {FACTORY_COMMAND_PLACE_POWER_GENERATOR,{.place_power_generator={4,3}}},
        {FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,{.place_construction_depot={4,0}}}};
    s->fixture_initial_generator_fuel=FACTORY_TEST_GENERATOR_FUEL_QUANTITY;
    for(size_t i=0U;i<sizeof(commands)/sizeof(commands[0]);++i)
        CHECK(factory_simulation_submit_command(s,&commands[i])==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    FactoryStorage *storage=&s->storages.items[0];
    storage->construction_material_amount=30U;
    FactoryCommand output={FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output={storage->entity_id,FACTORY_ITEM_CONSTRUCTION_MATERIAL}}};
    CHECK(factory_simulation_submit_command(s,&output)==FACTORY_RESULT_OK);
    FactoryTelemetryConfig config={4U,300U,32U};
    FactoryTelemetry *telemetry=factory_telemetry_create(&config);
    for(size_t tick=0U;tick<220U;++tick){
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_telemetry_observe_step(telemetry,s)==FACTORY_TELEMETRY_RESULT_OK);
    }
    FactoryConstructionDepot inspected;
    CHECK(factory_simulation_get_construction_depot(s,7U,&inspected));
    CHECK(inspected.material_quantity!=0U);
    FactoryTelemetryEntityItemMetrics flow;
    CHECK(factory_telemetry_get_entity_item_metrics(telemetry,7U,
        FACTORY_ITEM_CONSTRUCTION_MATERIAL,FACTORY_TELEMETRY_WINDOW_LONG,&flow));
    CHECK(flow.received_quantity==inspected.material_quantity);
    CHECK(inspected.material_quantity>=19U);
    uint32_t before_supply=inspected.material_quantity;
    FactoryCommand outpost[]={
        {FACTORY_COMMAND_PLACE_EXTRACTOR,
            {.place_extractor={5,0,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={6,0,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_STORAGE,{.place_storage={7,0}}}};
    for(size_t i=0U;i<3U;++i)
        CHECK(factory_simulation_submit_command(s,&outpost[i])==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_telemetry_observe_step(telemetry,s)==FACTORY_TELEMETRY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK);
    CHECK(result_at(s,1)->result==FACTORY_RESULT_OK);
    CHECK(result_at(s,2)->result==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_construction_depot(s,7U,&inspected));
    CHECK(inspected.material_quantity==before_supply-19U);
    for(size_t tick=0U;tick<120U;++tick){
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_telemetry_observe_step(telemetry,s)==FACTORY_TELEMETRY_RESULT_OK);
    }
    CHECK(factory_world_get_tile(w,5,0)->resource_amount<50U);
    CHECK(factory_storage_get_total_amount(&s->storages.items[1])!=0U);
    FactoryTelemetryEntityMetrics extractor_metrics;
    CHECK(factory_telemetry_get_entity_metrics(telemetry,8U,
        FACTORY_TELEMETRY_WINDOW_LONG,&extractor_metrics));
    CHECK(extractor_metrics.completed_cycles!=0U);
    FactorySnapshotBuffer snapshot={0};FactorySimulation *loaded=NULL;
    CHECK(factory_simulation_create_snapshot(s,&snapshot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snapshot.data,snapshot.size,&loaded)
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_construction_depot(loaded,7U,&inspected));
    CHECK(inspected.material_quantity==before_supply-19U);
    factory_snapshot_buffer_destroy(&snapshot);factory_simulation_destroy(loaded);
    factory_telemetry_destroy(telemetry);factory_simulation_destroy(s);
    factory_world_destroy(w);
}

static void test_multiple_depots_and_nonempty_demolition(void)
{
    FactoryWorld *w=factory_world_create(20U,8U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(w,525U);
    FactoryCommand first={FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={1,1}}};
    CHECK(factory_simulation_submit_command(s,&first)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    FactoryCommand second={FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={3,1}}};
    CHECK(factory_simulation_submit_command(s,&second)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(s->construction_depots.count==2U);
    s->construction_depots.items[0].material_quantity=0U;
    s->construction_depots.items[1].material_quantity=4U;
    FactoryCommand belt={FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={2,2,FACTORY_DIRECTION_EAST}}};
    CHECK(factory_simulation_submit_command(s,&belt)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->construction_depot_id==2U);
    CHECK(factory_logistics_endpoint_transfer(s,
        (FactoryLogisticsEndpoint){2U,FACTORY_LOGISTICS_SLOT_OUTPUT},
        (FactoryLogisticsEndpoint){1U,
            FACTORY_LOGISTICS_SLOT_CONSTRUCTION_DEPOT_INPUT},
        FACTORY_ITEM_CONSTRUCTION_MATERIAL)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_transfer(s,
        (FactoryLogisticsEndpoint){2U,FACTORY_LOGISTICS_SLOT_OUTPUT},
        (FactoryLogisticsEndpoint){1U,
            FACTORY_LOGISTICS_SLOT_CONSTRUCTION_DEPOT_INPUT},
        FACTORY_ITEM_CONSTRUCTION_MATERIAL)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(s->construction_depots.items[0].material_quantity==2U);
    FactoryCommand belt2={FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={2,3,FACTORY_DIRECTION_EAST}}};
    CHECK(factory_simulation_submit_command(s,&belt2)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->construction_depot_id==1U);
    FactoryCommand demolish={FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={2U}}};
    CHECK(factory_simulation_submit_command(s,&demolish)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_CONSTRUCTION_DEPOT_NOT_EMPTY);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_bootstrap_lifecycle_and_final_depot(void)
{
    FactoryWorld *w=factory_world_create(20U,8U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(w,
        FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT+500U);
    FactoryCommand depot={FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={1,1}}};
    CHECK(!factory_simulation_construction_bootstrap_completed(s));
    CHECK(factory_simulation_submit_command(s,&depot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK);
    CHECK(factory_simulation_construction_bootstrap_completed(s));
    CHECK(factory_simulation_construction_units(s)==0U);
    CHECK(s->construction_depots.count==1U);
    CHECK(s->construction_depots.items[0].material_quantity==500U);

    /* Emptying the final depot does not make its refund globally owned. */
    s->construction_depots.items[0].material_quantity=0U;
    FactoryCommand demolish={FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={1U}}};
    CHECK(factory_simulation_submit_command(s,&demolish)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_NO_CONSTRUCTION_SUPPLY);
    CHECK(s->construction_depots.count==1U);
    CHECK(factory_simulation_construction_units(s)==0U);
    CHECK(factory_simulation_construction_bootstrap_completed(s));

    FactorySnapshotBuffer snapshot={0};FactorySimulation *loaded=NULL;
    CHECK(factory_simulation_create_snapshot(s,&snapshot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snapshot.data,snapshot.size,&loaded)
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_construction_bootstrap_completed(loaded));
    CHECK(factory_simulation_construction_units(loaded)==0U);
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_bootstrap_excess_rejected_atomically(void)
{
    FactoryWorld *w=factory_world_create(8U,8U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(w,
        FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT+501U);
    FactoryCommand depot={FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={1,1}}};
    CHECK(factory_simulation_submit_command(s,&depot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_CONSTRUCTION_INVENTORY_OVERFLOW);
    CHECK(factory_simulation_get_entity_count(s)==0U);
    CHECK(s->construction_depots.count==0U);
    CHECK(factory_simulation_construction_units(s)
        ==FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT+501U);
    CHECK(!factory_simulation_construction_bootstrap_completed(s));
    CHECK(factory_simulation_get_event_count(s)==0U);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_bootstrap_preflight_failure_is_transactional(void)
{
    FactoryWorld *w=factory_world_create(8U,8U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(w,
        FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT+100U);
    FactorySnapshotBuffer before={0},after={0};
    FactoryCommand depot={FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={1,1}}};
    CHECK(factory_simulation_submit_command(s,&depot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(s,&before)==FACTORY_RESULT_OK);
    factory_tick_preflight_test_fail_allocations_after(0U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OUT_OF_MEMORY);
    factory_tick_preflight_test_fail_allocations_after(SIZE_MAX);
    CHECK(factory_simulation_create_snapshot(s,&after)==FACTORY_RESULT_OK);
    CHECK(before.size==after.size&&memcmp(before.data,after.data,before.size)==0);
    CHECK(!factory_simulation_construction_bootstrap_completed(s));
    CHECK(s->construction_depots.count==0U);
    CHECK(factory_simulation_get_pending_command_count(s)==1U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_construction_bootstrap_completed(s));
    CHECK(s->construction_depots.items[0].material_quantity==100U);
    factory_snapshot_buffer_destroy(&before);
    factory_snapshot_buffer_destroy(&after);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_generated_remote_outpost(void)
{
    FactoryWorldGenerationConfig config;
    FactoryWorld *w=factory_world_create_with_seed(64U,48U,UINT64_C(6));
    int32_t sx,sy,rx=0,ry=0,route_x,depot_x;
    uint32_t best=UINT32_MAX,initial_cost=0U,transported=30U;
    FactoryResourceQuantity starting_quantity=0U;
    FactoryCommand commands[FACTORY_COMMAND_QUEUE_CAPACITY];size_t count=0U;
    factory_world_generation_default_config(&config);
    config.water_threshold=0U;config.rock_threshold=0U;
    CHECK(factory_world_generate(w,&config)==FACTORY_RESULT_OK);
    sx=factory_world_get_start_x(w);sy=factory_world_get_start_y(w);
    for(int32_t y=3;y<45;++y)for(int32_t x=7;x<57;++x){
        const FactoryTile *tile=factory_world_get_tile(w,x,y);
        uint32_t distance=distance_u32(sx,sy,x,y);
        if(tile->resource==FACTORY_RESOURCE_COAL
            &&distance>11U&&distance<best){
            best=distance;rx=x;ry=y;starting_quantity=tile->resource_amount;
        }
    }
    CHECK(best!=UINT32_MAX);
    int32_t horizontal=rx>=sx?1:-1;
    int32_t vertical=ry>=sy?1:-1;
    FactoryDirection horizontal_direction=horizontal>0
        ?FACTORY_DIRECTION_EAST:FACTORY_DIRECTION_WEST;
    FactoryDirection vertical_direction=vertical>0
        ?FACTORY_DIRECTION_SOUTH:FACTORY_DIRECTION_NORTH;
    route_x=rx+horizontal*4;depot_x=route_x+horizontal*2;

#define ADD_COMMAND(value,cost) do {commands[count++]=(value);initial_cost+=(cost);} while(false)
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={sx,sy}}}),FACTORY_CONSTRUCTION_COST_STORAGE);
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter={sx+horizontal,sy,horizontal_direction}}}),
        FACTORY_CONSTRUCTION_COST_INSERTER);
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={sx,sy+1}}}),FACTORY_CONSTRUCTION_COST_POWER_POLE);
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator={sx,sy+2}}}),
        FACTORY_CONSTRUCTION_COST_POWER_GENERATOR);
    for(int32_t x=sx+horizontal*2;;x+=horizontal){
        FactoryDirection direction=x==route_x&&ry!=sy
            ?vertical_direction:horizontal_direction;
        ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={x,sy,direction}}}),FACTORY_CONSTRUCTION_COST_BELT);
        if(x==route_x)break;
    }
    if(ry!=sy)for(int32_t y=sy+vertical;;y+=vertical){
        FactoryDirection direction=y==ry
            ?horizontal_direction:vertical_direction;
        ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={route_x,y,direction}}}),FACTORY_CONSTRUCTION_COST_BELT);
        if(y==ry)break;
    }
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter={route_x+horizontal,ry,horizontal_direction}}}),
        FACTORY_CONSTRUCTION_COST_INSERTER);
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={rx,ry+1,FACTORY_DIRECTION_SOUTH}}}),
        FACTORY_CONSTRUCTION_COST_BELT);
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={rx,ry+2}}}),FACTORY_CONSTRUCTION_COST_STORAGE);
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={rx+horizontal,ry+1}}}),
        FACTORY_CONSTRUCTION_COST_POWER_POLE);
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={route_x+horizontal,ry+vertical}}}),
        FACTORY_CONSTRUCTION_COST_POWER_POLE);
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator={rx+horizontal,ry+2}}}),
        FACTORY_CONSTRUCTION_COST_POWER_GENERATOR);
    size_t depot_command=count;
    ADD_COMMAND(((FactoryCommand){FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={depot_x,ry}}}),
        FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT);
#undef ADD_COMMAND
    CHECK(count<FACTORY_COMMAND_QUEUE_CAPACITY);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,initial_cost);
    s->fixture_initial_generator_fuel=FACTORY_TEST_GENERATOR_FUEL_QUANTITY;
    for(size_t i=0U;i<count;++i)
        CHECK(factory_simulation_submit_command(s,&commands[i])==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    for(size_t i=0U;i<count;++i)CHECK(result_at(s,i)->result==FACTORY_RESULT_OK);
    FactoryEntityId depot_id=result_at(s,depot_command)->entity_id;
    CHECK(factory_simulation_construction_bootstrap_completed(s));
    CHECK(factory_simulation_construction_units(s)==0U);
    CHECK(s->construction_depots.items[0].material_quantity==0U);

    FactoryCommand extractor={FACTORY_COMMAND_PLACE_EXTRACTOR,
        {.place_extractor={rx,ry,FACTORY_DIRECTION_SOUTH}}};
    CHECK(factory_simulation_submit_command(s,&extractor)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_CONSTRUCTION_SUPPLY_INSUFFICIENT);
    CHECK(s->construction_depots.items[0].material_quantity==0U);

    s->storages.items[0].construction_material_amount=transported;
    FactoryCommand output={FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output={s->storages.items[0].entity_id,
            FACTORY_ITEM_CONSTRUCTION_MATERIAL}}};
    CHECK(factory_simulation_submit_command(s,&output)==FACTORY_RESULT_OK);
    FactoryTelemetryConfig telemetry_config={4U,1200U,128U};
    FactoryTelemetry *telemetry=factory_telemetry_create(&telemetry_config);
    size_t supply_ticks=0U;bool loaded_in_transit=false;
    while(s->construction_depots.items[0].material_quantity<transported
        &&supply_ticks<1100U){
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_telemetry_observe_step(telemetry,s)==FACTORY_TELEMETRY_RESULT_OK);
        ++supply_ticks;
        if(!loaded_in_transit
            &&s->construction_depots.items[0].material_quantity>0U
            &&s->construction_depots.items[0].material_quantity<transported){
            FactorySnapshotBuffer snapshot={0};FactorySimulation *loaded=NULL;
            CHECK(factory_simulation_create_snapshot(s,&snapshot)==FACTORY_RESULT_OK);
            CHECK(factory_simulation_load_snapshot(snapshot.data,snapshot.size,
                &loaded)==FACTORY_RESULT_OK);
            factory_snapshot_buffer_destroy(&snapshot);
            factory_simulation_destroy(s);s=loaded;loaded_in_transit=true;
        }
    }
    CHECK(s->construction_depots.items[0].material_quantity==transported);
    CHECK(loaded_in_transit);
    FactoryTelemetryEntityItemMetrics depot_flow;
    CHECK(factory_telemetry_get_entity_item_metrics(telemetry,depot_id,
        FACTORY_ITEM_CONSTRUCTION_MATERIAL,FACTORY_TELEMETRY_WINDOW_LONG,
        &depot_flow));
    CHECK(depot_flow.received_quantity==transported);

    CHECK(factory_simulation_submit_command(s,&extractor)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_telemetry_observe_step(telemetry,s)==FACTORY_TELEMETRY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK);
    FactoryEntityId extractor_id=result_at(s,0)->entity_id;
    CHECK(result_at(s,0)->construction_depot_id==depot_id);
    CHECK(s->construction_depots.items[0].material_quantity
        ==transported-FACTORY_CONSTRUCTION_COST_EXTRACTOR);
    for(size_t i=0U;i<160U;++i){
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_telemetry_observe_step(telemetry,s)==FACTORY_TELEMETRY_RESULT_OK);
    }
    CHECK(factory_storage_get_total_amount(&s->storages.items[1])!=0U);
    FactoryTelemetryEntityMetrics extractor_metrics;
    CHECK(factory_telemetry_get_entity_metrics(telemetry,extractor_id,
        FACTORY_TELEMETRY_WINDOW_LONG,&extractor_metrics));
    CHECK(extractor_metrics.completed_cycles!=0U);
    CHECK(initial_cost+transported==initial_cost
        +s->construction_depots.items[0].material_quantity
        +FACTORY_CONSTRUCTION_COST_EXTRACTOR);
    printf("remote Coal outpost: patch=(%d,%d) start=(%d,%d) distance=%u initial_quantity=%u material=%u belts=%zu supply_ticks=%zu production=%llu storage_inflow=%u\n",
        rx,ry,sx,sy,best,starting_quantity,transported,count-11U,supply_ticks,
        (unsigned long long)extractor_metrics.completed_cycles,
        factory_storage_get_total_amount(&s->storages.items[1]));
    factory_telemetry_destroy(telemetry);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_depot_output_physical_relay_and_contention(void)
{
    FactoryWorld *w=factory_world_create(20U,8U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(w,
        FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT+500U);
    s->fixture_initial_generator_fuel=FACTORY_TEST_GENERATOR_FUEL_QUANTITY;
    FactoryCommand commands[]={
        {FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
            {.place_construction_depot={1,1}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={2,1,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={3,1,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={4,1,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_INSERTER,
            {.place_inserter={5,1,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_POWER_POLE,
            {.place_power_pole={3,3}}},
        {FACTORY_COMMAND_PLACE_POWER_GENERATOR,
            {.place_power_generator={3,4}}},
        {FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
            {.place_construction_depot={6,1}}}
    };
    for(size_t i=0U;i<sizeof(commands)/sizeof(commands[0]);++i)
        CHECK(factory_simulation_submit_command(s,&commands[i])
            ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    for(size_t i=0U;i<sizeof(commands)/sizeof(commands[0]);++i)
        CHECK(result_at(s,i)->result==FACTORY_RESULT_OK);
    CHECK(s->construction_depots.items[0].material_quantity==432U);
    CHECK(s->construction_depots.items[1].material_quantity==0U);
    FactoryTelemetryConfig telemetry_config={4U,300U,32U};
    FactoryTelemetry *telemetry=factory_telemetry_create(&telemetry_config);
    for(size_t tick=0U;tick<300U
        &&s->construction_depots.items[1].material_quantity<10U;++tick){
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_telemetry_observe_step(telemetry,s)
            ==FACTORY_TELEMETRY_RESULT_OK);
    }
    CHECK(s->construction_depots.items[1].material_quantity==10U);
    CHECK(s->construction_depots.items[0].material_quantity
        +s->construction_depots.items[1].material_quantity
        +construction_material_in_transit(s)==432U);
    FactoryTelemetryEntityItemMetrics sent={0},received={0};
    CHECK(factory_telemetry_get_entity_item_metrics(telemetry,1U,
        FACTORY_ITEM_CONSTRUCTION_MATERIAL,FACTORY_TELEMETRY_WINDOW_LONG,&sent));
    CHECK(factory_telemetry_get_entity_item_metrics(telemetry,8U,
        FACTORY_ITEM_CONSTRUCTION_MATERIAL,FACTORY_TELEMETRY_WINDOW_LONG,
        &received));
    CHECK(sent.sent_quantity==10U+construction_material_in_transit(s)
        &&received.received_quantity==10U);
    FactoryCommand remote={FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={14,1,FACTORY_DIRECTION_EAST}}};
    CHECK(factory_simulation_submit_command(s,&remote)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK
        &&result_at(s,0)->construction_depot_id==8U);
    CHECK(s->construction_depots.items[1].material_quantity==9U);

    /* Commands spend first.  Once A has exactly 20, a 15-unit Refinery leaves
     * at most five units for the already-connected export logistics. */
    s->construction_depots.items[0].material_quantity=0U;
    for(size_t tick=0U;tick<240U;++tick)
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(construction_material_in_transit(s)==0U);
    s->construction_depots.items[0].material_quantity=20U;
    s->construction_depots.items[1].material_quantity=0U;
    FactoryCommand refinery={FACTORY_COMMAND_PLACE_REFINERY,
        {.place_refinery={1,2,FACTORY_DIRECTION_EAST,
            FACTORY_DIRECTION_WEST}}};
    CHECK(factory_simulation_submit_command(s,&refinery)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK
        &&result_at(s,0)->construction_depot_id==1U);
    CHECK(s->construction_depots.items[0].material_quantity<=5U);
    for(size_t tick=0U;tick<240U;++tick)
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(s->construction_depots.items[0].material_quantity==0U
        &&s->construction_depots.items[1].material_quantity==5U);
    CHECK(15U+s->construction_depots.items[0].material_quantity
        +s->construction_depots.items[1].material_quantity==20U);
    factory_telemetry_destroy(telemetry);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_depot_storage_and_belt_endpoints(void)
{
    FactoryWorld *w=factory_world_create(12U,6U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(w,
        FACTORY_CONSTRUCTION_COST_CONSTRUCTION_DEPOT+500U);
    FactoryCommand first={FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={1,1}}};
    CHECK(factory_simulation_submit_command(s,&first)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    FactoryCommand commands[]={
        {FACTORY_COMMAND_PLACE_STORAGE,{.place_storage={2,1}}},
        {FACTORY_COMMAND_PLACE_BELT,
            {.place_belt={3,1,FACTORY_DIRECTION_EAST}}},
        {FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
            {.place_construction_depot={4,1}}}
    };
    for(size_t i=0U;i<3U;++i)
        CHECK(factory_simulation_submit_command(s,&commands[i])
            ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    FactoryEntityId storage=result_at(s,0)->entity_id;
    FactoryEntityId belt=result_at(s,1)->entity_id;
    FactoryEntityId relay=result_at(s,2)->entity_id;
    CHECK(factory_logistics_endpoint_transfer(s,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_OUTPUT},
        (FactoryLogisticsEndpoint){storage,FACTORY_LOGISTICS_SLOT_STORAGE_INPUT},
        FACTORY_ITEM_CONSTRUCTION_MATERIAL)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(s->storages.items[0].construction_material_amount==1U);
    FactoryCommand output={FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output={storage,FACTORY_ITEM_CONSTRUCTION_MATERIAL}}};
    CHECK(factory_simulation_submit_command(s,&output)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_logistics_endpoint_transfer(s,
        (FactoryLogisticsEndpoint){storage,FACTORY_LOGISTICS_SLOT_STORAGE_OUTPUT},
        (FactoryLogisticsEndpoint){relay,
            FACTORY_LOGISTICS_SLOT_CONSTRUCTION_DEPOT_INPUT},
        FACTORY_ITEM_CONSTRUCTION_MATERIAL)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_transfer(s,
        (FactoryLogisticsEndpoint){1U,FACTORY_LOGISTICS_SLOT_OUTPUT},
        (FactoryLogisticsEndpoint){belt,FACTORY_LOGISTICS_SLOT_MAIN},
        FACTORY_ITEM_CONSTRUCTION_MATERIAL)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(factory_logistics_endpoint_transfer(s,
        (FactoryLogisticsEndpoint){belt,FACTORY_LOGISTICS_SLOT_MAIN},
        (FactoryLogisticsEndpoint){relay,
            FACTORY_LOGISTICS_SLOT_CONSTRUCTION_DEPOT_INPUT},
        FACTORY_ITEM_CONSTRUCTION_MATERIAL)==FACTORY_LOGISTICS_RESULT_OK);
    CHECK(s->construction_depots.items[1].material_quantity==2U);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

int main(void)
{test_radius_source_fifo_and_refund();test_physical_supply_snapshot_and_telemetry();
 test_multiple_depots_and_nonempty_demolition();
 test_bootstrap_lifecycle_and_final_depot();
 test_bootstrap_excess_rejected_atomically();
 test_bootstrap_preflight_failure_is_transactional();
 test_generated_remote_outpost();
 test_depot_output_physical_relay_and_contention();
 test_depot_storage_and_belt_endpoints();return failures?1:0;}
