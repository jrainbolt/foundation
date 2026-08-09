#include "foundation/foundation.h"
#include "foundation/snapshot.h"
#include "foundation/rail.h"
#include <stdio.h>

static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);++failures;}}while(0)

static FactoryEntityId result_id(FactorySimulation*s)
{const FactoryCommandResult*r=factory_simulation_get_command_result(s,0U);
 CHECK(r!=NULL&&r->result==FACTORY_RESULT_OK);return r!=NULL?r->entity_id:0U;}
static FactoryEntityId rail(FactorySimulation*s,int x)
{FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL,{.place_rail={x,0,FACTORY_RAIL_HORIZONTAL}}};
 c.data.place_rail.x=x+1;
 CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);return result_id(s);}
static void queue_vehicle(FactorySimulation*s,FactoryCommandType type,
    FactoryEntityId r)
{FactoryCommand c={0};c.type=type;if(type==FACTORY_COMMAND_PLACE_LOCOMOTIVE)
 {c.data.place_locomotive.rail_entity_id=r;c.data.place_locomotive.direction=FACTORY_DIRECTION_EAST;}
 else {c.data.place_cargo_wagon.rail_entity_id=r;c.data.place_cargo_wagon.direction=FACTORY_DIRECTION_EAST;}
 CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);}

static void test_shift_cargo_snapshot_and_decouple(void)
{FactoryWorld*world=factory_world_create(12,3);FactorySimulation*s=
 factory_simulation_create_with_construction_units(world,500U);CHECK(s!=NULL);
 FactoryEntityId r[7];for(int i=0;i<7;++i)r[i]=rail(s,i);
 queue_vehicle(s,FACTORY_COMMAND_PLACE_CARGO_WAGON,r[1]);
 queue_vehicle(s,FACTORY_COMMAND_PLACE_CARGO_WAGON,r[2]);
 queue_vehicle(s,FACTORY_COMMAND_PLACE_LOCOMOTIVE,r[3]);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 const FactoryCommandResult*a=factory_simulation_get_command_result(s,0U);
 const FactoryCommandResult*b=factory_simulation_get_command_result(s,1U);
 const FactoryCommandResult*c=factory_simulation_get_command_result(s,2U);
 CHECK(a!=NULL&&a->result==FACTORY_RESULT_OK);
 CHECK(b!=NULL&&b->result==FACTORY_RESULT_OK);
 CHECK(c!=NULL&&c->result==FACTORY_RESULT_OK);
 FactoryEntityId w2=a->entity_id,w1=b->entity_id,l=c->entity_id;
 FactoryCommand couple1={FACTORY_COMMAND_COUPLE_REAR_WAGON,
    {.couple_rear_wagon={l,w1}}};
 FactoryCommand couple2={FACTORY_COMMAND_COUPLE_REAR_WAGON,
    {.couple_rear_wagon={l,w2}}};
 CHECK(factory_simulation_submit_command(s,&couple1)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_submit_command(s,&couple2)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryLocomotiveInspection li;FactoryCargoWagonInspection wi1,wi2;
 CHECK(factory_simulation_get_locomotive(s,l,&li)&&li.vehicle_count==3U
    &&li.train_id==l);
 CHECK(factory_simulation_get_cargo_wagon(s,w1,&wi1)&&wi1.consist_index==1U);
 CHECK(factory_simulation_get_cargo_wagon(s,w2,&wi2)&&wi2.consist_index==2U);
 CHECK(factory_simulation_cargo_wagon_insert(s,w1,FACTORY_ITEM_IRON_PLATE,40U)
    ==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_locomotive(s,l,&li)&&li.rail_entity_id==r[4]);
 CHECK(factory_simulation_get_cargo_wagon(s,w1,&wi1)&&wi1.rail_entity_id==r[3]
    &&wi1.cargo_quantity==40U);
 CHECK(factory_simulation_get_cargo_wagon(s,w2,&wi2)&&wi2.rail_entity_id==r[2]);
 CHECK(factory_simulation_get_rail_vehicle_occupant(s,r[1])==0U);
 FactorySnapshotBuffer snapshot={0};FactorySimulation*loaded=NULL;
 CHECK(factory_simulation_create_snapshot(s,&snapshot)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_load_snapshot(snapshot.data,snapshot.size,&loaded)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_cargo_wagon(loaded,w1,&wi1)&&wi1.cargo_quantity==40U
    &&wi1.train_id==l&&wi1.consist_index==1U);
 for(int i=0;i<4;++i){CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(loaded)==FACTORY_RESULT_OK);}
 CHECK(factory_simulation_get_locomotive(s,l,&li));FactoryEntityId expected=li.rail_entity_id;
 CHECK(factory_simulation_get_locomotive(loaded,l,&li)&&li.rail_entity_id==expected);
 FactoryCommand decouple={FACTORY_COMMAND_DECOUPLE_REAR_WAGON,
    {.decouple_rear_wagon={l}}};CHECK(factory_simulation_submit_command(loaded,&decouple)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(loaded)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_cargo_wagon(loaded,w2,&wi2)&&!wi2.coupled);
 CHECK(factory_simulation_cargo_wagon_remove(loaded,w1,FACTORY_ITEM_IRON_PLATE,40U)
    ==FACTORY_RESULT_OK);
 factory_snapshot_buffer_destroy(&snapshot);factory_simulation_destroy(loaded);
 factory_simulation_destroy(s);factory_world_destroy(world);}

