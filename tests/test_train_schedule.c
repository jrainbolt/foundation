#include <foundation/foundation.h>
#include <foundation/snapshot.h>
#include "logistics_endpoint_internal.h"
#include "power_fixture.h"
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

static FactoryEntityId place(FactorySimulation *s,FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(s,&command)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    return result_id(s);
}

static uint32_t iron_plate_total(FactorySimulation *s,
    FactoryEntityId source,FactoryEntityId source_inserter,
    FactoryEntityId load_station,FactoryEntityId wagon,
    FactoryEntityId unload_station,FactoryEntityId destination_inserter,
    FactoryEntityId destination)
{
    FactoryStorage storage;
    FactoryInserter inserter;
    FactoryRailStationInspection station_state;
    FactoryCargoWagonInspection wagon_state;
    uint32_t total=0U;
    CHECK(factory_simulation_get_storage(s,source,&storage));
    total+=storage.iron_plate_amount;
    total+=storage.output_occupied
        &&storage.output_item==FACTORY_ITEM_IRON_PLATE?1U:0U;
    CHECK(factory_simulation_get_inserter(s,source_inserter,&inserter));
    total+=inserter.held_item==FACTORY_ITEM_IRON_PLATE
        ?inserter.held_amount:0U;
    CHECK(factory_simulation_get_rail_station(s,load_station,&station_state));
    total+=station_state.freight_quantity;
    CHECK(factory_simulation_get_cargo_wagon(s,wagon,&wagon_state));
    total+=wagon_state.cargo_item==FACTORY_ITEM_IRON_PLATE
        ?wagon_state.cargo_quantity:0U;
    CHECK(factory_simulation_get_rail_station(s,unload_station,&station_state));
    total+=station_state.freight_quantity;
    CHECK(factory_simulation_get_inserter(s,destination_inserter,&inserter));
    total+=inserter.held_item==FACTORY_ITEM_IRON_PLATE
        ?inserter.held_amount:0U;
    CHECK(factory_simulation_get_storage(s,destination,&storage));
    total+=storage.iron_plate_amount;
    total+=storage.output_occupied
        &&storage.output_item==FACTORY_ITEM_IRON_PLATE?1U:0U;
    return total;
}

static void submit_schedule_command(FactorySimulation *s,FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(s,&command)==FACTORY_RESULT_OK);
}

