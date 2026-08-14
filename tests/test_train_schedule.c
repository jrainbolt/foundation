#include <foundation/foundation.h>
#include <foundation/snapshot.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n", \
    __FILE__,__LINE__,#x); ++failures; } } while (0)

static FactoryEntityId result_id(FactorySimulation*s)
{
    const FactoryCommandResult*r=factory_simulation_get_command_result(s,0U);
    CHECK(r!=NULL&&r->result==FACTORY_RESULT_OK);
    return r!=NULL?r->entity_id:0U;
}

static FactoryEntityId rail(FactorySimulation*s,int32_t x)
{
    FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL,
        {.place_rail={x,2,FACTORY_RAIL_HORIZONTAL}}};
    CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    return result_id(s);
}

static FactoryEntityId station(FactorySimulation*s,int32_t x)
{
    FactoryCommand c={FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={x,1,FACTORY_DIRECTION_SOUTH}}};
    CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    return result_id(s);
}

static FactoryEntityId locomotive(FactorySimulation*s,FactoryEntityId rail_id)
{
    FactoryCommand c={FACTORY_COMMAND_PLACE_LOCOMOTIVE,
        {.place_locomotive={rail_id,FACTORY_DIRECTION_EAST}}};
    CHECK(factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    return result_id(s);
}

static void timed_wait_snapshot_and_manual_ownership(void)
{
    FactoryWorld*w=factory_world_create(10,5);
    FactorySimulation*s=factory_simulation_create_with_construction_units(w,500U);
    FactoryEntityId rails[6];
    for(int32_t x=1;x<=6;++x)rails[x-1]=rail(s,x);
    FactoryEntityId stop=station(s,6);
    FactoryEntityId train=locomotive(s,rails[1]);
    FactoryCommand add={FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP,
        {.train_schedule_add_stop={train,stop,FACTORY_TRAIN_WAIT_TIME,2U}}};
    FactoryCommand enable={FACTORY_COMMAND_TRAIN_SCHEDULE_SET_ENABLED,
        {.train_schedule_set_enabled={train,true}}};
    CHECK(factory_simulation_submit_command(s,&add)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_submit_command(s,&enable)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,1U)->result==FACTORY_RESULT_OK);
    FactoryTrainScheduleStop entry;
    CHECK(factory_simulation_get_train_schedule_stop(s,train,0U,&entry));
    CHECK(entry.station_entity_id==stop
        &&entry.wait_condition==FACTORY_TRAIN_WAIT_TIME&&entry.wait_value==2U);
    FactoryLocomotiveInspection state;
    for(uint32_t tick=0U;tick<30U;++tick){
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_locomotive(s,train,&state));
        if(state.route_status==FACTORY_TRAIN_ROUTE_ARRIVED)break;
    }
    CHECK(state.route_status==FACTORY_TRAIN_ROUTE_ARRIVED);
    CHECK(state.schedule_enabled&&state.schedule_count==1U
        &&state.wait_progress==0U
        &&state.schedule_status==FACTORY_TRAIN_SCHEDULE_WAITING);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_locomotive(s,train,&state)
        &&state.wait_progress==1U);

    FactorySnapshotBuffer snapshot={0};FactorySimulation*loaded=NULL;
    CHECK(factory_simulation_create_snapshot(s,&snapshot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snapshot.data,snapshot.size,&loaded)
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_event_count(loaded)==0U);
    CHECK(factory_simulation_get_locomotive(loaded,train,&state)
        &&state.wait_progress==1U&&state.schedule_enabled);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(loaded)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_locomotive(s,train,&state)
        &&state.wait_progress==0U&&state.current_stop_index==0U);
    FactorySnapshotBuffer a={0},b={0};
    CHECK(factory_simulation_create_snapshot(s,&a)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(loaded,&b)==FACTORY_RESULT_OK);
    CHECK(a.size==b.size&&memcmp(a.data,b.data,a.size)==0);
    bool wait_event=false,advance_event=false;
    for(size_t i=0U;i<factory_simulation_get_event_count(s);++i){
        const FactoryEvent*event=factory_simulation_get_event(s,i);
        if(event->type==FACTORY_EVENT_TRAIN_WAIT_COMPLETED)wait_event=true;
        if(event->type==FACTORY_EVENT_TRAIN_SCHEDULE_ADVANCED)advance_event=true;
    }
    CHECK(wait_event&&advance_event);

    FactoryCommand manual={FACTORY_COMMAND_SET_TRAIN_DESTINATION,
        {.set_train_destination={train,stop}}};
    CHECK(factory_simulation_submit_command(s,&manual)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_INVALID_STATE);
    FactoryCommand disable={FACTORY_COMMAND_TRAIN_SCHEDULE_SET_ENABLED,
        {.train_schedule_set_enabled={train,false}}};
    CHECK(factory_simulation_submit_command(s,&disable)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_locomotive(s,train,&state)
        &&!state.schedule_enabled
        &&state.schedule_status==FACTORY_TRAIN_SCHEDULE_DISABLED);

    factory_snapshot_buffer_destroy(&a);factory_snapshot_buffer_destroy(&b);
    factory_snapshot_buffer_destroy(&snapshot);factory_simulation_destroy(loaded);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

