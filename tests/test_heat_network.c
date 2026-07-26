#include <foundation/heat_network.h>
#include <foundation/presentation.h>
#include <foundation/simulation.h>
#include <foundation/snapshot.h>

#include <stdio.h>

#include "simulation_internal.h"

#define CHECK(c) do { if (!(c)) {                                           \
    fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #c);\
    return 1;                                                               \
} } while (0)

static FactorySimulation *make_simulation(void)
{
    FactoryWorld *world=factory_world_create(9U,7U);
    FactorySimulation *s;
    if (world==NULL) return NULL;
    s=factory_simulation_create_with_construction_units(world,10000U);
    if (s==NULL) factory_world_destroy(world);
    return s;
}

static void destroy_borrowing(FactorySimulation *s)
{
    FactoryWorld *world;
    if (s==NULL) return;
    world=(FactoryWorld *)factory_simulation_get_world(s);
    factory_simulation_destroy(s); factory_world_destroy(world);
}

static int submit(FactorySimulation *s,FactoryCommand c)
{ return factory_simulation_submit_command(s,&c)==FACTORY_RESULT_OK?0:1; }

static int build_line(FactorySimulation *s,bool fuel,bool water)
{
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_REACTOR_CORE,
        {.place_reactor_core={1,2}}})==0);                         /* 1 */
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={2,2}}})==0);                       /* 2 */
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={3,2}}})==0);                       /* 3 */
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_EXCHANGER,
        {.place_heat_exchanger={4,2}}})==0);                       /* 4 */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    if (water) {
        FactoryFluidStorage *input=factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages,4U,
            FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT);
        CHECK(input!=NULL && factory_fluid_storage_insert(
            input,FACTORY_FLUID_WATER,100U)==FACTORY_RESULT_OK);
    }
    if (fuel) {
        CHECK(submit(s,(FactoryCommand){
            FACTORY_COMMAND_INSERT_REACTOR_FUEL,
            {.insert_reactor_fuel={
                1U,FACTORY_NUCLEAR_FUEL_BASIC_ROD}}})==0);
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    }
    return 0;
}

static int test_topology_same_tick_and_atomic_conversion(void)
{
    FactorySimulation *s=make_simulation();
    FactoryHeatConductorInspection first,second;
    FactoryHeatExchangerInspection exchanger;
    FactoryReactorInspection reactor;
    FactoryHeatNetworkInspection network;
    const FactoryHeatExchangeRecipe *recipe=
        factory_heat_exchange_recipe_get(
            FACTORY_HEAT_EXCHANGE_RECIPE_WATER_TO_STEAM);
    CHECK(s!=NULL && build_line(s,true,true)==0);
    CHECK(recipe!=NULL && recipe->heat_input==100U
        && recipe->water_input==100U && recipe->steam_output==100U
        && recipe->maximum_cycles_per_tick==1U
        && factory_heat_exchange_recipe_get(99U)==NULL);
    CHECK(factory_simulation_get_heat_conductor(s,2U,&first)
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_heat_conductor(s,3U,&second)
        ==FACTORY_RESULT_OK);
    CHECK(first.network_id==2U && second.network_id==2U
        && (first.connection_mask&FACTORY_HEAT_CONNECTION_WEST)!=0U
        && (second.connection_mask&FACTORY_HEAT_CONNECTION_EAST)!=0U);
    CHECK(factory_simulation_get_heat_network_count(s)==1U);
    CHECK(factory_simulation_get_heat_network(s,0U,&network)
        ==FACTORY_RESULT_OK);
    CHECK(network.network_id==2U && network.conductor_count==2U
        && network.source_count==1U && network.consumer_count==1U
        && network.transferred_last_tick==100U);
    CHECK(factory_simulation_get_reactor(s,1U,&reactor)==FACTORY_RESULT_OK);
    CHECK(reactor.stored_heat==0U && reactor.remaining_heat_yield==9900U);
    CHECK(factory_simulation_get_heat_exchanger(s,4U,&exchanger)
        ==FACTORY_RESULT_OK);
    CHECK(exchanger.stored_water==0U && exchanger.stored_steam==100U
        && exchanger.consumed_heat_last_tick==100U
        && exchanger.consumed_water_last_tick==100U
        && exchanger.produced_steam_last_tick==100U
        && exchanger.activity==FACTORY_HEAT_EXCHANGER_WORKING);
    {
        const FactoryEvent *transfer=NULL,*cycle=NULL;
        for (size_t i=0U;i<factory_simulation_get_event_count(s);++i) {
            const FactoryEvent *event=factory_simulation_get_event(s,i);
            if (event->type==FACTORY_EVENT_HEAT_TRANSFERRED) transfer=event;
            if (event->type==FACTORY_EVENT_HEAT_EXCHANGER_CYCLE_COMPLETED)
                cycle=event;
        }
        CHECK(transfer!=NULL && transfer->entity_id==1U
            && transfer->related_entity_id==4U && transfer->quantity==100U);
        CHECK(cycle!=NULL && cycle->entity_id==4U && cycle->quantity==100U
            && cycle->related_quantity==100U && cycle->third_quantity==100U
            && cycle->tick==factory_simulation_get_tick(s)-1U);
    }
    destroy_borrowing(s);
    return 0;
}

