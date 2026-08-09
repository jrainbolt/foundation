#include <foundation/foundation.h>
#include <foundation/snapshot.h>

#include "../src/simulation_internal.h"
#include "power_fixture.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);++failures;}}while(0)

static const FactoryCommandResult *result_at(const FactorySimulation *s,size_t i)
{return factory_simulation_get_command_result(s,i);}

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
    FactorySimulation *s=factory_simulation_create_with_construction_units(w,1000U);
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
    s->construction_depots.items[1].material_quantity=2U;
    FactoryCommand belt={FACTORY_COMMAND_PLACE_BELT,
        {.place_belt={2,2,FACTORY_DIRECTION_EAST}}};
    CHECK(factory_simulation_submit_command(s,&belt)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->result==FACTORY_RESULT_OK);
    CHECK(result_at(s,0)->construction_depot_id==2U);
    s->construction_depots.items[0].material_quantity=2U;
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

int main(void)
{test_radius_source_fifo_and_refund();test_physical_supply_snapshot_and_telemetry();
 test_multiple_depots_and_nonempty_demolition();return failures?1:0;}
