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

static FactoryEntityId place_switch(FactorySimulation*s,int32_t x,int32_t y,
    FactoryRailSwitchGeometry geometry)
{
    FactoryCommand command={FACTORY_COMMAND_PLACE_RAIL_SWITCH,
        {.place_rail_switch={x,y,(uint32_t)geometry}}};
    CHECK(factory_simulation_submit_command(s,&command)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    const FactoryCommandResult*result=factory_simulation_get_command_result(s,0U);
    CHECK(result!=NULL&&result->result==FACTORY_RESULT_OK);
    return result!=NULL?result->entity_id:0U;
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
    FactoryCommand rail_switch={FACTORY_COMMAND_PLACE_RAIL_SWITCH,
        {.place_rail_switch={1,1,FACTORY_RAIL_SWITCH_STEM_NORTH}}};
    CHECK(factory_simulation_submit_command(s,&rail_switch)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_TERRAIN_BLOCKED);
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
    FactoryEntityId rail_switch=place_switch(s,3,1,
        FACTORY_RAIL_SWITCH_STEM_EAST);
    CHECK(rail_switch!=0U);
    CHECK(s->construction_depots.items[0].material_quantity
        ==before-FACTORY_CONSTRUCTION_COST_RAIL
            -FACTORY_CONSTRUCTION_COST_RAIL_STATION
            -FACTORY_CONSTRUCTION_COST_RAIL_SWITCH);
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
            -FACTORY_CONSTRUCTION_COST_RAIL_STATION
            -FACTORY_CONSTRUCTION_COST_RAIL_SWITCH);
    s->construction_depots.items[0].material_quantity=
        FACTORY_CONSTRUCTION_COST_RAIL_SWITCH-1U;
    FactoryCommand insufficient={FACTORY_COMMAND_PLACE_RAIL_SWITCH,
        {.place_rail_switch={4,1,FACTORY_RAIL_SWITCH_STEM_EAST}}};
    CHECK(factory_simulation_submit_command(s,&insufficient)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_CONSTRUCTION_SUPPLY_INSUFFICIENT);
    CHECK(s->construction_depots.items[0].material_quantity
        ==FACTORY_CONSTRUCTION_COST_RAIL_SWITCH-1U);
    FactoryCommand demolish_switch={FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={rail_switch}}};
    CHECK(factory_simulation_submit_command(s,&demolish_switch)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result==FACTORY_RESULT_OK);
    CHECK(s->construction_depots.items[0].material_quantity
        ==FACTORY_CONSTRUCTION_COST_RAIL_SWITCH*2U-1U);
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

