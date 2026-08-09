#include <foundation/foundation.h>
#include <foundation/snapshot.h>

#include "../src/simulation_internal.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);++failures;}}while(false)

static FactoryEntityId place(FactorySimulation*s,int32_t x,int32_t y,
    FactoryRailGeometry g)
{
    FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL,
        {.place_rail={x,y,(uint32_t)g}}};
    CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    const FactoryCommandResult*r=factory_simulation_get_command_result(s,0U);
    CHECK(r!=NULL&&r->result==FACTORY_RESULT_OK);return r!=NULL?r->entity_id:0U;
}

static void test_masks_and_connections(void)
{
    CHECK(factory_rail_geometry_port_mask(FACTORY_RAIL_HORIZONTAL)==
        (FACTORY_RAIL_PORT_EAST|FACTORY_RAIL_PORT_WEST));
    CHECK(factory_rail_geometry_port_mask(FACTORY_RAIL_VERTICAL)==
        (FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_SOUTH));
    CHECK(factory_rail_geometry_port_mask(FACTORY_RAIL_CURVE_NE)==
        (FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_EAST));
    CHECK(factory_rail_geometry_port_mask(FACTORY_RAIL_CURVE_NW)==
        (FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_WEST));
    CHECK(factory_rail_geometry_port_mask(FACTORY_RAIL_CURVE_SE)==
        (FACTORY_RAIL_PORT_SOUTH|FACTORY_RAIL_PORT_EAST));
    CHECK(factory_rail_geometry_port_mask(FACTORY_RAIL_CURVE_SW)==
        (FACTORY_RAIL_PORT_SOUTH|FACTORY_RAIL_PORT_WEST));
    CHECK(!factory_rail_geometry_is_valid((FactoryRailGeometry)99));
    FactoryWorld*w=factory_world_create(8U,8U);
    FactorySimulation*s=factory_simulation_create_with_construction_units(w,100U);
    FactoryEntityId a=place(s,1,1,FACTORY_RAIL_HORIZONTAL);
    FactoryEntityId b=place(s,2,1,FACTORY_RAIL_HORIZONTAL);
    FactoryEntityId c=place(s,1,2,FACTORY_RAIL_HORIZONTAL);
    FactoryRailInspection ia,ib,ic;
    CHECK(factory_simulation_get_rail(s,a,&ia));CHECK(factory_simulation_get_rail(s,b,&ib));
    CHECK(factory_simulation_get_rail(s,c,&ic));
    CHECK(ia.neighbors[FACTORY_DIRECTION_EAST]==b&&ib.neighbors[FACTORY_DIRECTION_WEST]==a);
    CHECK(ia.network_id==a&&ib.network_id==a&&ic.network_id==c);
    CHECK(ia.connection_count==1U&&ic.connection_count==0U);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_loop_station_snapshot_and_split(void)
{
    static const struct{int32_t x,y;FactoryRailGeometry g;} loop[]={
        {1,1,FACTORY_RAIL_CURVE_SE},{2,1,FACTORY_RAIL_HORIZONTAL},
        {3,1,FACTORY_RAIL_CURVE_SW},{1,2,FACTORY_RAIL_VERTICAL},
        {3,2,FACTORY_RAIL_VERTICAL},{1,3,FACTORY_RAIL_CURVE_NE},
        {2,3,FACTORY_RAIL_HORIZONTAL},{3,3,FACTORY_RAIL_CURVE_NW}};
    FactoryWorld*w=factory_world_create(8U,8U);
    FactorySimulation*s=factory_simulation_create_with_construction_units(w,200U);
    FactoryEntityId ids[8];
    for(size_t i=0U;i<8U;++i)ids[i]=place(s,loop[i].x,loop[i].y,loop[i].g);
    CHECK(factory_simulation_get_rail_network_count(s)==1U);
    CHECK(factory_simulation_get_rail_network(s,0U)->network_id==ids[0]);
    CHECK(factory_simulation_get_rail_network(s,0U)->rail_count==8U);
    for(size_t i=0U;i<8U;++i){FactoryRailInspection r;
        CHECK(factory_simulation_get_rail(s,ids[i],&r));CHECK(r.connection_count==2U);}
    FactoryCommand station={FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={2,0,FACTORY_DIRECTION_SOUTH}}};
    CHECK(factory_simulation_submit_command(s,&station)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    FactoryEntityId station_id=factory_simulation_get_command_result(s,0U)->entity_id;
    FactoryRailStationInspection st;
    CHECK(factory_simulation_get_rail_station(s,station_id,&st));
    CHECK(st.connected&&st.attached_rail_id==ids[1]&&st.network_id==ids[0]);
    FactorySnapshotBuffer bytes={0};FactorySimulation*loaded=NULL;
    CHECK(factory_simulation_create_snapshot(s,&bytes)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(bytes.data,bytes.size,&loaded)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_rail_station(loaded,station_id,&st)&&st.connected);
    FactoryCommand demolish={FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={ids[1]}}};
    CHECK(factory_simulation_submit_command(loaded,&demolish)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(loaded)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_rail_station(loaded,station_id,&st)&&!st.connected);
    CHECK(factory_simulation_get_rail_network_count(loaded)==1U);
    CHECK(place(loaded,2,1,FACTORY_RAIL_HORIZONTAL)!=0U);
    CHECK(factory_simulation_get_rail_station(loaded,station_id,&st)&&st.connected);
    factory_snapshot_buffer_destroy(&bytes);factory_simulation_destroy(loaded);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_terrain_and_station_rejection(void)
{
    FactoryWorld*w=factory_world_create(5U,5U);
    CHECK(factory_world_initialize_terrain(w,1,1,FACTORY_TERRAIN_WATER)==FACTORY_RESULT_OK);
    CHECK(factory_world_initialize_terrain(w,2,1,FACTORY_TERRAIN_ROCK)==FACTORY_RESULT_OK);
    FactorySimulation*s=factory_simulation_create_with_construction_units(w,100U);
    FactoryCommand rail={FACTORY_COMMAND_PLACE_RAIL,{.place_rail={1,1,0U}}};
    CHECK(factory_simulation_submit_command(s,&rail)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result==FACTORY_RESULT_TERRAIN_BLOCKED);
    rail.data.place_rail.x=2;
    CHECK(factory_simulation_submit_command(s,&rail)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result==FACTORY_RESULT_TERRAIN_BLOCKED);
    FactoryCommand station={FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={3,3,FACTORY_DIRECTION_NORTH}}};
    CHECK(factory_simulation_submit_command(s,&station)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result==FACTORY_RESULT_INVALID_STATE);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_construction_depot_supply(void)
{
    FactoryWorld*w=factory_world_create(16U,6U);
    FactorySimulation*s=factory_simulation_create_with_construction_units(w,100U);
    FactoryCommand depot={FACTORY_COMMAND_PLACE_CONSTRUCTION_DEPOT,
        {.place_construction_depot={1,1}}};
    CHECK(factory_simulation_submit_command(s,&depot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(s->construction_depots.count==1U);
    FactoryConstructionMaterial before=s->construction_depots.items[0].material_quantity;
    FactoryEntityId rail=place(s,2,1,FACTORY_RAIL_HORIZONTAL);
    CHECK(s->construction_depots.items[0].material_quantity
        ==before-FACTORY_CONSTRUCTION_COST_RAIL);
    FactoryCommand station={FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={2,2,FACTORY_DIRECTION_NORTH}}};
    CHECK(factory_simulation_submit_command(s,&station)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result==FACTORY_RESULT_OK);
    CHECK(s->construction_depots.items[0].material_quantity
        ==before-FACTORY_CONSTRUCTION_COST_RAIL
            -FACTORY_CONSTRUCTION_COST_RAIL_STATION);
    FactoryCommand remote={FACTORY_COMMAND_PLACE_RAIL,
        {.place_rail={15,5,FACTORY_RAIL_HORIZONTAL}}};
    CHECK(factory_simulation_submit_command(s,&remote)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_NO_CONSTRUCTION_SUPPLY);
    FactoryRailInspection inspection;
    CHECK(factory_simulation_get_rail(s,rail,&inspection));
    CHECK(s->construction_depots.items[0].material_quantity
        ==before-FACTORY_CONSTRUCTION_COST_RAIL
            -FACTORY_CONSTRUCTION_COST_RAIL_STATION);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_network_merge_split_and_store_reorder(void)
{
    FactoryWorld*w=factory_world_create(8U,2U);
    FactorySimulation*s=factory_simulation_create_with_construction_units(w,100U);
    FactoryEntityId left=place(s,0,0,FACTORY_RAIL_HORIZONTAL);
    (void)place(s,1,0,FACTORY_RAIL_HORIZONTAL);
    FactoryEntityId right=place(s,3,0,FACTORY_RAIL_HORIZONTAL);
    (void)place(s,4,0,FACTORY_RAIL_HORIZONTAL);
    CHECK(factory_simulation_get_rail_network_count(s)==2U);
    FactoryEntityId bridge=place(s,2,0,FACTORY_RAIL_HORIZONTAL);
    CHECK(factory_simulation_get_rail_network_count(s)==1U);
    CHECK(factory_simulation_get_rail_network(s,0U)->network_id==left);
    FactoryRail temporary=s->rails.items[0];
    s->rails.items[0]=s->rails.items[s->rails.count-1U];
    s->rails.items[s->rails.count-1U]=temporary;
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_rail_network_count(s)==1U);
    CHECK(factory_simulation_get_rail_network(s,0U)->network_id==left);
    FactoryCommand demolish={FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={bridge}}};
    CHECK(factory_simulation_submit_command(s,&demolish)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_rail_network_count(s)==2U);
    FactoryRailInspection inspection;
    CHECK(factory_simulation_get_rail(s,right,&inspection));
    CHECK(inspection.network_id==right);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

int main(void){test_masks_and_connections();test_loop_station_snapshot_and_split();
    test_terrain_and_station_rejection();test_construction_depot_supply();
    test_network_merge_split_and_store_reorder();
    return failures?1:0;}
