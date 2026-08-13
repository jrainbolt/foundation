#include <foundation/foundation.h>
#include <foundation/snapshot.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);++failures;}}while(0)
static FactoryEntityId result_id(FactorySimulation*s,size_t i)
{const FactoryCommandResult*r=factory_simulation_get_command_result(s,i);
 CHECK(r&&r->result==FACTORY_RESULT_OK);return r?r->entity_id:0U;}
static FactoryEntityId rail(FactorySimulation*s,int x)
{FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL,
 {.place_rail={x,2,FACTORY_RAIL_HORIZONTAL}}};CHECK(factory_simulation_submit_command(s,&c)==0);
 CHECK(factory_simulation_tick(s)==0);return result_id(s,0U);}
static FactoryEntityId station(FactorySimulation*s,int x)
{FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL_STATION,
 {.place_rail_station={x,1,FACTORY_DIRECTION_SOUTH}}};
 CHECK(factory_simulation_submit_command(s,&c)==0);CHECK(factory_simulation_tick(s)==0);
 return result_id(s,0U);}

static void block_derivation(void)
{FactoryWorld*w=factory_world_create(12,6);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,1000U);FactoryEntityId r[5];
 for(int x=1;x<=5;++x)r[x-1]=rail(s,x);
 station(s,5);
 CHECK(factory_simulation_get_rail_block_count(s)==3U);
 CHECK(factory_simulation_get_rail_block_for_rail(s,r[0])==r[0]);
 CHECK(factory_simulation_get_rail_block_for_rail(s,r[1])==r[1]);
 CHECK(factory_simulation_get_rail_block_for_rail(s,r[2])==r[1]);
 CHECK(factory_simulation_get_rail_block_for_rail(s,r[3])==r[1]);
 CHECK(factory_simulation_get_rail_block_for_rail(s,r[4])==r[4]);
 FactoryRailBlockInspection b;CHECK(factory_simulation_get_rail_block_at(s,1U,&b)
    &&b.block_id==r[1]&&b.member_count==3U&&!b.endpoint_boundary);
 FactoryEntityId member=0U;CHECK(factory_simulation_get_rail_block_member(s,r[1],2U,
    &member)&&member==r[3]);
 factory_simulation_destroy(s);factory_world_destroy(w);}

static void two_train_reservation_and_snapshot(void)
{FactoryWorld*w=factory_world_create(14,6);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,2000U);FactoryEntityId r[7];
 for(int x=1;x<=7;++x)r[x-1]=rail(s,x);
 FactoryEntityId st=station(s,4);
 FactoryCommand vehicles[2]={
  {FACTORY_COMMAND_PLACE_LOCOMOTIVE,{.place_locomotive={r[1],FACTORY_DIRECTION_EAST}}},
  {FACTORY_COMMAND_PLACE_LOCOMOTIVE,{.place_locomotive={r[5],FACTORY_DIRECTION_WEST}}}};
 CHECK(factory_simulation_submit_command(s,&vehicles[0])==0);
 CHECK(factory_simulation_submit_command(s,&vehicles[1])==0);
 CHECK(factory_simulation_tick(s)==0);FactoryEntityId low=result_id(s,0U),high=result_id(s,1U);
 FactoryCommand routes[2]={
  {FACTORY_COMMAND_SET_TRAIN_DESTINATION,{.set_train_destination={low,st}}},
  {FACTORY_COMMAND_SET_TRAIN_DESTINATION,{.set_train_destination={high,st}}}};
 CHECK(factory_simulation_submit_command(s,&routes[0])==0);
 CHECK(factory_simulation_submit_command(s,&routes[1])==0);CHECK(factory_simulation_tick(s)==0);
 FactoryLocomotiveInspection a,b;bool contested=false;
 for(int tick=0;tick<12&&!contested;++tick){CHECK(factory_simulation_tick(s)==0);
   CHECK(factory_simulation_get_locomotive(s,low,&a));
   CHECK(factory_simulation_get_locomotive(s,high,&b));
   contested=a.reserved_block_id==r[3]
      &&b.reservation_status==FACTORY_TRAIN_RESERVATION_WAITING;}
 CHECK(contested&&a.reservation_status==FACTORY_TRAIN_RESERVATION_HELD
    &&b.blocking_train_id==low);
 FactoryRailBlockInspection block={0};for(size_t i=0U;
    i<factory_simulation_get_rail_block_count(s);++i){FactoryRailBlockInspection v;
    if(factory_simulation_get_rail_block_at(s,i,&v)&&v.block_id==r[3])block=v;}
 CHECK(block.reserved_train_id==low);
 FactorySnapshotBuffer bytes={0};FactorySimulation*loaded=NULL;
 CHECK(factory_simulation_create_snapshot(s,&bytes)==0);
 CHECK(factory_simulation_load_snapshot(bytes.data,bytes.size,&loaded)==0);
 CHECK(factory_simulation_get_locomotive(loaded,low,&a)&&a.reserved_block_id==r[3]);
 CHECK(factory_simulation_get_locomotive(loaded,high,&b)
    &&b.reservation_status!=FACTORY_TRAIN_RESERVATION_HELD);
 for(int i=0;i<4;++i){CHECK(factory_simulation_tick(s)==0);
    CHECK(factory_simulation_tick(loaded)==0);}
 FactorySnapshotBuffer x={0},y={0};CHECK(factory_simulation_create_snapshot(s,&x)==0);
 CHECK(factory_simulation_create_snapshot(loaded,&y)==0);
 CHECK(x.size==y.size&&memcmp(x.data,y.data,x.size)==0);
 factory_snapshot_buffer_destroy(&x);factory_snapshot_buffer_destroy(&y);
 factory_snapshot_buffer_destroy(&bytes);factory_simulation_destroy(loaded);
 factory_simulation_destroy(s);factory_world_destroy(w);}

int main(void){block_derivation();two_train_reservation_and_snapshot();
 return failures?1:0;}
