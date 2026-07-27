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
    {
        FactoryFluidStorage *input=factory_fluid_storage_store_find_slot_mutable(
            &s->fluid_storages,1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
        CHECK(input!=NULL);
        CHECK(factory_fluid_storage_insert(input,FACTORY_FLUID_WATER,100U)
            ==FACTORY_RESULT_FLUID_INCOMPATIBLE);
        CHECK(factory_fluid_storage_insert(input,FACTORY_FLUID_STEAM,99U)
            ==FACTORY_RESULT_OK);
    }
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

/* A fired cycle's exhaust is authoritative fluid, not a discarded byproduct:
 * it must land in the output slot atomically with the steam it came from,
 * under the exhaust fluid's own distinct identity. */
static void test_exhaust_produced_atomically_with_steam(void)
{
    FactoryWorld *w=factory_world_create(3U,3U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactorySteamTurbineInspection t;
    FactoryFluidStorage *input;
    FactoryFluidStorage *output;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={1,1}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    input=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    output=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT);
    CHECK(input!=NULL&&output!=NULL);
    CHECK(output->fluid_type==FACTORY_FLUID_NONE&&output->quantity==0U);
    input->fluid_type=FACTORY_FLUID_STEAM; input->quantity=200U;
    CHECK(factory_steam_turbine_consume_for_generation(s,1U,200U));
    CHECK(input->quantity==100U);
    CHECK(output->fluid_type==FACTORY_FLUID_EXHAUST_STEAM
        &&output->quantity==100U);
    CHECK(factory_simulation_get_steam_turbine(s,1U,&t)==FACTORY_RESULT_OK);
    CHECK(t.exhaust_fluid==FACTORY_FLUID_EXHAUST_STEAM
        &&t.stored_exhaust==100U
        &&t.exhaust_produced_last_tick==100U);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* A generator's own exhaust output storage gates its cycle exactly like
 * upstream input steam does: plentiful steam does not matter if there is
 * nowhere for that steam's exhaust to go. */
static void test_generation_blocked_by_full_exhaust_output(void)
{
    FactoryWorld *w=factory_world_create(3U,3U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactorySteamTurbineInspection t;
    FactoryFluidStorage *input;
    FactoryFluidStorage *output;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={1,1}}});                       /* 1 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={0,1}}});                                 /* 2, west */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_POWER_POLE,
        {.place_power_pole={2,1}}});                           /* 3 */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    input=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    output=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT);
    CHECK(input!=NULL&&output!=NULL);
    /* Plenty of steam, but only 50 units of exhaust room -- less than one
     * complete 100-unit cycle's worth of exhaust. */
    input->fluid_type=FACTORY_FLUID_STEAM; input->quantity=250U;
    output->fluid_type=FACTORY_FLUID_EXHAUST_STEAM; output->quantity=1950U;
    CHECK(factory_steam_turbine_available_generation(s,1U)==0U);
    s->events.recording=true;
    factory_simulation_clear_events(s);
    CHECK(!factory_steam_turbine_consume_for_generation(s,1U,200U));
    s->events.recording=false;
    /* No cycle-completed event on a blocked attempt: the event represents
     * a fired cycle's physical outcome, and no cycle fired here. */
    CHECK(factory_simulation_get_event_count(s)==0U);
    CHECK(input->quantity==250U&&output->quantity==1950U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_turbine(s,1U,&t)==FACTORY_RESULT_OK);
    CHECK(t.activity==FACTORY_STEAM_TURBINE_BLOCKED_EXHAUST_FULL);
    CHECK(t.actual_output==0U&&t.steam_consumed_last_tick==0U);
    CHECK(t.stored_steam==250U&&t.stored_exhaust==1950U);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* Live steam and exhaust steam share the vapor class, but that is not what
 * keeps them apart: exact fluid-type identity does, enforced by the same
 * generic storage mismatch check every other fluid pair relies on. */
static void test_exhaust_and_live_steam_are_distinct_identities(void)
{
    FactoryWorld *w=factory_world_create(3U,3U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactoryFluidStorageInspection output;
    FactoryFluidStorage *slot;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={1,1}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_fluid_storage_slot(s,1U,
        FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT,&output)
        ==FACTORY_RESULT_OK);
    CHECK(output.accepted_fluid_classes==FACTORY_FLUID_CLASS_VAPOR);
    slot=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT);
    CHECK(slot!=NULL);
    CHECK(factory_fluid_storage_insert(slot,FACTORY_FLUID_EXHAUST_STEAM,50U)
        ==FACTORY_RESULT_OK);
    CHECK(factory_fluid_storage_insert(slot,FACTORY_FLUID_STEAM,10U)
        ==FACTORY_RESULT_FLUID_MISMATCH);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* Declared identity, not current contents, is what a dedicated slot
 * enforces: an empty live-steam input must reject exhaust steam exactly
 * as a non-empty one would, both by direct insert and by transfer from a
 * genuine exhaust-steam source. */