static int test_atomic_blocks_and_resume(void)
{
    FactorySimulation *s=make_simulation();
    FactoryHeatExchangerInspection exchanger;
    CHECK(s!=NULL && build_line(s,true,false)==0);
    CHECK(factory_simulation_get_heat_exchanger(s,4U,&exchanger)
        ==FACTORY_RESULT_OK);
    CHECK(exchanger.activity==FACTORY_HEAT_EXCHANGER_BLOCKED_NO_WATER
        && s->reactors.items[0].heat_storage.stored_heat==100U);
    CHECK(factory_fluid_storage_insert(
        factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,4U,
            FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT),
        FACTORY_FLUID_WATER,50U)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(s->reactors.items[0].heat_storage.stored_heat==200U);
    CHECK(factory_simulation_get_heat_exchanger(s,4U,&exchanger)
        ==FACTORY_RESULT_OK && exchanger.stored_water==50U
        && exchanger.stored_steam==0U);
    CHECK(factory_fluid_storage_insert(
        factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,4U,
            FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT),
        FACTORY_FLUID_WATER,50U)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_heat_exchanger(s,4U,&exchanger)
        ==FACTORY_RESULT_OK && exchanger.stored_water==0U
        && exchanger.stored_steam==100U);
    CHECK(s->reactors.items[0].heat_storage.stored_heat==200U);
    destroy_borrowing(s);
    return 0;
}

static int test_split_merge_and_snapshot_continuation(void)
{
    FactorySimulation *a=make_simulation(),*b=NULL;
    FactorySnapshotBuffer snapshot={0};
    FactoryHeatConductorInspection c;
    CHECK(a!=NULL && build_line(a,true,true)==0);
    CHECK(factory_simulation_create_snapshot(a,&snapshot)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(snapshot.data,snapshot.size,&b)
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_heat_conductor(b,2U,&c)==FACTORY_RESULT_OK
        && c.network_id==2U);
    for (uint32_t i=0U;i<10U;++i) {
        FactoryReactorInspection ar,br;
        FactoryHeatExchangerInspection ae,be;
        CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK
            && factory_simulation_tick(b)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_reactor(a,1U,&ar)==FACTORY_RESULT_OK
            && factory_simulation_get_reactor(b,1U,&br)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_heat_exchanger(a,4U,&ae)
                ==FACTORY_RESULT_OK
            && factory_simulation_get_heat_exchanger(b,4U,&be)
                ==FACTORY_RESULT_OK);
        CHECK(ar.stored_heat==br.stored_heat
            && ar.remaining_heat_yield==br.remaining_heat_yield
            && ae.stored_water==be.stored_water
            && ae.stored_steam==be.stored_steam);
    }
    CHECK(submit(a,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={3U}}})==0);
    CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_heat_network_count(a)==1U);
    {
        FactoryHeatPortInspection p;
        CHECK(factory_simulation_get_heat_port(a,4U,
            FACTORY_HEAT_PORT_EXCHANGER_INPUT,&p)==FACTORY_RESULT_OK
            && !p.connected);
    }
    factory_snapshot_buffer_destroy(&snapshot);
    factory_simulation_destroy(b); destroy_borrowing(a);
    return 0;
}