static void test_blocked_atomic_and_rejections(void)
{FactoryWorld*world=factory_world_create(8,3);FactorySimulation*s=
 factory_simulation_create_with_construction_units(world,300U);
 FactoryEntityId r[5];for(int i=0;i<5;++i)r[i]=rail(s,i);
 queue_vehicle(s,FACTORY_COMMAND_PLACE_CARGO_WAGON,r[1]);
 queue_vehicle(s,FACTORY_COMMAND_PLACE_LOCOMOTIVE,r[2]);
 queue_vehicle(s,FACTORY_COMMAND_PLACE_CARGO_WAGON,r[3]);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryEntityId w=factory_simulation_get_command_result(s,0)->entity_id;
 FactoryEntityId l=factory_simulation_get_command_result(s,1)->entity_id;
 FactoryEntityId obstacle=factory_simulation_get_command_result(s,2)->entity_id;
 FactoryCommand bad={FACTORY_COMMAND_COUPLE_REAR_WAGON,{.couple_rear_wagon={l,obstacle}}};
 CHECK(factory_simulation_submit_command(s,&bad)==FACTORY_RESULT_OK);CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_INVALID_STATE);
 FactoryCommand good={FACTORY_COMMAND_COUPLE_REAR_WAGON,{.couple_rear_wagon={l,w}}};
 CHECK(factory_simulation_submit_command(s,&good)==FACTORY_RESULT_OK);CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 for(int i=0;i<2;++i)CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 FactoryLocomotiveInspection li;FactoryCargoWagonInspection wi;
 CHECK(factory_simulation_get_locomotive(s,l,&li)&&li.rail_entity_id==r[2]
    &&li.movement_progress==FACTORY_LOCOMOTIVE_MOVE_TICKS);
 CHECK(factory_simulation_get_cargo_wagon(s,w,&wi)&&wi.rail_entity_id==r[1]);
 FactoryCommand demolish={FACTORY_COMMAND_DEMOLISH_ENTITY,{.demolish_entity={r[1]}}};
 CHECK(factory_simulation_submit_command(s,&demolish)==FACTORY_RESULT_OK);CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_get_command_result(s,0)->result==FACTORY_RESULT_ENTITY_BUSY);
 (void)obstacle;factory_simulation_destroy(s);factory_world_destroy(world);}

int main(void){test_shift_cargo_snapshot_and_decouple();test_blocked_atomic_and_rejections();return failures?1:0;}