static void test_empty_input_rejects_exhaust_steam(void)
{
    FactoryWorld *w=factory_world_create(3U,3U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactoryFluidStorage *input;
    FactoryFluidStorage *exhaust_source;
    FactoryFluidType transferred=FACTORY_FLUID_NONE;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={1,1}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    input=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    exhaust_source=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT);
    CHECK(input!=NULL && exhaust_source!=NULL);
    CHECK(input->quantity==0U && input->fluid_type==FACTORY_FLUID_NONE);

    CHECK(factory_fluid_storage_insert(input,FACTORY_FLUID_EXHAUST_STEAM,50U)
        ==FACTORY_RESULT_FLUID_MISMATCH);
    CHECK(input->quantity==0U && input->fluid_type==FACTORY_FLUID_NONE);

    exhaust_source->fluid_type=FACTORY_FLUID_EXHAUST_STEAM;
    exhaust_source->quantity=80U;
    CHECK(factory_fluid_storage_transfer(exhaust_source,input,50U,&transferred)
        ==FACTORY_RESULT_FLUID_MISMATCH);
    CHECK(input->quantity==0U && input->fluid_type==FACTORY_FLUID_NONE);
    CHECK(exhaust_source->quantity==80U
        && exhaust_source->fluid_type==FACTORY_FLUID_EXHAUST_STEAM);

    CHECK(factory_fluid_storage_insert(input,FACTORY_FLUID_STEAM,50U)
        ==FACTORY_RESULT_OK);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* An empty exhaust output cannot be transferred into an empty live-steam-
 * declared destination either: the rejection is symmetric and holds
 * regardless of which side is empty, because it is checked from the
 * destination's declared identity, not from either side's quantity. */
static void test_exhaust_output_cannot_transfer_into_live_steam_destination(
    void)
{
    FactoryWorld *w=factory_world_create(6U,3U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactoryFluidStorage *source_exhaust;
    FactoryFluidStorage *destination_input;
    FactoryFluidType transferred=FACTORY_FLUID_NONE;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={1,1}}});
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={4,1}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    source_exhaust=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT);
    destination_input=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,2U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    CHECK(source_exhaust!=NULL && destination_input!=NULL);
    CHECK(destination_input->quantity==0U
        && destination_input->fluid_type==FACTORY_FLUID_NONE);
    /* Source is empty too: this is the strict empty-into-empty case. */
    CHECK(source_exhaust->quantity==0U
        && source_exhaust->fluid_type==FACTORY_FLUID_NONE);

    CHECK(factory_fluid_storage_transfer(
        source_exhaust,destination_input,10U,&transferred)
        ==FACTORY_RESULT_INSUFFICIENT_FLUID);

    source_exhaust->fluid_type=FACTORY_FLUID_EXHAUST_STEAM;
    source_exhaust->quantity=60U;
    CHECK(factory_fluid_storage_transfer(
        source_exhaust,destination_input,40U,&transferred)
        ==FACTORY_RESULT_FLUID_MISMATCH);
    CHECK(destination_input->quantity==0U
        && destination_input->fluid_type==FACTORY_FLUID_NONE);
    CHECK(source_exhaust->quantity==60U
        && source_exhaust->fluid_type==FACTORY_FLUID_EXHAUST_STEAM);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* Demolition must not silently discard stored fluid: blocked while either
 * the live-steam input or the exhaust-steam output holds anything,
 * independently and together, and only succeeds once both are empty. */
static void test_demolition_blocked_by_stored_fluid(void)
{
    FactoryWorld *w=factory_world_create(3U,3U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactoryFluidStorage *input;
    FactoryFluidStorage *exhaust;
    FactoryFluidType removed=FACTORY_FLUID_NONE;
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={1,1}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    input=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    exhaust=factory_fluid_storage_store_find_slot_mutable(&s->fluid_storages,
        1U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT);
    CHECK(input!=NULL && exhaust!=NULL);

    /* Live steam alone. */
    CHECK(factory_fluid_storage_insert(input,FACTORY_FLUID_STEAM,40U)
        ==FACTORY_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    CHECK(input->quantity==40U);

    /* Exhaust steam alone (live steam drained first). */
    CHECK(factory_fluid_storage_remove(input,40U,&removed)
        ==FACTORY_RESULT_OK);
    CHECK(factory_fluid_storage_insert(exhaust,FACTORY_FLUID_EXHAUST_STEAM,
        25U)==FACTORY_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    CHECK(exhaust->quantity==25U);

    /* Both at once. */
    CHECK(factory_fluid_storage_insert(input,FACTORY_FLUID_STEAM,15U)
        ==FACTORY_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    CHECK(input->quantity==15U && exhaust->quantity==25U);

    /* Both empty: demolition succeeds. Every unit above was removed by an
     * explicit, accounted operation -- demolition itself never discards. */
    CHECK(factory_fluid_storage_remove(input,15U,&removed)
        ==FACTORY_RESULT_OK);
    CHECK(factory_fluid_storage_remove(exhaust,25U,&removed)
        ==FACTORY_RESULT_OK);
    submit(s,(FactoryCommand){FACTORY_COMMAND_DEMOLISH_ENTITY,
        {.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_OK);
    CHECK(factory_fluid_storage_store_find_slot(&s->fluid_storages,1U,
        FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT)==NULL);
    CHECK(factory_fluid_storage_store_find_slot(&s->fluid_storages,1U,
        FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT)==NULL);
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
    test_exhaust_produced_atomically_with_steam();
    test_generation_blocked_by_full_exhaust_output();
    test_exhaust_and_live_steam_are_distinct_identities();
    test_empty_input_rejects_exhaust_steam();
    test_exhaust_output_cannot_transfer_into_live_steam_destination();
    test_demolition_blocked_by_stored_fluid();
    test_dispatch_uses_one_complete_cycle();
    return failures==0?0:1;
}