static void cargo_conditions_and_zero_wagon(void)
{
    FactoryWorld*w=factory_world_create(8,5);
    FactorySimulation*s=factory_simulation_create_with_construction_units(w,500U);
    (void)rail(s,1);FactoryEntityId r1=rail(s,2);
    FactoryEntityId r2=rail(s,3);(void)rail(s,4);
    FactoryEntityId stop=station(s,3);
    FactoryCommand wagon_command={FACTORY_COMMAND_PLACE_CARGO_WAGON,
        {.place_cargo_wagon={r1,FACTORY_DIRECTION_EAST}}};
    CHECK(factory_simulation_submit_command(s,&wagon_command)==0);
    CHECK(factory_simulation_tick(s)==0);
    FactoryEntityId wagon=result_id(s);
    FactoryEntityId train=locomotive(s,r2);
    FactoryCommand couple={FACTORY_COMMAND_COUPLE_REAR_WAGON,
        {.couple_rear_wagon={train,wagon}}};
    CHECK(factory_simulation_submit_command(s,&couple)==0);
    CHECK(factory_simulation_tick(s)==0);
    FactoryCommand full={FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP,
        {.train_schedule_add_stop={train,stop,FACTORY_TRAIN_WAIT_CARGO_FULL,0U}}};
    FactoryCommand empty={FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP,
        {.train_schedule_add_stop={train,stop,FACTORY_TRAIN_WAIT_CARGO_EMPTY,0U}}};
    FactoryCommand enable={FACTORY_COMMAND_TRAIN_SCHEDULE_SET_ENABLED,
        {.train_schedule_set_enabled={train,true}}};
    CHECK(factory_simulation_submit_command(s,&full)==0);
    CHECK(factory_simulation_submit_command(s,&empty)==0);
    CHECK(factory_simulation_submit_command(s,&enable)==0);
    CHECK(factory_simulation_tick(s)==0);
    CHECK(factory_simulation_tick(s)==0);
    FactoryLocomotiveInspection state;
    CHECK(factory_simulation_get_locomotive(s,train,&state)
        &&state.current_stop_index==0U);

    CHECK(factory_simulation_cargo_wagon_insert(s,wagon,
        FACTORY_ITEM_IRON_PLATE,FACTORY_CARGO_WAGON_CAPACITY)==0);
    CHECK(factory_simulation_tick(s)==0);
    CHECK(factory_simulation_get_locomotive(s,train,&state)
        &&state.current_stop_index==1U
        &&state.wait_condition==FACTORY_TRAIN_WAIT_CARGO_EMPTY);
    CHECK(factory_simulation_cargo_wagon_remove(s,wagon,
        FACTORY_ITEM_IRON_PLATE,FACTORY_CARGO_WAGON_CAPACITY)==0);
    CHECK(factory_simulation_tick(s)==0);
    CHECK(factory_simulation_get_locomotive(s,train,&state)
        &&state.current_stop_index==0U);
    factory_simulation_destroy(s);factory_world_destroy(w);
}

int main(void)
{
    timed_wait_snapshot_and_manual_ownership();
    cargo_conditions_and_zero_wagon();
    return failures!=0?1:0;
}