static void autonomous_scheduled_freight_loop(void)
{
    FactoryWorld *world=factory_world_create(12,14);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        world,UINT32_MAX);
    FactoryEntityId top[7]={0};
    FactoryEntityId bottom[7]={0};
    FactoryEntityId left[3]={0};
    FactoryEntityId right[3]={0};
    const FactoryRailGeometry top_geometry[7]={
        FACTORY_RAIL_CURVE_SE,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_CURVE_SW};
    const FactoryRailGeometry bottom_geometry[7]={
        FACTORY_RAIL_CURVE_NE,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_HORIZONTAL,FACTORY_RAIL_HORIZONTAL,
        FACTORY_RAIL_CURVE_NW};
    for(int32_t index=0;index<7;++index){
        top[index]=place(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={index+2,3,top_geometry[index]}}});
        bottom[index]=place(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={index+2,7,bottom_geometry[index]}}});
    }
    for(int32_t index=0;index<3;++index){
        left[index]=place(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={2,index+4,FACTORY_RAIL_VERTICAL}}});
        right[index]=place(s,(FactoryCommand){FACTORY_COMMAND_PLACE_RAIL,
            {.place_rail={8,index+4,FACTORY_RAIL_VERTICAL}}});
    }
    (void)left;(void)right;
    FactoryEntityId load_station=place(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={4,2,FACTORY_DIRECTION_SOUTH}}});
    FactoryEntityId unload_station=place(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_RAIL_STATION,
        {.place_rail_station={6,8,FACTORY_DIRECTION_NORTH}}});
    FactoryEntityId source=place(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STORAGE,
        {.place_storage={4,0}}});
    FactoryEntityId source_inserter=place(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter={4,1,FACTORY_DIRECTION_SOUTH}}});
    FactoryEntityId destination_inserter=place(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_INSERTER,
        {.place_inserter={6,9,FACTORY_DIRECTION_SOUTH}}});
    FactoryEntityId destination=place(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_STORAGE,{.place_storage={6,10}}});
    CHECK(factory_test_submit_power_pair(s,2,1,1,1));
    CHECK(factory_test_submit_power_pair(s,8,9,9,9));
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    for(size_t i=0U;i<4U;++i){
        CHECK(factory_simulation_get_command_result(s,i)->result
            ==FACTORY_RESULT_OK);
    }
    FactoryEntityId signal=place(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_RAIL_SIGNAL,
        {.place_rail_signal={5,4,FACTORY_DIRECTION_EAST}}});
    FactoryRailSignalInspection signal_state;
    CHECK(factory_simulation_get_rail_signal(s,signal,&signal_state)
        &&signal_state.connected);

    FactoryEntityId wagon=place(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_CARGO_WAGON,
        {.place_cargo_wagon={top[1],FACTORY_DIRECTION_EAST}}});
    FactoryEntityId train=place(s,(FactoryCommand){
        FACTORY_COMMAND_PLACE_LOCOMOTIVE,
        {.place_locomotive={top[2],FACTORY_DIRECTION_EAST}}});
    submit_schedule_command(s,(FactoryCommand){FACTORY_COMMAND_COUPLE_REAR_WAGON,
        {.couple_rear_wagon={train,wagon}}});
    submit_schedule_command(s,(FactoryCommand){
        FACTORY_COMMAND_SET_STORAGE_OUTPUT,
        {.set_storage_output={source,FACTORY_ITEM_IRON_PLATE}}});
    submit_schedule_command(s,(FactoryCommand){
        FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_ITEM,
        {.set_rail_station_freight_item={load_station,FACTORY_ITEM_IRON_PLATE}}});
    submit_schedule_command(s,(FactoryCommand){
        FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_MODE,
        {.set_rail_station_freight_mode={load_station,
            FACTORY_RAIL_STATION_FREIGHT_LOAD}}});
    submit_schedule_command(s,(FactoryCommand){
        FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_ITEM,
        {.set_rail_station_freight_item={unload_station,
            FACTORY_ITEM_IRON_PLATE}}});
    submit_schedule_command(s,(FactoryCommand){
        FACTORY_COMMAND_SET_RAIL_STATION_FREIGHT_MODE,
        {.set_rail_station_freight_mode={unload_station,
            FACTORY_RAIL_STATION_FREIGHT_UNLOAD}}});
    submit_schedule_command(s,(FactoryCommand){
        FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP,
        {.train_schedule_add_stop={train,load_station,
            FACTORY_TRAIN_WAIT_CARGO_FULL,0U}}});
    submit_schedule_command(s,(FactoryCommand){
        FACTORY_COMMAND_TRAIN_SCHEDULE_ADD_STOP,
        {.train_schedule_add_stop={train,unload_station,
            FACTORY_TRAIN_WAIT_CARGO_EMPTY,0U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    for(size_t i=0U;i<8U;++i){
        CHECK(factory_simulation_get_command_result(s,i)->result
            ==FACTORY_RESULT_OK);
    }
    FactoryLogisticsEndpoint source_input={source,
        FACTORY_LOGISTICS_SLOT_STORAGE_INPUT};
    for(uint32_t item=0U;item<FACTORY_CARGO_WAGON_CAPACITY;++item){
        CHECK(factory_logistics_endpoint_insert(s,source_input,
            FACTORY_ITEM_IRON_PLATE)==FACTORY_LOGISTICS_RESULT_OK);
    }
    CHECK(iron_plate_total(s,source,source_inserter,load_station,wagon,
        unload_station,destination_inserter,destination)==100U);
    submit_schedule_command(s,(FactoryCommand){
        FACTORY_COMMAND_TRAIN_SCHEDULE_SET_ENABLED,
        {.train_schedule_set_enabled={train,true}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_OK);
    const uint64_t enabled_tick=factory_simulation_get_tick(s);

    FactorySnapshotBuffer checkpoint={0};
    FactorySimulation *loaded=NULL;
    bool loaded_started=false;
    bool saved_partial=false;
    bool saw_partial_load=false;
    bool saw_loaded_departure=false;
    bool saw_unload_arrival=false;
    bool saw_partial_unload=false;
    bool saw_empty_departure=false;
    bool returned_to_load=false;
    bool saw_reserved_signal=false;
    bool saw_travel_with_cargo=false;
    bool saw_destination_storage=false;
    uint64_t full_tick=0U,load_departure_tick=0U,unload_arrival_tick=0U;
    uint64_t empty_tick=0U,unload_departure_tick=0U,return_tick=0U;
    uint32_t load_wait_events=0U,load_advance_events=0U;
    uint32_t unload_wait_events=0U,unload_advance_events=0U;
    for(uint32_t iteration=0U;iteration<2000U&&!returned_to_load;++iteration){
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        if(loaded!=NULL&&loaded_started){
            CHECK(factory_simulation_tick(loaded)==FACTORY_RESULT_OK);
        }
        FactoryLocomotiveInspection locomotive_state;
        FactoryCargoWagonInspection wagon_state;
        FactoryRailStationInspection load_state,unload_state;
        CHECK(factory_simulation_get_locomotive(s,train,&locomotive_state));
        CHECK(factory_simulation_get_cargo_wagon(s,wagon,&wagon_state));
        CHECK(factory_simulation_get_rail_station(s,load_station,&load_state));
        CHECK(factory_simulation_get_rail_station(s,unload_station,&unload_state));
        FactoryStorage destination_state;
        CHECK(factory_simulation_get_storage(s,destination,&destination_state));
        CHECK(iron_plate_total(s,source,source_inserter,load_station,wagon,
            unload_station,destination_inserter,destination)==100U);
        CHECK(factory_simulation_get_rail_signal(s,signal,&signal_state));
        saw_reserved_signal|=signal_state.reserved_train_id==train;
        saw_travel_with_cargo|=locomotive_state.route_status
                ==FACTORY_TRAIN_ROUTE_ACTIVE
            &&wagon_state.cargo_quantity!=0U;
        saw_destination_storage|=destination_state.iron_plate_amount!=0U;
        if(wagon_state.cargo_quantity>0U
            &&wagon_state.cargo_quantity<FACTORY_CARGO_WAGON_CAPACITY
            &&locomotive_state.current_stop_index==0U){
            saw_partial_load=true;
            CHECK(locomotive_state.route_status==FACTORY_TRAIN_ROUTE_ARRIVED);
            if(!saved_partial){
                CHECK(factory_simulation_create_snapshot(s,&checkpoint)
                    ==FACTORY_RESULT_OK);
                CHECK(factory_simulation_load_snapshot(checkpoint.data,
                    checkpoint.size,&loaded)==FACTORY_RESULT_OK);
                saved_partial=true;
            }
        }
        int freight_index=-1,wait_index=-1,advance_index=-1;
        for(size_t index=0U;
            index<factory_simulation_get_event_count(s);++index){
            const FactoryEvent *event=factory_simulation_get_event(s,index);
            if(event->type==FACTORY_EVENT_RAIL_FREIGHT_TRANSFERRED
                &&(event->entity_id==load_station
                    ||event->entity_id==unload_station)){
                freight_index=(int)index;
            }
            if(event->type==FACTORY_EVENT_TRAIN_WAIT_COMPLETED){
                wait_index=(int)index;
                if(event->related_entity_id==load_station)++load_wait_events;
                if(event->related_entity_id==unload_station)++unload_wait_events;
            }
            if(event->type==FACTORY_EVENT_TRAIN_SCHEDULE_ADVANCED){
                advance_index=(int)index;
                if(event->related_entity_id==unload_station)++load_advance_events;
                if(event->related_entity_id==load_station)++unload_advance_events;
            }
        }
        if(wait_index>=0){
            CHECK(freight_index>=0&&freight_index<wait_index);
            CHECK(advance_index>wait_index);
        }
        const uint64_t tick=factory_simulation_get_tick(s);
        if(full_tick==0U&&wagon_state.cargo_quantity==100U)full_tick=tick;
        if(full_tick!=0U&&!saw_loaded_departure
            &&locomotive_state.current_stop_index==1U
            &&locomotive_state.route_status==FACTORY_TRAIN_ROUTE_ACTIVE){
            saw_loaded_departure=true;load_departure_tick=tick;
            CHECK(locomotive_state.destination_station_id==unload_station);
            CHECK(locomotive_state.route_length>1U);
            for(size_t route_index=0U;
                route_index<locomotive_state.route_length;++route_index){
                FactoryTrainRouteStep route_step;
                CHECK(factory_simulation_get_train_route_step(s,train,
                    route_index,&route_step));
                CHECK(route_step.rail_entity_id!=0U);
            }
            CHECK(locomotive_state.reservation_status
                !=FACTORY_TRAIN_RESERVATION_INVALID);
        }
        if(saw_loaded_departure&&locomotive_state.current_stop_index==1U
            &&locomotive_state.route_status==FACTORY_TRAIN_ROUTE_ARRIVED){
            if(!saw_unload_arrival)unload_arrival_tick=tick;
            saw_unload_arrival=true;
        }
        if(saw_unload_arrival&&wagon_state.cargo_quantity>0U
            &&wagon_state.cargo_quantity<100U)saw_partial_unload=true;
        if(saw_unload_arrival&&empty_tick==0U
            &&wagon_state.cargo_quantity==0U)empty_tick=tick;
        if(empty_tick!=0U&&!saw_empty_departure
            &&locomotive_state.current_stop_index==0U
            &&locomotive_state.route_status==FACTORY_TRAIN_ROUTE_ACTIVE){
            saw_empty_departure=true;unload_departure_tick=tick;
            CHECK(locomotive_state.destination_station_id==load_station);
        }
        if(saw_empty_departure&&locomotive_state.current_stop_index==0U
            &&locomotive_state.route_status==FACTORY_TRAIN_ROUTE_ARRIVED){
            returned_to_load=true;return_tick=tick;
        }
        if(loaded!=NULL){
            FactorySnapshotBuffer a={0},b={0};
            FactoryLocomotiveInspection loaded_locomotive;
            FactoryCargoWagonInspection loaded_wagon;
            FactoryRailStationInspection loaded_load,loaded_unload;
            CHECK(factory_simulation_get_locomotive(loaded,train,
                &loaded_locomotive));
            CHECK(factory_simulation_get_cargo_wagon(loaded,wagon,
                &loaded_wagon));
            CHECK(factory_simulation_get_rail_station(loaded,load_station,
                &loaded_load));
            CHECK(factory_simulation_get_rail_station(loaded,unload_station,
                &loaded_unload));
            CHECK(loaded_locomotive.current_stop_index
                ==locomotive_state.current_stop_index);
            CHECK(loaded_locomotive.destination_station_id
                ==locomotive_state.destination_station_id);
            CHECK(loaded_locomotive.route_status==locomotive_state.route_status);
            CHECK(loaded_locomotive.reservation_status
                ==locomotive_state.reservation_status);
            CHECK(loaded_wagon.cargo_quantity==wagon_state.cargo_quantity);
            CHECK(loaded_load.freight_quantity==load_state.freight_quantity);
            CHECK(loaded_unload.freight_quantity==unload_state.freight_quantity);
            CHECK(factory_simulation_create_snapshot(s,&a)==FACTORY_RESULT_OK);
            CHECK(factory_simulation_create_snapshot(loaded,&b)
                ==FACTORY_RESULT_OK);
            CHECK(a.size==b.size&&memcmp(a.data,b.data,a.size)==0);
            factory_snapshot_buffer_destroy(&a);
            factory_snapshot_buffer_destroy(&b);
            loaded_started=true;
        }
    }
    CHECK(saved_partial&&saw_partial_load&&saw_loaded_departure);
    CHECK(saw_unload_arrival&&saw_partial_unload&&saw_empty_departure);
    CHECK(returned_to_load&&return_tick>unload_departure_tick);
    CHECK(saw_travel_with_cargo&&saw_destination_storage);
    CHECK(full_tick!=0U&&load_departure_tick>=full_tick);
    CHECK(unload_arrival_tick>load_departure_tick);
    CHECK(empty_tick>=unload_arrival_tick);
    CHECK(load_wait_events==1U&&load_advance_events==1U);
    CHECK(unload_wait_events==1U&&unload_advance_events==1U);
    CHECK(saw_reserved_signal);
    printf("autonomous freight ticks: enabled=%llu full=%llu load_depart=%llu unload_arrive=%llu empty=%llu unload_depart=%llu return=%llu\n",
        (unsigned long long)enabled_tick,(unsigned long long)full_tick,
        (unsigned long long)load_departure_tick,
        (unsigned long long)unload_arrival_tick,(unsigned long long)empty_tick,
        (unsigned long long)unload_departure_tick,(unsigned long long)return_tick);

    CHECK(loaded!=NULL&&loaded_started);
    factory_snapshot_buffer_destroy(&checkpoint);
    factory_simulation_destroy(loaded);
    factory_simulation_destroy(s);
    factory_world_destroy(world);
}

int main(void)
{
    timed_wait_snapshot_and_manual_ownership();
    cargo_conditions_and_zero_wagon();
    autonomous_scheduled_freight_loop();
    return failures!=0?1:0;
}
