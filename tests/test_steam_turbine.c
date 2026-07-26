#include "foundation/snapshot.h"

#include "fluid_internal.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <stdio.h>

static int failures;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

static void submit(FactorySimulation *s, FactoryCommand command)
{
    CHECK(factory_simulation_submit_command(s,&command)==FACTORY_RESULT_OK);
}

static void test_definition_and_construction(void)
{
    FactoryWorld *w=factory_world_create(5U,5U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    const FactorySteamTurbineDefinition *d=
        factory_steam_turbine_definition_get(
            FACTORY_STEAM_TURBINE_DEFINITION_BASIC);
    FactorySteamTurbineInspection t;
    FactoryFluidStorageInspection storage;
    CHECK(factory_steam_turbine_definition_count()==1U);
    CHECK(factory_steam_turbine_definition_at(0U)==d);
    CHECK(factory_steam_turbine_definition_at(1U)==NULL);
    CHECK(factory_steam_turbine_definition_is_valid(d));
    CHECK(d!=NULL&&d->steam_per_cycle==100U&&d->energy_per_cycle==200U);
    CHECK(d!=NULL&&d->storage_capacity==2000U
        &&d->construction_cost==75U);
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={2,2}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_turbine(s,1U,&t)==FACTORY_RESULT_OK);
    CHECK(t.stored_steam==0U&&t.steam_capacity==2000U);
    CHECK(factory_simulation_get_fluid_storage_slot(s,1U,
        FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT,&storage)
        ==FACTORY_RESULT_OK);
    CHECK(storage.accepted_fluid_classes==FACTORY_FLUID_CLASS_VAPOR);
    CHECK(factory_simulation_submit_fluid_insert(
        s,1U,FACTORY_FLUID_WATER,100U)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_FLUID_INCOMPATIBLE);
    CHECK(factory_simulation_submit_fluid_insert(
        s,1U,FACTORY_FLUID_STEAM,99U)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_turbine(s,1U,&t)==FACTORY_RESULT_OK);
    CHECK(t.stored_steam==99U&&t.available_output==0U
        &&t.actual_output==0U);
    {
        FactorySnapshotBuffer snapshot={0};
        FactorySimulation *loaded=NULL;
        CHECK(factory_simulation_create_snapshot(s,&snapshot)
            ==FACTORY_RESULT_OK);
        CHECK(factory_simulation_load_snapshot(
            snapshot.data,snapshot.size,&loaded)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_steam_turbine(loaded,1U,&t)
            ==FACTORY_RESULT_OK);
        CHECK(t.stored_steam==99U&&t.actual_output==0U
            &&factory_simulation_get_event_count(loaded)==0U);
        factory_simulation_destroy(loaded);
        factory_snapshot_buffer_destroy(&snapshot);
    }
    factory_simulation_destroy(s); factory_world_destroy(w);
}

static void test_atomic_consumption_and_event(void)
{
    FactoryWorld *w=factory_world_create(3U,3U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactorySteamTurbineInspection t;
    FactoryFluidStorage *storage;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={1,1}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    storage=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    storage->fluid_type=FACTORY_FLUID_STEAM; storage->quantity=250U;
    CHECK(factory_steam_turbine_available_generation(s,1U)==200U);
    CHECK(!factory_steam_turbine_consume_for_generation(s,1U,100U));
    CHECK(storage->quantity==250U);
    factory_simulation_clear_events(s);
    s->events.recording=true;
    CHECK(factory_steam_turbine_consume_for_generation(s,1U,200U));
    s->events.recording=false;
    CHECK(storage->quantity==150U);
    CHECK(factory_simulation_get_steam_turbine(s,1U,&t)==FACTORY_RESULT_OK);
    CHECK(t.actual_output==200U&&t.steam_consumed_last_tick==100U
        &&t.completed_cycles_last_tick==1U);
    CHECK(factory_simulation_get_event_count(s)==1U);
    CHECK(factory_simulation_get_event(s,0U)->type
        ==FACTORY_EVENT_STEAM_TURBINE_CYCLE_COMPLETED);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

static void test_dispatch_uses_one_complete_cycle(void)
{
    static const int32_t positions[10][2]={
        {1,1},{2,1},{3,1},{4,1},{5,1},
        {1,2},{2,2},{4,2},{5,2},{5,3}};
    FactoryWorld *w=factory_world_create(7U,7U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactoryFluidStorage *storage;
    FactorySteamTurbineInspection t;
    size_t i;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={3,3}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={2,3}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={3,2}}});
    for (i=0U;i<10U;++i)
        submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_ASSEMBLER,
            {.place_assembler={positions[i][0],positions[i][1],
                FACTORY_DIRECTION_NORTH}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    storage=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    storage->fluid_type=FACTORY_FLUID_STEAM; storage->quantity=200U;
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_turbine(s,1U,&t)==FACTORY_RESULT_OK);
    CHECK(t.actual_output==200U&&t.steam_consumed_last_tick==100U
        &&t.stored_steam==100U&&t.activity==FACTORY_STEAM_TURBINE_WORKING);
    {
        size_t powered=0U;
        for (i=0U;i<s->power.consumer_count;++i)
            if (s->power.consumers[i].powered) ++powered;
        CHECK(powered==8U);
    }
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_turbine(s,1U,&t)==FACTORY_RESULT_OK);
    CHECK(t.actual_output==200U&&t.stored_steam==0U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_turbine(s,1U,&t)==FACTORY_RESULT_OK);
    CHECK(t.actual_output==0U&&t.steam_consumed_last_tick==0U);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

int main(void)
{
    test_definition_and_construction();
    test_atomic_consumption_and_event();
    test_dispatch_uses_one_complete_cycle();
    return failures==0?0:1;
}
