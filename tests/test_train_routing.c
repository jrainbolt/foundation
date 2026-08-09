#include <foundation/foundation.h>
#include <foundation/snapshot.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);++failures;}}while(0)

static FactoryEntityId result_id(FactorySimulation*s)
{const FactoryCommandResult*r=factory_simulation_get_command_result(s,0U);
 if(r==NULL||r->result!=FACTORY_RESULT_OK)fprintf(stderr,"unexpected result %d\n",
    r!=NULL?(int)r->result:-1);
 CHECK(r!=NULL&&r->result==FACTORY_RESULT_OK);return r!=NULL?r->entity_id:0U;}
static FactoryEntityId rail(FactorySimulation*s,int x,int y,FactoryRailGeometry g)
{FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL,{.place_rail={x,y,g}}};
 CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);return result_id(s);}
static FactoryEntityId station(FactorySimulation*s,int x,int y,FactoryDirection d)
{FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL_STATION,
 {.place_rail_station={x,y,d}}};CHECK(factory_simulation_submit_command(s,&c)==0);
 CHECK(factory_simulation_tick(s)==0);return result_id(s);}
static FactoryEntityId locomotive(FactorySimulation*s,FactoryEntityId r)
{FactoryCommand c={FACTORY_COMMAND_PLACE_LOCOMOTIVE,
 {.place_locomotive={r,FACTORY_DIRECTION_EAST}}};CHECK(factory_simulation_submit_command(s,&c)==0);
 CHECK(factory_simulation_tick(s)==0);return result_id(s);}

static void route_arrival_clear_and_snapshot(void)
{FactoryWorld*w=factory_world_create(12,6);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,1000U);FactoryEntityId r[6];
 for(int x=1;x<=6;++x)r[x-1]=rail(s,x,2,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId destination=station(s,6,1,FACTORY_DIRECTION_SOUTH);
 FactoryEntityId train=locomotive(s,r[1]);FactoryCommand set={
 FACTORY_COMMAND_SET_TRAIN_DESTINATION,
 {.set_train_destination={train,destination}}};
 CHECK(factory_simulation_submit_command(s,&set)==0);CHECK(factory_simulation_tick(s)==0);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_OK);
 FactoryLocomotiveInspection v;CHECK(factory_simulation_get_locomotive(s,train,&v));
 CHECK(v.destination_station_id==destination&&v.route_status==FACTORY_TRAIN_ROUTE_ACTIVE
    &&v.route_length==5U&&v.route_index==0U&&v.next_planned_rail_id==r[2]);
 FactoryTrainRouteStep step;CHECK(factory_simulation_get_train_route_step(s,train,4U,&step)
    &&step.rail_entity_id==r[5]);
 CHECK(!factory_simulation_get_train_route_step(s,train,5U,&step));
 FactorySnapshotBuffer bytes={0};FactorySimulation*loaded=NULL;
 CHECK(factory_simulation_create_snapshot(s,&bytes)==0);
 CHECK(factory_simulation_load_snapshot(bytes.data,bytes.size,&loaded)==0);
 CHECK(factory_simulation_get_locomotive(loaded,train,&v)&&v.route_length==5U
    &&v.destination_station_id==destination);
 for(int i=0;i<16;++i){CHECK(factory_simulation_tick(s)==0);
    CHECK(factory_simulation_tick(loaded)==0);}
 FactorySnapshotBuffer a={0},b={0};CHECK(factory_simulation_create_snapshot(s,&a)==0);
 CHECK(factory_simulation_create_snapshot(loaded,&b)==0);
 CHECK(a.size==b.size&&memcmp(a.data,b.data,a.size)==0);
 CHECK(factory_simulation_get_locomotive(s,train,&v)
    &&v.route_status==FACTORY_TRAIN_ROUTE_ARRIVED&&v.rail_entity_id==r[5]);
 CHECK(factory_simulation_get_event_count(s)==0U);
 FactoryCommand clear={FACTORY_COMMAND_CLEAR_TRAIN_DESTINATION,
 {.clear_train_destination={train}}};CHECK(factory_simulation_submit_command(s,&clear)==0);
 CHECK(factory_simulation_tick(s)==0);CHECK(factory_simulation_get_locomotive(s,train,&v)
    &&v.route_status==FACTORY_TRAIN_ROUTE_NONE&&v.destination_station_id==0U);
 factory_snapshot_buffer_destroy(&a);factory_snapshot_buffer_destroy(&b);
 factory_snapshot_buffer_destroy(&bytes);factory_simulation_destroy(loaded);
 factory_simulation_destroy(s);factory_world_destroy(w);}

static void unreachable_is_transactional(void)
{FactoryWorld*w=factory_world_create(12,8);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,1000U);
 rail(s,1,2,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId a=rail(s,2,2,FACTORY_RAIL_HORIZONTAL);
 rail(s,3,2,FACTORY_RAIL_HORIZONTAL);rail(s,8,5,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId remote=station(s,8,4,FACTORY_DIRECTION_SOUTH);
 FactoryEntityId train=locomotive(s,a);
 FactoryCommand set={FACTORY_COMMAND_SET_TRAIN_DESTINATION,
 {.set_train_destination={train,remote}}};CHECK(factory_simulation_submit_command(s,&set)==0);
 CHECK(factory_simulation_tick(s)==0);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_INVALID_STATE);
 FactoryLocomotiveInspection v;CHECK(factory_simulation_get_locomotive(s,train,&v)
    &&v.route_status==FACTORY_TRAIN_ROUTE_NONE&&v.destination_station_id==0U);
 CHECK(factory_simulation_get_event_count(s)==0U);
 factory_simulation_destroy(s);factory_world_destroy(w);}