static void test_switch_geometry_topology_traversal_and_fifo(void)
{
    static const uint32_t masks[FACTORY_RAIL_SWITCH_GEOMETRY_COUNT]={
        FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_EAST|FACTORY_RAIL_PORT_WEST,
        FACTORY_RAIL_PORT_SOUTH|FACTORY_RAIL_PORT_EAST|FACTORY_RAIL_PORT_WEST,
        FACTORY_RAIL_PORT_EAST|FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_SOUTH,
        FACTORY_RAIL_PORT_WEST|FACTORY_RAIL_PORT_NORTH|FACTORY_RAIL_PORT_SOUTH};
    for(size_t i=0U;i<FACTORY_RAIL_SWITCH_GEOMETRY_COUNT;++i){
        FactoryDirection stem,branch_a,branch_b;
        CHECK(factory_rail_switch_geometry_is_valid((FactoryRailSwitchGeometry)i));
        CHECK(factory_rail_switch_geometry_port_mask((FactoryRailSwitchGeometry)i)
            ==masks[i]);
        CHECK(factory_rail_switch_geometry_directions(
            (FactoryRailSwitchGeometry)i,&stem,&branch_a,&branch_b));
        CHECK(stem!=branch_a&&stem!=branch_b&&branch_a!=branch_b);
    }
    CHECK(!factory_rail_switch_geometry_is_valid(
        (FactoryRailSwitchGeometry)FACTORY_RAIL_SWITCH_GEOMETRY_COUNT));
    FactoryWorld*w=factory_world_create(10U,10U);
    FactorySimulation*s=factory_simulation_create_with_construction_units(w,200U);
    FactoryEntityId west_outer=place(s,1,3,FACTORY_RAIL_HORIZONTAL);
    FactoryEntityId west=place(s,2,3,FACTORY_RAIL_HORIZONTAL);
    FactoryEntityId north=place(s,3,2,FACTORY_RAIL_VERTICAL);
    FactoryEntityId south=place(s,3,4,FACTORY_RAIL_VERTICAL);
    FactoryEntityId rail_switch=place_switch(s,3,3,
        FACTORY_RAIL_SWITCH_STEM_WEST);
    FactoryCommand station_command={FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={1,2,FACTORY_DIRECTION_SOUTH}}};
    CHECK(factory_simulation_submit_command(s,&station_command)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    FactoryEntityId station=factory_simulation_get_command_result(s,0U)->entity_id;
    FactoryRailSwitchInspection inspection;
    CHECK(factory_simulation_get_rail_switch(s,rail_switch,&inspection));
    CHECK(inspection.connection_count==3U
        &&inspection.neighbors[FACTORY_DIRECTION_WEST]==west
        &&inspection.neighbors[FACTORY_DIRECTION_NORTH]==north
        &&inspection.neighbors[FACTORY_DIRECTION_SOUTH]==south);
    CHECK(factory_simulation_get_rail_network_count(s)==1U);
    CHECK(inspection.network_id==west_outer);
    FactoryRailTraversal traversal;
    CHECK(factory_simulation_get_rail_traversal(s,rail_switch,
        FACTORY_DIRECTION_WEST,&traversal)&&traversal.allowed
        &&traversal.exit_direction==FACTORY_DIRECTION_NORTH
        &&traversal.exit_entity_id==north);
    CHECK(factory_simulation_get_rail_traversal(s,rail_switch,
        FACTORY_DIRECTION_NORTH,&traversal)&&traversal.allowed
        &&traversal.exit_entity_id==west);
    CHECK(factory_simulation_get_rail_traversal(s,rail_switch,
        FACTORY_DIRECTION_SOUTH,&traversal)&&!traversal.allowed);
    CHECK(factory_simulation_get_rail_traversal(s,west,
        FACTORY_DIRECTION_EAST,&traversal)&&traversal.allowed
        &&traversal.exit_entity_id==west_outer);
    CHECK(!factory_simulation_get_rail_traversal(s,9999U,
        FACTORY_DIRECTION_NORTH,&traversal));
    CHECK(!factory_simulation_get_rail_traversal(s,rail_switch,
        (FactoryDirection)99,&traversal));
    FactoryCommand commands[]={
        {FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH,
            {.set_rail_switch_branch={rail_switch,FACTORY_RAIL_SWITCH_BRANCH_A}}},
        {FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH,
            {.set_rail_switch_branch={rail_switch,FACTORY_RAIL_SWITCH_BRANCH_B}}}};
    for(size_t i=0U;i<2U;++i)CHECK(factory_simulation_submit_command(s,
        &commands[i])==FACTORY_RESULT_OK);
    uint64_t event_tick=factory_simulation_get_tick(s);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(s)==1U);
    const FactoryEvent*event=factory_simulation_get_event(s,0U);
    CHECK(event!=NULL&&event->type==FACTORY_EVENT_RAIL_SWITCH_CHANGED
        &&event->entity_id==rail_switch&&event->quantity==FACTORY_RAIL_SWITCH_BRANCH_A
        &&event->related_quantity==FACTORY_RAIL_SWITCH_BRANCH_B
        &&event->tick==event_tick);
    CHECK(factory_simulation_get_rail_switch(s,rail_switch,&inspection)
        &&inspection.selected_branch==FACTORY_RAIL_SWITCH_BRANCH_B);
    CHECK(factory_simulation_get_rail_traversal(s,rail_switch,
        FACTORY_DIRECTION_WEST,&traversal)&&traversal.allowed
        &&traversal.exit_entity_id==south);
    CHECK(factory_simulation_get_rail_network_count(s)==1U);
    FactoryCommand reverse[]={
        {FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH,
            {.set_rail_switch_branch={rail_switch,FACTORY_RAIL_SWITCH_BRANCH_A}}},
        {FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH,
            {.set_rail_switch_branch={rail_switch,FACTORY_RAIL_SWITCH_BRANCH_B}}}};
    for(size_t i=0U;i<2U;++i)CHECK(factory_simulation_submit_command(s,
        &reverse[i])==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(s)==2U);
    CHECK(factory_simulation_get_event(s,0U)->quantity
        ==FACTORY_RAIL_SWITCH_BRANCH_B
        &&factory_simulation_get_event(s,0U)->related_quantity
            ==FACTORY_RAIL_SWITCH_BRANCH_A);
    CHECK(factory_simulation_get_event(s,1U)->quantity
        ==FACTORY_RAIL_SWITCH_BRANCH_A
        &&factory_simulation_get_event(s,1U)->related_quantity
            ==FACTORY_RAIL_SWITCH_BRANCH_B);
    FactoryCommand wrong={FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH,
        {.set_rail_switch_branch={west,FACTORY_RAIL_SWITCH_BRANCH_A}}};
    CHECK(factory_simulation_submit_command(s,&wrong)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_UNSUPPORTED_ENTITY);
    wrong.data.set_rail_switch_branch.entity_id=9999U;
    CHECK(factory_simulation_submit_command(s,&wrong)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_ENTITY_NOT_FOUND);
    FactoryCommand invalid={FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH,
        {.set_rail_switch_branch={rail_switch,FACTORY_RAIL_SWITCH_BRANCH_COUNT}}};
    CHECK(factory_simulation_submit_command(s,&invalid)
        ==FACTORY_RESULT_INVALID_ARGUMENT);
    FactorySnapshotBuffer snapshot={0};FactorySimulation*loaded=NULL;
    CHECK(factory_simulation_create_snapshot(s,&snapshot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snapshot.data,snapshot.size,&loaded)
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(loaded)==0U);
    CHECK(factory_simulation_get_rail_switch(loaded,rail_switch,&inspection)
        &&inspection.selected_branch==FACTORY_RAIL_SWITCH_BRANCH_B
        &&inspection.connection_count==3U);
    FactoryRailStationInspection station_inspection;
    CHECK(factory_simulation_get_rail_station(loaded,station,
        &station_inspection)&&station_inspection.connected
        &&station_inspection.network_id==west_outer);
    CHECK(factory_simulation_get_rail_traversal(loaded,rail_switch,
        FACTORY_DIRECTION_WEST,&traversal)&&traversal.allowed
        &&traversal.exit_entity_id==south);
    FactoryCommand demolish={FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={rail_switch}}};
    CHECK(factory_simulation_submit_command(loaded,&demolish)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(loaded)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_rail_network_count(loaded)==3U);
    CHECK(place_switch(loaded,3,3,FACTORY_RAIL_SWITCH_STEM_WEST)!=0U);
    CHECK(factory_simulation_get_rail_network_count(loaded)==1U);
    factory_snapshot_buffer_destroy(&snapshot);factory_simulation_destroy(loaded);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void test_traversal_edge_cases(void)
{
    FactoryWorld*w=factory_world_create(10U,10U);
    FactorySimulation*s=factory_simulation_create_with_construction_units(w,100U);
    FactoryEntityId stem=place(s,1,2,FACTORY_RAIL_HORIZONTAL);
    FactoryEntityId rail_switch=place_switch(s,2,2,
        FACTORY_RAIL_SWITCH_STEM_WEST);
    (void)place(s,2,1,FACTORY_RAIL_HORIZONTAL);
    FactoryRailTraversal traversal;
    CHECK(factory_simulation_get_rail_traversal(s,rail_switch,
        FACTORY_DIRECTION_WEST,&traversal)&&!traversal.allowed);
    CHECK(factory_simulation_get_rail_traversal(s,stem,
        FACTORY_DIRECTION_EAST,&traversal)&&!traversal.allowed);
    FactoryEntityId curve=place(s,6,6,FACTORY_RAIL_CURVE_NE);
    FactoryEntityId north=place(s,6,5,FACTORY_RAIL_VERTICAL);
    FactoryEntityId east=place(s,7,6,FACTORY_RAIL_HORIZONTAL);
    CHECK(factory_simulation_get_rail_traversal(s,curve,
        FACTORY_DIRECTION_NORTH,&traversal)&&traversal.allowed
        &&traversal.exit_direction==FACTORY_DIRECTION_EAST
        &&traversal.exit_entity_id==east);
    CHECK(factory_simulation_get_rail_traversal(s,curve,
        FACTORY_DIRECTION_EAST,&traversal)&&traversal.allowed
        &&traversal.exit_entity_id==north);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

int main(void){test_masks_and_connections();test_loop_station_snapshot_and_split();
    test_terrain_and_station_rejection();test_construction_depot_supply();
    test_network_merge_split_and_store_reorder();
    test_switch_geometry_topology_traversal_and_fifo();
    test_traversal_edge_cases();
    return failures?1:0;}