static int test_full_reactor_resume_and_multi_priority(void)
{
    FactorySimulation *s=make_simulation();
    FactoryFluidStorage *water;
    CHECK(s!=NULL && build_line(s,true,false)==0);
    water=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,4U,
        FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT);
    CHECK(factory_fluid_storage_insert(water,FACTORY_FLUID_WATER,200U)
        ==FACTORY_RESULT_OK);
    s->reactors.items[0].heat_storage.stored_heat=
        FACTORY_REACTOR_HEAT_CAPACITY;
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(s->reactors.items[0].heat_storage.stored_heat==9900U
        && s->reactors.items[0].remaining_heat_yield==9900U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(s->reactors.items[0].heat_storage.stored_heat==9900U
        && s->reactors.items[0].remaining_heat_yield==9800U);
    destroy_borrowing(s);

    s=make_simulation();
    CHECK(s!=NULL);
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_REACTOR_CORE,
        {.place_reactor_core={2,3}}})==0);                         /* 1 */
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_REACTOR_CORE,
        {.place_reactor_core={3,2}}})==0);                         /* 2 */
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={3,3}}})==0);                       /* 3 */
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_EXCHANGER,
        {.place_heat_exchanger={4,3}}})==0);                       /* 4 */
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_EXCHANGER,
        {.place_heat_exchanger={3,4}}})==0);                       /* 5 */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    s->reactors.items[0].heat_storage.stored_heat=150U;
    s->reactors.items[1].heat_storage.stored_heat=100U;
    for (FactoryEntityId id=4U;id<=5U;++id) {
        water=factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages,id,
            FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT);
        CHECK(factory_fluid_storage_insert(water,FACTORY_FLUID_WATER,100U)
            ==FACTORY_RESULT_OK);
    }
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(s->reactors.items[0].heat_storage.stored_heat==0U);
    CHECK(s->reactors.items[1].heat_storage.stored_heat==50U);
    destroy_borrowing(s);
    return 0;
}

static int test_network_split_and_merge(void)
{
    FactorySimulation *s=make_simulation();
    CHECK(s!=NULL);
    for (int32_t x=1;x<=3;++x)
        CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
            {.place_heat_conductor={x,1}}})==0);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK
        && factory_simulation_get_heat_network_count(s)==1U);
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={2U}}})==0);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK
        && factory_simulation_get_heat_network_count(s)==2U);
    {
        bool split=false;
        for (size_t i=0U;i<factory_simulation_get_event_count(s);++i)
            split |= factory_simulation_get_event(s,i)->type
                ==FACTORY_EVENT_HEAT_NETWORK_SPLIT;
        CHECK(split);
    }
    CHECK(submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={2,1}}})==0);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK
        && factory_simulation_get_heat_network_count(s)==1U);
    {
        bool merged=false;
        for (size_t i=0U;i<factory_simulation_get_event_count(s);++i)
            merged |= factory_simulation_get_event(s,i)->type
                ==FACTORY_EVENT_HEAT_NETWORK_MERGED;
        CHECK(merged);
    }
    destroy_borrowing(s);
    return 0;
}

int main(void)
{
    CHECK(test_topology_same_tick_and_atomic_conversion()==0);
    CHECK(test_atomic_blocks_and_resume()==0);
    CHECK(test_split_merge_and_snapshot_continuation()==0);
    CHECK(test_full_reactor_resume_and_multi_priority()==0);
    CHECK(test_network_split_and_merge()==0);
    puts("heat network tests passed");
    return 0;
}