static void physical_switch_route_requires_alignment(void)
{FactoryWorld*w=factory_world_create(10,8);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,1000U);
 rail(s,1,3,FACTORY_RAIL_HORIZONTAL);FactoryEntityId approach=
 rail(s,2,3,FACTORY_RAIL_HORIZONTAL);FactoryCommand switch_command={
 FACTORY_COMMAND_PLACE_RAIL_SWITCH,
 {.place_rail_switch={3,3,FACTORY_RAIL_SWITCH_STEM_WEST}}};
 CHECK(factory_simulation_submit_command(s,&switch_command)==0);
 CHECK(factory_simulation_tick(s)==0);FactoryEntityId sw=result_id(s);
 rail(s,3,2,FACTORY_RAIL_VERTICAL);FactoryEntityId south=
 rail(s,3,4,FACTORY_RAIL_VERTICAL);
 FactoryEntityId destination=station(s,4,4,FACTORY_DIRECTION_WEST);
 FactoryEntityId train=locomotive(s,approach);FactoryCommand set={
 FACTORY_COMMAND_SET_TRAIN_DESTINATION,
 {.set_train_destination={train,destination}}};
 CHECK(factory_simulation_submit_command(s,&set)==0);CHECK(factory_simulation_tick(s)==0);
 FactoryLocomotiveInspection v;CHECK(factory_simulation_get_locomotive(s,train,&v)
    &&v.route_status==FACTORY_TRAIN_ROUTE_ACTIVE&&v.route_length==3U);
 for(int i=0;i<6;++i)CHECK(factory_simulation_tick(s)==0);
 CHECK(factory_simulation_get_locomotive(s,train,&v)&&v.rail_entity_id==sw
    &&v.activity==FACTORY_LOCOMOTIVE_BLOCKED_SWITCH
    &&v.next_planned_rail_id==south);
 FactoryCommand align={FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH,
 {.set_rail_switch_branch={sw,FACTORY_RAIL_SWITCH_BRANCH_B}}};
 CHECK(factory_simulation_submit_command(s,&align)==0);CHECK(factory_simulation_tick(s)==0);
 CHECK(factory_simulation_get_locomotive(s,train,&v)&&v.rail_entity_id==south
    &&v.route_status==FACTORY_TRAIN_ROUTE_ARRIVED);
 factory_simulation_destroy(s);factory_world_destroy(w);}

static void invalidation_and_explicit_replan(void)
{FactoryWorld*w=factory_world_create(12,6);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,1000U);FactoryEntityId r[6];
 for(int x=1;x<=6;++x)r[x-1]=rail(s,x,2,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId destination=station(s,6,1,FACTORY_DIRECTION_SOUTH);
 FactoryEntityId train=locomotive(s,r[1]);FactoryCommand set={
 FACTORY_COMMAND_SET_TRAIN_DESTINATION,
 {.set_train_destination={train,destination}}};
 CHECK(factory_simulation_submit_command(s,&set)==0);CHECK(factory_simulation_tick(s)==0);
 FactoryCommand demolish={FACTORY_COMMAND_DEMOLISH_ENTITY,
 {.demolish_entity={r[3]}}};CHECK(factory_simulation_submit_command(s,&demolish)==0);
 CHECK(factory_simulation_tick(s)==0);
 for(int i=0;i<6;++i)CHECK(factory_simulation_tick(s)==0);
 FactoryLocomotiveInspection v;CHECK(factory_simulation_get_locomotive(s,train,&v)
    &&v.route_status==FACTORY_TRAIN_ROUTE_INVALID
    &&v.destination_station_id==destination);
 FactoryEntityId replacement=rail(s,4,2,FACTORY_RAIL_HORIZONTAL);
 FactoryCommand replan={FACTORY_COMMAND_REPLAN_TRAIN_ROUTE,
 {.replan_train_route={train}}};CHECK(factory_simulation_submit_command(s,&replan)==0);
 CHECK(factory_simulation_tick(s)==0);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_locomotive(s,train,&v)
    &&v.route_status==FACTORY_TRAIN_ROUTE_ACTIVE);
 bool uses_replacement=false;for(size_t i=0U;i<v.route_length;++i){
    FactoryTrainRouteStep step;if(factory_simulation_get_train_route_step(s,train,i,&step)
        &&step.rail_entity_id==replacement)uses_replacement=true;}
 CHECK(uses_replacement);
 factory_simulation_destroy(s);factory_world_destroy(w);}

int main(void){route_arrival_clear_and_snapshot();unreachable_is_transactional();
 physical_switch_route_requires_alignment();
 invalidation_and_explicit_replan();
 return failures?1:0;}
