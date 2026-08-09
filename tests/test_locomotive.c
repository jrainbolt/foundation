#include <foundation/foundation.h>
#include <foundation/snapshot.h>
#include "../src/tick_preflight_internal.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);++failures;}}while(false)

static FactoryEntityId result_id(FactorySimulation*s)
{const FactoryCommandResult*r=factory_simulation_get_command_result(s,0U);
 CHECK(r!=NULL&&r->result==FACTORY_RESULT_OK);return r?r->entity_id:0U;}
static FactoryEntityId rail(FactorySimulation*s,int x,int y,FactoryRailGeometry g)
{FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL,{.place_rail={x,y,(uint32_t)g}}};
 CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);return result_id(s);}
static FactoryEntityId loco(FactorySimulation*s,FactoryEntityId r,FactoryDirection d)
{FactoryCommand c={FACTORY_COMMAND_PLACE_LOCOMOTIVE,{.place_locomotive={r,d}}};
 CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);return result_id(s);}

static void movement_and_snapshot(void)
{FactoryWorld*w=factory_world_create(8,8);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,200);
 FactoryEntityId a=rail(s,1,2,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId b=rail(s,2,2,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId c=rail(s,3,2,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId id=loco(s,b,FACTORY_DIRECTION_EAST);FactoryLocomotiveInspection v;
 CHECK(factory_simulation_get_locomotive(s,id,&v));
 CHECK(v.rail_entity_id==b&&v.movement_progress==1U&&v.next_rail_id==c);
 CHECK(factory_simulation_get_rail_vehicle_occupant(s,b)==id);
 CHECK(factory_simulation_get_rail_vehicle_occupant(s,a)==0U);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_locomotive(s,id,&v)&&v.rail_entity_id==b
    &&v.movement_progress==3U);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_locomotive(s,id,&v)&&v.rail_entity_id==c
    &&v.movement_progress==0U&&v.entry_direction==FACTORY_DIRECTION_WEST);
 CHECK(factory_simulation_get_event_count(s)==1U);
 const FactoryEvent*e=factory_simulation_get_event(s,0);
 CHECK(e&&e->type==FACTORY_EVENT_LOCOMOTIVE_MOVED&&e->entity_id==id
    &&e->related_entity_id==b&&e->quantity==c);
 for(int i=0;i<4;++i)CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_locomotive(s,id,&v)&&v.rail_entity_id==c
    &&v.movement_progress==FACTORY_LOCOMOTIVE_MOVE_TICKS
    &&v.activity==FACTORY_LOCOMOTIVE_BLOCKED_TRACK);
 FactorySnapshotBuffer bytes={0};FactorySimulation*loaded=NULL;
 CHECK(factory_simulation_create_snapshot(s,&bytes)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_load_snapshot(bytes.data,bytes.size,&loaded)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_locomotive(loaded,id,&v)&&v.rail_entity_id==c
    &&v.movement_progress==FACTORY_LOCOMOTIVE_MOVE_TICKS);
 CHECK(factory_simulation_get_event_count(loaded)==0U);
 factory_snapshot_buffer_destroy(&bytes);factory_simulation_destroy(loaded);
 factory_simulation_destroy(s);factory_world_destroy(w);}

