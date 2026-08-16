#include <foundation/foundation.h>
#include <foundation/snapshot.h>
#include "../src/simulation_internal.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);++failures;}}while(0)
static FactoryEntityId result_id(FactorySimulation*s)
{const FactoryCommandResult*r=factory_simulation_get_command_result(s,0U);
 CHECK(r&&r->result==FACTORY_RESULT_OK);return r?r->entity_id:0U;}
static FactoryEntityId place(FactorySimulation*s,FactoryCommand c)
{CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);return result_id(s);}
static void unlock_chain_signal_fixture(FactorySimulation *s)
{
 s->research.completed_bits|=
    (UINT64_C(1)<<FACTORY_TECHNOLOGY_BASIC_AUTOMATION)
    |(UINT64_C(1)<<FACTORY_TECHNOLOGY_FLUID_HANDLING)
    |(UINT64_C(1)<<FACTORY_TECHNOLOGY_ADVANCED_MANUFACTURING);
 s->research.progress[0].completed_units=2U;
 s->research.progress[1].completed_units=2U;
 s->research.progress[2].completed_units=3U;
}

static void chain_lock_and_snapshot(void)
{FactoryWorld*w=factory_world_create(14,7);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,2000U);FactoryEntityId r[8];
 unlock_chain_signal_fixture(s);
 for(int x=1;x<=8;++x){FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL,
    {.place_rail={x,2,FACTORY_RAIL_HORIZONTAL}}};r[x-1]=place(s,c);}
 FactoryCommand chain={FACTORY_COMMAND_PLACE_RAIL_CHAIN_SIGNAL,
    {.place_rail_chain_signal={3,3,FACTORY_DIRECTION_EAST}}};
 FactoryEntityId chain_id=place(s,chain);
 FactoryCommand ordinary={FACTORY_COMMAND_PLACE_RAIL_SIGNAL,
    {.place_rail_signal={6,3,FACTORY_DIRECTION_EAST}}};place(s,ordinary);
 FactoryCommand station={FACTORY_COMMAND_PLACE_RAIL_STATION,
    {.place_rail_station={8,1,FACTORY_DIRECTION_SOUTH}}};
 FactoryEntityId station_id=place(s,station);
 FactoryRailChainSignalInspection signal={0};
 CHECK(factory_simulation_get_rail_chain_signal(s,chain_id,&signal));
 CHECK(signal.connected&&signal.attached_rail_id==r[2]
    &&signal.upstream_rail_id==r[1]);
 FactoryCommand loco={FACTORY_COMMAND_PLACE_LOCOMOTIVE,
    {.place_locomotive={r[1],FACTORY_DIRECTION_EAST}}};FactoryEntityId train=place(s,loco);
 FactoryCommand destination={FACTORY_COMMAND_SET_TRAIN_DESTINATION,
    {.set_train_destination={train,station_id}}};place(s,destination);
 bool locked=false;for(int tick=0;tick<20&&!locked;++tick){
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    FactoryLocomotiveInspection v;CHECK(factory_simulation_get_locomotive(s,train,&v));
    if(v.chain_status==FACTORY_TRAIN_CHAIN_HELD){locked=true;
        CHECK(v.reserved_block_count>=2U);
        CHECK(v.chain_required_block_count==v.reserved_block_count);
        FactoryRailBlockId first=0U,last=0U;
        CHECK(factory_simulation_get_train_reserved_block_at(s,train,0U,&first));
        CHECK(factory_simulation_get_train_reserved_block_at(s,train,
            v.reserved_block_count-1U,&last));CHECK(first!=last);}}
 CHECK(locked);
 FactorySnapshotBuffer a={0},b={0};FactorySimulation*loaded=NULL;
 CHECK(factory_simulation_create_snapshot(s,&a)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_load_snapshot(a.data,a.size,&loaded)==FACTORY_RESULT_OK);
 CHECK(factory_simulation_create_snapshot(loaded,&b)==FACTORY_RESULT_OK);
 CHECK(a.size==b.size&&memcmp(a.data,b.data,a.size)==0);
 factory_snapshot_buffer_destroy(&a);factory_snapshot_buffer_destroy(&b);
 factory_simulation_destroy(loaded);factory_simulation_destroy(s);factory_world_destroy(w);}

static void occupied_exit_rejects_atomic_request(void)
{FactoryWorld*w=factory_world_create(14,7);FactorySimulation*s=
 factory_simulation_create_with_construction_units(w,2000U);FactoryEntityId r[8];
 unlock_chain_signal_fixture(s);
 for(int x=1;x<=8;++x){FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL,
    {.place_rail={x,2,FACTORY_RAIL_HORIZONTAL}}};r[x-1]=place(s,c);}
 FactoryCommand chain={FACTORY_COMMAND_PLACE_RAIL_CHAIN_SIGNAL,
    {.place_rail_chain_signal={3,3,FACTORY_DIRECTION_EAST}}};place(s,chain);
 FactoryCommand ordinary={FACTORY_COMMAND_PLACE_RAIL_SIGNAL,
    {.place_rail_signal={6,3,FACTORY_DIRECTION_EAST}}};place(s,ordinary);
 FactoryCommand station={FACTORY_COMMAND_PLACE_RAIL_STATION,
    {.place_rail_station={8,1,FACTORY_DIRECTION_SOUTH}}};
 FactoryEntityId station_id=place(s,station);
 FactoryCommand blocker_command={FACTORY_COMMAND_PLACE_LOCOMOTIVE,
    {.place_locomotive={r[6],FACTORY_DIRECTION_WEST}}};
 FactoryEntityId blocker=place(s,blocker_command);
 FactoryCommand requester_command={FACTORY_COMMAND_PLACE_LOCOMOTIVE,
    {.place_locomotive={r[1],FACTORY_DIRECTION_EAST}}};
 FactoryEntityId requester=place(s,requester_command);
 FactoryCommand destination={FACTORY_COMMAND_SET_TRAIN_DESTINATION,
    {.set_train_destination={requester,station_id}}};place(s,destination);
 FactoryLocomotiveInspection v={0};CHECK(factory_simulation_get_locomotive(
    s,requester,&v));
 CHECK(v.chain_status==FACTORY_TRAIN_CHAIN_WAITING);
 CHECK(v.reserved_block_count==0U&&v.chain_required_block_count>=2U);
 CHECK(v.blocking_train_id==blocker&&v.blocking_block_id!=0U);
 CHECK(factory_simulation_get_train_reserved_block_count(s,requester)==0U);
 factory_simulation_destroy(s);factory_world_destroy(w);}

int main(void){chain_lock_and_snapshot();occupied_exit_rejects_atomic_request();
 return failures?1:0;}
