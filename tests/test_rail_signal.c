#include <foundation/foundation.h>
#include <foundation/snapshot.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);++failures;}}while(0)

static FactoryEntityId result_id(FactorySimulation*s)
{const FactoryCommandResult*r=factory_simulation_get_command_result(s,0U);
 CHECK(r&&r->result==FACTORY_RESULT_OK);return r?r->entity_id:0U;}
static FactoryEntityId rail(FactorySimulation*s,int x)
{FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL,
 {.place_rail={x,2,FACTORY_RAIL_HORIZONTAL}}};
 CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);return result_id(s);}
static FactoryEntityId signal(FactorySimulation*s,int x,FactoryDirection d)
{FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL_SIGNAL,
 {.place_rail_signal={x,3,d}}};
 CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);return result_id(s);}

static void splitting_demolition_snapshot(void)
{FactoryWorld*w=factory_world_create(10,6);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,1000U);FactoryEntityId r[6];
 for(int x=1;x<=6;++x)r[x-1]=rail(s,x);
 CHECK(factory_simulation_get_rail_block_count(s)==3U);
 FactoryEntityId id=signal(s,4,FACTORY_DIRECTION_EAST);
 CHECK(id!=0U&&factory_simulation_get_rail_block_count(s)==4U);
 CHECK(factory_simulation_get_rail_block_for_rail(s,r[2])!=
    factory_simulation_get_rail_block_for_rail(s,r[3]));
 FactoryRailSignalInspection v={0};CHECK(factory_simulation_get_rail_signal(s,id,&v));
 CHECK(v.connected&&v.attached_rail_id==r[3]&&v.upstream_rail_id==r[2]);
 CHECK(v.orientation==FACTORY_DIRECTION_EAST
    &&v.aspect==FACTORY_RAIL_SIGNAL_GREEN
    &&v.downstream_block_id==factory_simulation_get_rail_block_for_rail(s,r[3]));
 FactorySnapshotBuffer a={0},b={0};FactorySimulation*loaded=NULL;
 CHECK(factory_simulation_create_snapshot(s,&a)==FACTORY_RESULT_OK);
 FactoryResult load_result=factory_simulation_load_snapshot(a.data,a.size,&loaded);
 CHECK(load_result==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_rail_signal(loaded,id,&v)&&v.connected);
 CHECK(factory_simulation_create_snapshot(loaded,&b)==FACTORY_RESULT_OK);
 CHECK(a.size==b.size&&memcmp(a.data,b.data,a.size)==0);
 FactoryCommand demolish={FACTORY_COMMAND_DEMOLISH_ENTITY,
    {.demolish_entity={id}}};
 CHECK(factory_simulation_submit_command(s,&demolish)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(!factory_simulation_get_rail_signal(s,id,&v));
 CHECK(factory_simulation_get_rail_block_count(s)==3U);
 factory_snapshot_buffer_destroy(&a);factory_snapshot_buffer_destroy(&b);
 factory_simulation_destroy(loaded);factory_simulation_destroy(s);
 factory_world_destroy(w);}

static void invalid_attachment(void)
{FactoryWorld*w=factory_world_create(8,6);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,100U);
 FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL_SIGNAL,
  {.place_rail_signal={3,3,FACTORY_DIRECTION_EAST}}};
 CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_command_result(s,0U)->result
    ==FACTORY_RESULT_INVALID_STATE);
 CHECK(factory_simulation_get_entity_count(s)==0U);
 factory_simulation_destroy(s);factory_world_destroy(w);}

static void reservation_aspect_and_owner_crossing(void)
{FactoryWorld*w=factory_world_create(12,6);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,1000U);FactoryEntityId r[8];
 for(int x=1;x<=8;++x)r[x-1]=rail(s,x);FactoryEntityId sig=signal(s,5,
    FACTORY_DIRECTION_EAST);
 FactoryCommand station_command={FACTORY_COMMAND_PLACE_RAIL_STATION,
    {.place_rail_station={8,1,FACTORY_DIRECTION_SOUTH}}};
 CHECK(factory_simulation_submit_command(s,&station_command)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);FactoryEntityId st=result_id(s);
 FactoryCommand loco={FACTORY_COMMAND_PLACE_LOCOMOTIVE,
    {.place_locomotive={r[1],FACTORY_DIRECTION_EAST}}};
 CHECK(factory_simulation_submit_command(s,&loco)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);FactoryEntityId train=result_id(s);
 FactoryCommand route={FACTORY_COMMAND_SET_TRAIN_DESTINATION,
    {.set_train_destination={train,st}}};
 CHECK(factory_simulation_submit_command(s,&route)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 bool reserved=false,crossed=false;for(int tick=0;tick<20&&!crossed;++tick){
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    FactoryRailSignalInspection v;FactoryLocomotiveInspection l;
    CHECK(factory_simulation_get_rail_signal(s,sig,&v));
    CHECK(factory_simulation_get_locomotive(s,train,&l));
    if(v.aspect==FACTORY_RAIL_SIGNAL_RESERVED){reserved=true;
        CHECK(v.reserved_train_id==train);}
    if(l.rail_entity_id==r[4])crossed=true;
 }
 CHECK(reserved&&crossed);
 factory_simulation_destroy(s);factory_world_destroy(w);}

int main(void){splitting_demolition_snapshot();invalid_attachment();
 reservation_aspect_and_owner_crossing();
 return failures?1:0;}