static void placement_conflict_and_demolition(void)
{FactoryWorld*w=factory_world_create(8,8);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,200);
 FactoryEntityId a=rail(s,1,1,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId b=rail(s,2,1,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId c=rail(s,3,1,FACTORY_RAIL_HORIZONTAL);(void)a;(void)c;
 FactoryEntityId id=loco(s,b,FACTORY_DIRECTION_EAST);
 FactoryCommand duplicate={FACTORY_COMMAND_PLACE_LOCOMOTIVE,
   {.place_locomotive={b,FACTORY_DIRECTION_EAST}}};
 CHECK(factory_simulation_submit_command(s,&duplicate)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_ENTITY_BUSY);
 FactoryCommand demolish_rail={FACTORY_COMMAND_DEMOLISH_ENTITY,
   {.demolish_entity={b}}};
 CHECK(factory_simulation_submit_command(s,&demolish_rail)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_ENTITY_BUSY);
 FactoryCommand demolish_loco={FACTORY_COMMAND_DEMOLISH_ENTITY,
   {.demolish_entity={id}}};
 CHECK(factory_simulation_submit_command(s,&demolish_loco)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_OK);
 FactoryRailInspection inspection;CHECK(factory_simulation_get_rail(s,b,&inspection));
 CHECK(factory_simulation_get_rail_vehicle_occupant(s,b)==0U);
 factory_simulation_destroy(s);factory_world_destroy(w);}

static void deterministic_conflict(void)
{FactoryWorld*w=factory_world_create(9,5);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,300);FactoryEntityId r[5];
 for(int x=1;x<=5;++x)r[x-1]=rail(s,x,2,FACTORY_RAIL_HORIZONTAL);
 FactoryCommand commands[2]={
  {FACTORY_COMMAND_PLACE_LOCOMOTIVE,{.place_locomotive={r[1],FACTORY_DIRECTION_EAST}}},
  {FACTORY_COMMAND_PLACE_LOCOMOTIVE,{.place_locomotive={r[3],FACTORY_DIRECTION_WEST}}}};
 for(int i=0;i<2;++i)CHECK(factory_simulation_submit_command(s,&commands[i])==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryEntityId low=factory_simulation_get_command_result(s,0)->entity_id;
 FactoryEntityId high=factory_simulation_get_command_result(s,1)->entity_id;
 for(int i=0;i<3;++i)CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryLocomotiveInspection a,b;CHECK(factory_simulation_get_locomotive(s,low,&a));
 CHECK(factory_simulation_get_locomotive(s,high,&b));
 CHECK(a.rail_entity_id==r[2]&&b.rail_entity_id==r[3]);
 CHECK(b.activity==FACTORY_LOCOMOTIVE_BLOCKED_OCCUPIED
    &&b.movement_progress==FACTORY_LOCOMOTIVE_MOVE_TICKS);
 factory_simulation_destroy(s);factory_world_destroy(w);}

static void switch_command_before_movement(void)
{FactoryWorld*w=factory_world_create(8,8);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,300);
 rail(s,1,3,FACTORY_RAIL_HORIZONTAL);FactoryEntityId approach=
 rail(s,2,3,FACTORY_RAIL_HORIZONTAL);
 FactoryCommand swc={FACTORY_COMMAND_PLACE_RAIL_SWITCH,
  {.place_rail_switch={3,3,FACTORY_RAIL_SWITCH_STEM_WEST}}};
 CHECK(factory_simulation_submit_command(s,&swc)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);FactoryEntityId sw=result_id(s);
 FactoryEntityId north=rail(s,3,2,FACTORY_RAIL_VERTICAL);
 FactoryEntityId south=rail(s,3,4,FACTORY_RAIL_VERTICAL);(void)north;
 FactoryEntityId id=loco(s,approach,FACTORY_DIRECTION_EAST);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryCommand change={FACTORY_COMMAND_SET_RAIL_SWITCH_BRANCH,
  {.set_rail_switch_branch={sw,FACTORY_RAIL_SWITCH_BRANCH_B}}};
 CHECK(factory_simulation_submit_command(s,&change)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryLocomotiveInspection v;CHECK(factory_simulation_get_locomotive(s,id,&v));
 CHECK(v.rail_entity_id==sw);
 for(int i=0;i<4;++i)CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_locomotive(s,id,&v)&&v.rail_entity_id==south);
 factory_simulation_destroy(s);factory_world_destroy(w);}

static void curve_swap_and_preflight(void)
{FactoryWorld*w=factory_world_create(10,8);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,400);
 rail(s,2,3,FACTORY_RAIL_VERTICAL);FactoryEntityId curve=
 rail(s,2,2,FACTORY_RAIL_CURVE_SE);FactoryEntityId east=
 rail(s,3,2,FACTORY_RAIL_HORIZONTAL);rail(s,4,2,FACTORY_RAIL_HORIZONTAL);
 FactoryEntityId curved=loco(s,curve,FACTORY_DIRECTION_EAST);
 for(int i=0;i<3;++i)CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryLocomotiveInspection v;CHECK(factory_simulation_get_locomotive(s,curved,&v)
    &&v.rail_entity_id==east&&v.entry_direction==FACTORY_DIRECTION_WEST);
 FactorySnapshotBuffer before={0},after={0};
 CHECK(factory_simulation_create_snapshot(s,&before)==FACTORY_RESULT_OK);
 factory_tick_preflight_test_fail_allocations_after(0U);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OUT_OF_MEMORY);
 factory_tick_preflight_test_fail_allocations_after(SIZE_MAX);
 CHECK(factory_simulation_create_snapshot(s,&after)==FACTORY_RESULT_OK);
 CHECK(before.size==after.size&&memcmp(before.data,after.data,before.size)==0);
 factory_snapshot_buffer_destroy(&before);factory_snapshot_buffer_destroy(&after);
 factory_simulation_destroy(s);factory_world_destroy(w);

 w=factory_world_create(10,5);s=factory_simulation_create_with_construction_units(w,400);
 FactoryEntityId rails[6];for(int x=1;x<=6;++x)
    rails[x-1]=rail(s,x,2,FACTORY_RAIL_HORIZONTAL);
 FactoryCommand pair[2]={
  {FACTORY_COMMAND_PLACE_LOCOMOTIVE,{.place_locomotive={rails[2],FACTORY_DIRECTION_EAST}}},
  {FACTORY_COMMAND_PLACE_LOCOMOTIVE,{.place_locomotive={rails[3],FACTORY_DIRECTION_WEST}}}};
 for(int i=0;i<2;++i)CHECK(factory_simulation_submit_command(s,&pair[i])==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryEntityId first=factory_simulation_get_command_result(s,0)->entity_id;
 FactoryEntityId second=factory_simulation_get_command_result(s,1)->entity_id;
 for(int i=0;i<3;++i)CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryLocomotiveInspection a,b;CHECK(factory_simulation_get_locomotive(s,first,&a));
 CHECK(factory_simulation_get_locomotive(s,second,&b));
 CHECK(a.rail_entity_id==rails[2]&&b.rail_entity_id==rails[3]);
 CHECK(a.activity==FACTORY_LOCOMOTIVE_BLOCKED_OCCUPIED
    &&b.activity==FACTORY_LOCOMOTIVE_BLOCKED_OCCUPIED);
 factory_simulation_destroy(s);factory_world_destroy(w);}

int main(void){movement_and_snapshot();placement_conflict_and_demolition();
 deterministic_conflict();switch_command_before_movement();
 curve_swap_and_preflight();
 return failures?1:0;}
