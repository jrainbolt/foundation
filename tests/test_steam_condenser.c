#include "foundation/presentation.h"
#include "foundation/snapshot.h"

#include "fluid_internal.h"
#include "simulation_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { (void)fprintf(stderr,                    \
    "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (false)

#define GENERATOR_FUEL_QUANTITY 10000U

static void submit(FactorySimulation *s, FactoryCommand command)
{
    if (command.type == FACTORY_COMMAND_PLACE_POWER_GENERATOR)
        s->fixture_initial_generator_fuel = GENERATOR_FUEL_QUANTITY;
    CHECK(factory_simulation_submit_command(s,&command)==FACTORY_RESULT_OK);
}

static FactoryCommand pole(int32_t x,int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_POLE,{.place_power_pole={x,y}}};
}

static FactoryCommand generator(int32_t x,int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_POWER_GENERATOR,
        {.place_power_generator={x,y}}};
}

static FactoryCommand condenser_command(int32_t x,int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_STEAM_CONDENSER,
        {.place_steam_condenser={x,y}}};
}

static void set_steam(FactorySimulation *s,FactoryEntityId id,uint32_t amount)
{
    FactoryFluidStorage *storage=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,id,FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT);
    CHECK(storage!=NULL);
    if (storage==NULL) return;
    storage->fluid_type=amount==0U?FACTORY_FLUID_NONE:FACTORY_FLUID_EXHAUST_STEAM;
    storage->quantity=amount;
}

static void set_water(FactorySimulation *s,FactoryEntityId id,uint32_t amount)
{
    FactoryFluidStorage *storage=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,id,FACTORY_FLUID_STORAGE_STEAM_CONDENSER_OUTPUT);
    CHECK(storage!=NULL);
    if (storage==NULL) return;
    storage->fluid_type=amount==0U?FACTORY_FLUID_NONE:FACTORY_FLUID_WATER;
    storage->quantity=amount;
}

/* ---- Definition ---- */

static void test_definition(void)
{
    const FactorySteamCondenserDefinition *d=
        factory_steam_condenser_definition_get(
            FACTORY_STEAM_CONDENSER_DEFINITION_BASIC);
    CHECK(factory_steam_condenser_definition_count()==1U);
    CHECK(factory_steam_condenser_definition_at(0U)==d);
    CHECK(factory_steam_condenser_definition_at(1U)==NULL);
    CHECK(factory_steam_condenser_definition_is_valid(d));
    CHECK(!factory_steam_condenser_definition_is_valid(NULL));
    CHECK(d!=NULL && d->input_fluid==FACTORY_FLUID_EXHAUST_STEAM
        && d->output_fluid==FACTORY_FLUID_WATER
        && d->steam_per_cycle==100U && d->water_per_cycle==100U
        && d->power_per_cycle==50U && d->maximum_cycles_per_tick==1U
        && d->steam_capacity==2000U && d->water_capacity==2000U
        && d->construction_cost==75U);
    /* Stable lookup: repeated calls return the same immutable pointer. */
    CHECK(factory_steam_condenser_definition_get(
            FACTORY_STEAM_CONDENSER_DEFINITION_BASIC) == d);
    CHECK(factory_steam_condenser_definition_get(
            FACTORY_STEAM_CONDENSER_DEFINITION_NONE) == NULL);
}

/* ---- Construction ---- */

static void test_construction_and_occupancy(void)
{
    FactoryWorld *w=factory_world_create(5U,5U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,500U);
    FactorySteamCondenserInspection c;
    FactoryFluidStorageInspection steam;
    FactoryFluidStorageInspection water;
    submit(s,condenser_command(2,2));
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_construction_units(s)==500U-75U);
    CHECK(factory_simulation_get_steam_condenser(s,1U,&c)==FACTORY_RESULT_OK);
    CHECK(c.definition_id==FACTORY_STEAM_CONDENSER_DEFINITION_BASIC);
    CHECK(c.stored_steam==0U && c.steam_capacity==2000U);
    CHECK(c.stored_water==0U && c.water_capacity==2000U);
    CHECK(c.activity==FACTORY_STEAM_CONDENSER_DISCONNECTED_FLUID
        || c.activity==FACTORY_STEAM_CONDENSER_DISCONNECTED_POWER);
    CHECK(factory_simulation_get_fluid_storage_slot(s,1U,
        FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT,&steam)
        ==FACTORY_RESULT_OK);
    CHECK(steam.accepted_fluid_classes==FACTORY_FLUID_CLASS_VAPOR);
    CHECK(factory_simulation_get_fluid_storage_slot(s,1U,
        FACTORY_FLUID_STORAGE_STEAM_CONDENSER_OUTPUT,&water)
        ==FACTORY_RESULT_OK);
    CHECK(water.accepted_fluid_classes==FACTORY_FLUID_CLASS_AQUEOUS);
    /* Occupies its tile: a second placement on the same tile fails. */
    submit(s,condenser_command(2,2));
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        !=FACTORY_RESULT_OK);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

static void test_demolition(void)
{
    FactoryWorld *w=factory_world_create(5U,5U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,500U);
    uint32_t balance;
    submit(s,condenser_command(2,2));
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    balance=factory_simulation_construction_units(s);
    set_steam(s,1U,50U);
    submit(s,(FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY,{.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    set_steam(s,1U,0U);
    set_water(s,1U,25U);
    submit(s,(FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY,{.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    /* Both at once. */
    set_steam(s,1U,10U);
    submit(s,(FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY,{.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_ENTITY_HAS_MATERIAL);
    set_steam(s,1U,0U);
    set_water(s,1U,0U);
    submit(s,(FactoryCommand){
        FACTORY_COMMAND_DEMOLISH_ENTITY,{.demolish_entity={1U}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_construction_units(s)==balance+75U);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* ---- Empty-storage fluid-identity enforcement: a condenser's exhaust
 * input must reject live steam even while it holds nothing, because the
 * slot's declared identity -- not its current contents -- is what the
 * generic fluid system checks. ---- */

static void test_empty_input_rejects_live_steam(void)
{
    FactoryWorld *w=factory_world_create(6U,6U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactoryFluidStorage *condenser_input;
    FactoryFluidStorage *turbine_input;
    FactoryFluidType transferred=FACTORY_FLUID_NONE;
    submit(s,condenser_command(1,1));                        /* 1 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={3,3}}});                        /* 2 */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    condenser_input=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,1U,FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT);
    turbine_input=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,2U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    CHECK(condenser_input!=NULL && turbine_input!=NULL);
    CHECK(condenser_input->quantity==0U
        && condenser_input->fluid_type==FACTORY_FLUID_NONE);

    /* Direct insert of live steam into the empty, exhaust-declared slot. */
    CHECK(factory_fluid_storage_insert(
        condenser_input,FACTORY_FLUID_STEAM,50U)
        ==FACTORY_RESULT_FLUID_MISMATCH);
    CHECK(condenser_input->quantity==0U
        && condenser_input->fluid_type==FACTORY_FLUID_NONE);

    /* Transfer from a genuine live-steam source (a turbine's own input
     * slot, filled directly as the turbine's own tests do) into the same
     * empty, exhaust-declared slot. */
    turbine_input->fluid_type=FACTORY_FLUID_STEAM; turbine_input->quantity=80U;
    CHECK(factory_fluid_storage_transfer(
        turbine_input,condenser_input,50U,&transferred)
        ==FACTORY_RESULT_FLUID_MISMATCH);
    CHECK(condenser_input->quantity==0U
        && condenser_input->fluid_type==FACTORY_FLUID_NONE);
    CHECK(turbine_input->quantity==80U
        && turbine_input->fluid_type==FACTORY_FLUID_STEAM);

    /* The slot's declared identity is unaffected: exhaust steam is still
     * accepted, proving this is a targeted rejection, not a broken slot. */
    CHECK(factory_fluid_storage_insert(
        condenser_input,FACTORY_FLUID_EXHAUST_STEAM,50U)==FACTORY_RESULT_OK);
    CHECK(condenser_input->quantity==50U
        && condenser_input->fluid_type==FACTORY_FLUID_EXHAUST_STEAM);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* ---- Fluid / recipe: build a powered, unconnected-fluid condenser and
 * manipulate its steam/water storage directly (mirroring the existing
 * steam-turbine test style) to exercise the recipe in isolation. ---- */

static FactorySimulation *build_powered_condenser(
    FactoryWorld **out_world, FactoryEntityId *out_condenser_id
)
{
    FactoryWorld *w=factory_world_create(6U,6U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    submit(s,pole(3,3));                 /* 1 */
    submit(s,generator(4,3));            /* 2, avail 100, within pole radius */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={2,2}}});           /* 3, west of the condenser's
                                           * steam input so it is fluid-
                                           * connected (matching the steam
                                           * turbine's own full-tick tests);
                                           * storage is still set directly. */
    submit(s,condenser_command(3,2));    /* 4, within pole radius */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    *out_world=w;
    *out_condenser_id=4U;
    return s;
}

static void test_no_partial_execution_and_exact_accounting(void)
{
    FactoryWorld *w;
    FactoryEntityId id;
    FactorySimulation *s=build_powered_condenser(&w,&id);
    FactorySteamCondenserInspection c;

    /* Insufficient steam: no execution. */
    set_steam(s,id,99U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_condenser(s,id,&c)==FACTORY_RESULT_OK);
    CHECK(c.stored_steam==99U && c.stored_water==0U);
    CHECK(c.activity==FACTORY_STEAM_CONDENSER_NO_STEAM);
    CHECK(c.completed_cycles_last_tick==0U);

    /* Output storage without room for a full cycle: no execution, no
     * fractional conversion. */
    set_steam(s,id,250U);
    set_water(s,id,1950U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_condenser(s,id,&c)==FACTORY_RESULT_OK);
    CHECK(c.stored_steam==250U && c.stored_water==1950U);
    CHECK(c.activity==FACTORY_STEAM_CONDENSER_OUTPUT_FULL);

    /* Sufficient steam and room: exactly one complete cycle. */
    set_water(s,id,0U);
    s->events.recording=true;
    factory_simulation_clear_events(s);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    s->events.recording=false;
    CHECK(factory_simulation_get_steam_condenser(s,id,&c)==FACTORY_RESULT_OK);
    CHECK(c.stored_steam==150U && c.stored_water==100U);
    CHECK(c.activity==FACTORY_STEAM_CONDENSER_WORKING);
    CHECK(c.steam_consumed_last_tick==100U
        && c.water_produced_last_tick==100U
        && c.completed_cycles_last_tick==1U);
    {
        bool found=false;
        size_t i;
        for (i=0U;i<factory_simulation_get_event_count(s);++i) {
            const FactoryEvent *e=factory_simulation_get_event(s,i);
            if (e->type==FACTORY_EVENT_STEAM_CONDENSER_CYCLE_COMPLETED) {
                CHECK(e->entity_id==id);
                CHECK(e->fluid_type==FACTORY_FLUID_EXHAUST_STEAM);
                CHECK(e->related_fluid_type==FACTORY_FLUID_WATER);
                CHECK(e->quantity==100U && e->related_quantity==100U);
                found=true;
            }
        }
        CHECK(found);
    }

    /* Remaining 150 steam supports exactly one more cycle, then stalls. */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_condenser(s,id,&c)==FACTORY_RESULT_OK);
    CHECK(c.stored_steam==50U && c.stored_water==200U);
    CHECK(c.completed_cycles_last_tick==1U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_condenser(s,id,&c)==FACTORY_RESULT_OK);
    CHECK(c.stored_steam==50U && c.stored_water==200U);
    CHECK(c.activity==FACTORY_STEAM_CONDENSER_NO_STEAM);
    CHECK(c.completed_cycles_last_tick==0U);

    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* ---- Power: behaves as an ordinary indivisible power consumer. ---- */

static FactoryCommand pipe_command(int32_t x,int32_t y)
{
    return (FactoryCommand){
        FACTORY_COMMAND_PLACE_PIPE,{.place_pipe={x,y}}};
}

static void test_power_gates_execution(void)
{
    FactoryWorld *w=factory_world_create(8U,8U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    FactorySteamCondenserInspection c;
    /* Fluid-connected (so the check under test is power, not fluid) but
     * placed far (Chebyshev distance 5) from the pole cluster below, so it
     * never competes for that network's power: a clean disconnected-power
     * control case. */
    submit(s,pipe_command(0,1));         /* 1 */
    submit(s,condenser_command(1,1));    /* 2: no pole nearby */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    set_steam(s,2U,200U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_condenser(s,2U,&c)==FACTORY_RESULT_OK);
    CHECK(c.activity==FACTORY_STEAM_CONDENSER_DISCONNECTED_POWER);
    CHECK(c.stored_steam==200U);

    submit(s,pole(6,6));                 /* 3 */
    submit(s,generator(6,5));            /* 4, avail 100, distance 1 */
    submit(s,pipe_command(3,6));         /* 5 */
    submit(s,condenser_command(4,6));    /* 6, distance 2 */
    submit(s,pipe_command(3,5));         /* 7 */
    submit(s,condenser_command(4,5));    /* 8, distance 2 */
    submit(s,pipe_command(3,4));         /* 9 */
    submit(s,condenser_command(4,4));    /* 10, distance 2 */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    set_steam(s,6U,200U); set_steam(s,8U,200U); set_steam(s,10U,200U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_condenser(s,6U,&c)==FACTORY_RESULT_OK);
    CHECK(c.powered && c.activity==FACTORY_STEAM_CONDENSER_WORKING);
    CHECK(factory_simulation_get_steam_condenser(s,8U,&c)==FACTORY_RESULT_OK);
    CHECK(c.powered && c.activity==FACTORY_STEAM_CONDENSER_WORKING);
    /* Deterministic ascending-ID priority: the third (50 more demand
     * needed, only 0 left of the 100-unit generator after the first two)
     * stalls. */
    CHECK(factory_simulation_get_steam_condenser(s,10U,&c)==FACTORY_RESULT_OK);
    CHECK(!c.powered && c.activity==FACTORY_STEAM_CONDENSER_NO_POWER);
    CHECK(c.stored_steam==200U);

    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* ---- Snapshots ---- */

static bool snapshot_equal(const FactorySimulation *a,const FactorySimulation *b)
{
    FactorySnapshotBuffer x={0}; FactorySnapshotBuffer y={0}; bool equal=false;
    if (factory_simulation_create_snapshot(a,&x)==FACTORY_RESULT_OK
        && factory_simulation_create_snapshot(b,&y)==FACTORY_RESULT_OK)
        equal=x.size==y.size && memcmp(x.data,y.data,x.size)==0;
    factory_snapshot_buffer_destroy(&x); factory_snapshot_buffer_destroy(&y);
    return equal;
}

static void check_round_trip(FactorySimulation *s,FactoryEntityId id)
{
    FactorySnapshotBuffer buffer={0};
    FactorySimulation *loaded=NULL;
    FactorySteamCondenserInspection before;
    FactorySteamCondenserInspection after;
    CHECK(factory_simulation_get_steam_condenser(s,id,&before)
        ==FACTORY_RESULT_OK);
    CHECK(factory_simulation_create_snapshot(s,&buffer)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(
        buffer.data,buffer.size,&loaded)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_steam_condenser(loaded,id,&after)
        ==FACTORY_RESULT_OK);
    CHECK(before.stored_steam==after.stored_steam);
    CHECK(before.stored_water==after.stored_water);
    CHECK(before.definition_id==after.definition_id);
    factory_snapshot_buffer_destroy(&buffer);
    factory_simulation_destroy(loaded);
}

static void test_snapshots_across_states(void)
{
    FactoryWorld *w;
    FactoryEntityId id;
    FactorySimulation *s=build_powered_condenser(&w,&id);

    check_round_trip(s,id); /* idle: never yet evaluated */

    set_steam(s,id,0U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    check_round_trip(s,id); /* no steam */

    set_steam(s,id,250U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    check_round_trip(s,id); /* working, partial steam/water left */

    set_water(s,id,1950U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    check_round_trip(s,id); /* output full */

    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* ---- Determinism ---- */

static FactorySimulation *build_determinism_scenario(FactoryWorld **out_world)
{
    FactoryWorld *w=factory_world_create(6U,6U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,UINT32_MAX);
    submit(s,pole(2,2));
    submit(s,generator(2,3));
    submit(s,condenser_command(3,2));
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    set_steam(s,3U,500U);
    *out_world=w;
    return s;
}

static void test_determinism(void)
{
    FactoryWorld *world_a; FactoryWorld *world_b;
    FactorySimulation *a=build_determinism_scenario(&world_a);
    FactorySimulation *b=build_determinism_scenario(&world_b);
    uint32_t tick;
    CHECK(snapshot_equal(a,b));
    for (tick=0U;tick<10U;++tick) {
        CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b)==FACTORY_RESULT_OK);
        CHECK(snapshot_equal(a,b));
        CHECK(factory_simulation_get_event_count(a)
            ==factory_simulation_get_event_count(b));
    }
    factory_simulation_destroy(b); factory_simulation_destroy(a);
    factory_world_destroy(world_b); factory_world_destroy(world_a);
}

/* ---- Presentation ---- */

static void test_presentation(void)
{
    FactoryWorld *w;
    FactoryEntityId id;
    FactorySimulation *s=build_powered_condenser(&w,&id);
    FactoryPresentationSnapshot *snapshot=factory_presentation_snapshot_create();
    size_t i;
    bool found=false;
    set_steam(s,id,250U);
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_presentation_snapshot_rebuild(snapshot,s)==FACTORY_RESULT_OK);
    for (i=0U;i<factory_presentation_snapshot_get_entity_count(snapshot);++i) {
        const FactoryPresentationEntity *e=
            factory_presentation_snapshot_get_entity(snapshot,i);
        if (e->entity_id!=id) continue;
        CHECK(e->entity_type==FACTORY_ENTITY_TYPE_STEAM_CONDENSER);
        CHECK(e->powered);
        CHECK(e->status==FACTORY_PRESENTATION_MACHINE_STATUS_WORKING);
        CHECK(e->data.steam_condenser.stored_steam==150U);
        CHECK(e->data.steam_condenser.stored_water==100U);
        CHECK(e->data.steam_condenser.completed_cycles_last_tick==1U);
        found=true;
    }
    CHECK(found);
    factory_presentation_snapshot_destroy(snapshot);
    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* ---- Integration: Reactor -> Heat Exchanger -> Live Steam -> Steam
 * Turbine -> Exhaust Steam -> Steam Condenser -> Water, a genuine closed
 * loop connected entirely by pipes and driven by the generic per-tick
 * network pressure equalization (factory_fluid_network_transfer), not by
 * directly writing into the condenser's storage. This is the scenario the
 * turbine-exhaust correction exists to make possible: the condenser
 * recovers the turbine's own exhaust rather than independently drawing
 * fresh steam from the boiler/heat-exchanger side of the loop. ---- */

static void test_full_thermal_cycle(void)
{
    FactoryWorld *w=factory_world_create(9U,6U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,10000U);
    FactoryFluidStorage *water_input;
    uint32_t tick;
    uint64_t total_events_seen=0U;
    bool condenser_cycle_seen=false;
    bool turbine_cycle_seen=false;

    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_REACTOR_CORE,
        {.place_reactor_core={1,2}}});                       /* 1 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={2,2}}});                      /* 2 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={3,2}}});                      /* 3 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_EXCHANGER,
        {.place_heat_exchanger={4,2}}});                      /* 4 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={5,2}}});                                /* 5, live
                                                                * steam link */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={6,2}}});                       /* 6 */
    submit(s,pole(5,3));                                      /* 7 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={7,2}}});                                /* 8, east of
                                                                * the turbine:
                                                                * exhaust
                                                                * link */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={7,3}}});                                /* 9, west of
                                                                * the
                                                                * condenser */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_CONDENSER,
        {.place_steam_condenser={8,3}}});                     /* 10 */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    for (size_t i=0U;i<10U;++i)
        CHECK(factory_simulation_get_command_result(s,i)->result
            ==FACTORY_RESULT_OK);

    water_input=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,4U,FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT);
    CHECK(water_input!=NULL && factory_fluid_storage_insert(
        water_input,FACTORY_FLUID_WATER,1000U)==FACTORY_RESULT_OK);
    /* No direct write into the condenser's storage: its only supply is
     * whatever the turbine exhausts, delivered through the pipe network's
     * generic pressure equalization -- the fluid-network transfer/topology
     * mechanics themselves are already covered end-to-end by the
     * fluid-network test suite. */

    submit(s,(FactoryCommand){FACTORY_COMMAND_INSERT_REACTOR_FUEL,
        {.insert_reactor_fuel={1U,FACTORY_NUCLEAR_FUEL_BASIC_ROD}}});
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_get_command_result(s,0U)->result
        ==FACTORY_RESULT_OK);

    for (tick=0U;tick<60U;++tick) {
        size_t i;
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
        total_events_seen+=factory_simulation_get_event_count(s);
        for (i=0U;i<factory_simulation_get_event_count(s);++i) {
            const FactoryEvent *e=factory_simulation_get_event(s,i);
            if (e->type==FACTORY_EVENT_STEAM_CONDENSER_CYCLE_COMPLETED)
                condenser_cycle_seen=true;
            if (e->type==FACTORY_EVENT_STEAM_TURBINE_CYCLE_COMPLETED)
                turbine_cycle_seen=true;
        }
    }
    CHECK(total_events_seen!=0U);
    CHECK(condenser_cycle_seen);
    CHECK(turbine_cycle_seen);
    {
        FactorySteamCondenserInspection condenser;
        FactorySteamTurbineInspection turbine;
        FactoryPowerConsumerInspection consumer;
        CHECK(factory_simulation_get_steam_condenser(s,10U,&condenser)
            ==FACTORY_RESULT_OK);
        /* Water increased; steam was consumed; nothing manufactured beyond
         * what the recipe accounts for. */
        CHECK(condenser.stored_water!=0U);
        CHECK(condenser.stored_water % 100U==0U);
        CHECK(condenser.stored_steam+condenser.stored_water<=2000U+2000U);
        CHECK(condenser.stored_steam==0U || condenser.steam_fluid
            ==FACTORY_FLUID_EXHAUST_STEAM);
        CHECK(factory_simulation_get_steam_turbine(s,6U,&turbine)
            ==FACTORY_RESULT_OK);
        CHECK(turbine.actual_output==0U || turbine.actual_output==200U);
        CHECK(factory_simulation_get_power_consumer(s,10U,&consumer)
            ==FACTORY_RESULT_OK);
        CHECK(consumer.demand==50U);
    }

    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* ---- Backpressure: a real closed loop (Water -> Heat Exchanger -> Live
 * Steam -> Steam Turbine -> Exhaust Steam -> Steam Condenser -> Water) with
 * downstream water removal deliberately blocked, run long enough to reach
 * and hold a genuine steady state. The condenser has its own independent
 * power source (not the turbine) so its blocking reason is attributable
 * purely to its output being full, not a secondary power failure caused by
 * the turbine also having stopped. Every tick, the sum of fluid quantities
 * across all six storage slots in the chain is checked against the total
 * ever inserted from outside: this is a tick-independent conservation
 * invariant, not a one-shot timing assertion, and it alone proves nothing
 * is silently discarded or duplicated anywhere in the cascade. ---- */

static void test_closed_loop_backpressure(void)
{
    FactoryWorld *w=factory_world_create(9U,6U);
    FactorySimulation *s=factory_simulation_create_with_construction_units(
        w,10000U);
    FactoryFluidStorage *hx_water;
    FactoryFluidStorage *condenser_water_out;
    uint64_t total_water_inserted;
    uint32_t tick;

    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_REACTOR_CORE,
        {.place_reactor_core={1,2}}});                       /* 1 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={2,2}}});                      /* 2 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={3,2}}});                      /* 3 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_EXCHANGER,
        {.place_heat_exchanger={4,2}}});                      /* 4 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={5,2}}});                                /* 5 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={6,2}}});                       /* 6 */
    submit(s,pole(5,3));                                      /* 7 */
    /* Independent power source: the condenser's blocking reason must be
     * its own full output, not the turbine's generation also stopping. */
    submit(s,generator(5,4));                                 /* 8 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={7,2}}});                                /* 9 */
    submit(s,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={7,3}}});                                /* 10 */
    submit(s,condenser_command(8,3));                         /* 11 */
    CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);
    for (size_t i=0U;i<11U;++i)
        CHECK(factory_simulation_get_command_result(s,i)->result
            ==FACTORY_RESULT_OK);

    /* Block downstream water removal: pre-fill the condenser's water
     * output to within 50 units of capacity -- less than one 100-unit
     * cycle's worth of room -- so it can never complete a cycle. */
    condenser_water_out=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,11U,FACTORY_FLUID_STORAGE_STEAM_CONDENSER_OUTPUT);
    CHECK(condenser_water_out!=NULL && factory_fluid_storage_insert(
        condenser_water_out,FACTORY_FLUID_WATER,1950U)==FACTORY_RESULT_OK);

    hx_water=factory_fluid_storage_store_find_slot_mutable(
        &s->fluid_storages,4U,FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT);
    CHECK(hx_water!=NULL && factory_fluid_storage_insert(
        hx_water,FACTORY_FLUID_WATER,1000U)==FACTORY_RESULT_OK);
    total_water_inserted=1000ULL+1950ULL;

    /* An effectively unlimited upstream water/heat supply: sustaining the
     * cascade far enough to actually saturate every downstream stage is
     * the point of this test, not modeling extractor throughput or fuel
     * duration (both already covered elsewhere). */
    for (tick=0U;tick<150U;++tick) {
        FactorySteamCondenserInspection condenser;
        FactorySteamTurbineInspection turbine;
        FactoryHeatExchangerInspection exchanger;
        uint64_t total;
        if (hx_water->quantity<100U) {
            FactoryFluidQuantity room=1000U-hx_water->quantity;
            CHECK(factory_fluid_storage_insert(hx_water,FACTORY_FLUID_WATER,
                room)==FACTORY_RESULT_OK);
            total_water_inserted+=room;
        }
        s->reactors.items[0].heat_storage.stored_heat=9000U;
        CHECK(factory_simulation_tick(s)==FACTORY_RESULT_OK);

        CHECK(factory_simulation_get_heat_exchanger(s,4U,&exchanger)
            ==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_steam_turbine(s,6U,&turbine)
            ==FACTORY_RESULT_OK);
        CHECK(factory_simulation_get_steam_condenser(s,11U,&condenser)
            ==FACTORY_RESULT_OK);
        total=(uint64_t)exchanger.stored_water+exchanger.stored_steam
            +turbine.stored_steam+turbine.stored_exhaust
            +condenser.stored_steam+condenser.stored_water;
        CHECK(total==total_water_inserted);
    }

    /* Steady state, reached well before tick 150 and held indefinitely
     * once reached: no single fragile tick number is being relied on, any
     * sufficiently large tick budget reaches the same final invariants. */
    {
        FactorySteamCondenserInspection condenser;
        FactorySteamTurbineInspection turbine;
        FactoryHeatExchangerInspection exchanger;

        CHECK(factory_simulation_get_steam_condenser(s,11U,&condenser)
            ==FACTORY_RESULT_OK);
        /* 1-2: condenser output full, stopped converting. */
        CHECK(condenser.activity==FACTORY_STEAM_CONDENSER_OUTPUT_FULL);
        CHECK(condenser.completed_cycles_last_tick==0U);
        CHECK(condenser.powered);
        /* condenser water output never grew past the deliberate seed: it
         * never completed a single cycle from tick zero onward. */
        CHECK(condenser.stored_water==1950U);
        /* 3: condenser's exhaust input backed up toward capacity. */
        CHECK(condenser.stored_steam>=1900U);

        CHECK(factory_simulation_get_steam_turbine(s,6U,&turbine)
            ==FACTORY_RESULT_OK);
        /* 4: turbine exhaust output backed up toward capacity. */
        CHECK(turbine.stored_exhaust>=1900U);
        /* 5-7: turbine availability zero, stopped firing, stopped
         * consuming live steam -- specifically because exhaust is full,
         * not for any other reason. */
        CHECK(turbine.activity==FACTORY_STEAM_TURBINE_BLOCKED_EXHAUST_FULL);
        CHECK(turbine.available_output==0U);
        CHECK(turbine.actual_output==0U);
        CHECK(turbine.steam_consumed_last_tick==0U);
        CHECK(turbine.exhaust_produced_last_tick==0U);
        /* 8: upstream live-steam storage backed up too. */
        CHECK(turbine.stored_steam>=900U);

        CHECK(factory_simulation_get_heat_exchanger(s,4U,&exchanger)
            ==FACTORY_RESULT_OK);
        /* 9: Heat Exchanger eventually stops, blocked on its own full
         * steam output -- not on missing heat or water, both of which
         * this test keeps abundant. */
        CHECK(exchanger.activity==FACTORY_HEAT_EXCHANGER_BLOCKED_STEAM_FULL);
        CHECK(exchanger.produced_steam_last_tick==0U);

        /* 10: final conservation check, restated in closed form: every
         * unit of water ever inserted is accounted for across exactly the
         * six storage slots in the chain -- none discarded, none
         * duplicated. */
        CHECK((uint64_t)exchanger.stored_water+exchanger.stored_steam
            +turbine.stored_steam+turbine.stored_exhaust
            +condenser.stored_steam+condenser.stored_water
            ==total_water_inserted);
    }

    factory_simulation_destroy(s); factory_world_destroy(w);
}

/* Compares everything the milestone's snapshot-continuation requirement
 * lists: fluid quantities across all six chain slots, power commitments,
 * turbine/condenser cycle outcomes, event batches, raw snapshot bytes, and
 * presentation state. */
static void assert_chain_matches(
    const FactorySimulation *a, const FactorySimulation *b
)
{
    FactoryHeatExchangerInspection hx_a,hx_b;
    FactorySteamTurbineInspection turbine_a,turbine_b;
    FactorySteamCondenserInspection condenser_a,condenser_b;
    FactoryPowerConsumerInspection consumer_a,consumer_b;
    FactoryPowerGeneratorInspection gen_a,gen_b;
    size_t i;

    CHECK(factory_simulation_get_heat_exchanger(a,4U,&hx_a)==FACTORY_RESULT_OK
        && factory_simulation_get_heat_exchanger(b,4U,&hx_b)
            ==FACTORY_RESULT_OK);
    CHECK(hx_a.stored_water==hx_b.stored_water
        && hx_a.stored_steam==hx_b.stored_steam
        && hx_a.activity==hx_b.activity
        && hx_a.produced_steam_last_tick==hx_b.produced_steam_last_tick);

    CHECK(factory_simulation_get_steam_turbine(a,6U,&turbine_a)
            ==FACTORY_RESULT_OK
        && factory_simulation_get_steam_turbine(b,6U,&turbine_b)
            ==FACTORY_RESULT_OK);
    CHECK(turbine_a.stored_steam==turbine_b.stored_steam
        && turbine_a.stored_exhaust==turbine_b.stored_exhaust
        && turbine_a.actual_output==turbine_b.actual_output
        && turbine_a.available_output==turbine_b.available_output
        && turbine_a.steam_consumed_last_tick
            ==turbine_b.steam_consumed_last_tick
        && turbine_a.exhaust_produced_last_tick
            ==turbine_b.exhaust_produced_last_tick
        && turbine_a.completed_cycles_last_tick
            ==turbine_b.completed_cycles_last_tick
        && turbine_a.activity==turbine_b.activity);

    CHECK(factory_simulation_get_steam_condenser(a,11U,&condenser_a)
            ==FACTORY_RESULT_OK
        && factory_simulation_get_steam_condenser(b,11U,&condenser_b)
            ==FACTORY_RESULT_OK);
    CHECK(condenser_a.stored_steam==condenser_b.stored_steam
        && condenser_a.stored_water==condenser_b.stored_water
        && condenser_a.steam_consumed_last_tick
            ==condenser_b.steam_consumed_last_tick
        && condenser_a.water_produced_last_tick
            ==condenser_b.water_produced_last_tick
        && condenser_a.completed_cycles_last_tick
            ==condenser_b.completed_cycles_last_tick
        && condenser_a.activity==condenser_b.activity
        && condenser_a.powered==condenser_b.powered);

    CHECK(factory_simulation_get_power_consumer(a,11U,&consumer_a)
            ==FACTORY_RESULT_OK
        && factory_simulation_get_power_consumer(b,11U,&consumer_b)
            ==FACTORY_RESULT_OK);
    CHECK(consumer_a.demand==consumer_b.demand
        && consumer_a.powered==consumer_b.powered
        && consumer_a.connected==consumer_b.connected);
    CHECK(factory_simulation_get_power_generator(a,6U,&gen_a)
            ==FACTORY_RESULT_OK
        && factory_simulation_get_power_generator(b,6U,&gen_b)
            ==FACTORY_RESULT_OK);
    CHECK(gen_a.generation_capacity==gen_b.generation_capacity
        && gen_a.committed_output==gen_b.committed_output
        && gen_a.connected==gen_b.connected);

    CHECK(factory_simulation_get_event_count(a)
        ==factory_simulation_get_event_count(b));
    for (i=0U;i<factory_simulation_get_event_count(a);++i) {
        const FactoryEvent *ea=factory_simulation_get_event(a,i);
        const FactoryEvent *eb=factory_simulation_get_event(b,i);
        CHECK(ea->type==eb->type && ea->entity_id==eb->entity_id
            && ea->fluid_type==eb->fluid_type
            && ea->related_fluid_type==eb->related_fluid_type
            && ea->quantity==eb->quantity
            && ea->related_quantity==eb->related_quantity
            && ea->third_quantity==eb->third_quantity);
    }

    {
        FactoryPresentationSnapshot *snap_a=
            factory_presentation_snapshot_create();
        FactoryPresentationSnapshot *snap_b=
            factory_presentation_snapshot_create();
        CHECK(factory_presentation_snapshot_rebuild(snap_a,a)
                ==FACTORY_RESULT_OK
            && factory_presentation_snapshot_rebuild(snap_b,b)
                ==FACTORY_RESULT_OK);
        CHECK(factory_presentation_snapshot_get_entity_count(snap_a)
            ==factory_presentation_snapshot_get_entity_count(snap_b));
        for (i=0U;i<factory_presentation_snapshot_get_entity_count(snap_a);
                ++i) {
            const FactoryPresentationEntity *pa=
                factory_presentation_snapshot_get_entity(snap_a,i);
            const FactoryPresentationEntity *pb=
                factory_presentation_snapshot_get_entity(snap_b,i);
            CHECK(pa->entity_id==pb->entity_id
                && pa->entity_type==pb->entity_type
                && pa->x==pb->x && pa->y==pb->y
                && pa->status==pb->status && pa->powered==pb->powered);
        }
        factory_presentation_snapshot_destroy(snap_a);
        factory_presentation_snapshot_destroy(snap_b);
    }

    CHECK(snapshot_equal(a,b));
}

static void test_snapshot_continuation_across_full_chain(void)
{
    FactoryWorld *w;
    FactorySimulation *a;
    FactorySimulation *b=NULL;
    FactorySnapshotBuffer buffer={0};
    FactoryFluidStorage *hx_water_in;
    FactoryFluidStorage *hx_steam_out;
    FactoryFluidStorage *turbine_steam_in;
    FactoryFluidStorage *turbine_exhaust_out;
    FactoryFluidStorage *condenser_exhaust_in;
    FactoryFluidStorage *condenser_water_out;
    uint32_t tick;

    w=factory_world_create(9U,6U);
    a=factory_simulation_create_with_construction_units(w,10000U);
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_REACTOR_CORE,
        {.place_reactor_core={1,2}}});                       /* 1 */
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={2,2}}});                      /* 2 */
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_CONDUCTOR,
        {.place_heat_conductor={3,2}}});                      /* 3 */
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_HEAT_EXCHANGER,
        {.place_heat_exchanger={4,2}}});                      /* 4 */
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={5,2}}});                                /* 5 */
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_STEAM_TURBINE,
        {.place_steam_turbine={6,2}}});                       /* 6 */
    submit(a,pole(5,3));                                      /* 7 */
    submit(a,generator(5,4));                                 /* 8 */
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={7,2}}});                                /* 9 */
    submit(a,(FactoryCommand){FACTORY_COMMAND_PLACE_PIPE,
        {.place_pipe={7,3}}});                                /* 10 */
    submit(a,condenser_command(8,3));                         /* 11 */
    CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
    for (size_t i=0U;i<11U;++i)
        CHECK(factory_simulation_get_command_result(a,i)->result
            ==FACTORY_RESULT_OK);

    /* Seed nonzero authoritative quantities across every stage: heat
     * exchanger water input and live-steam output, turbine live-steam
     * input and exhaust-steam output, condenser exhaust-steam input and
     * water output -- all six storage slots named by the requirement. */
    hx_water_in=factory_fluid_storage_store_find_slot_mutable(
        &a->fluid_storages,4U,FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT);
    hx_steam_out=factory_fluid_storage_store_find_slot_mutable(
        &a->fluid_storages,4U,FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_OUTPUT);
    turbine_steam_in=factory_fluid_storage_store_find_slot_mutable(
        &a->fluid_storages,6U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    turbine_exhaust_out=factory_fluid_storage_store_find_slot_mutable(
        &a->fluid_storages,6U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT);
    condenser_exhaust_in=factory_fluid_storage_store_find_slot_mutable(
        &a->fluid_storages,11U,FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT);
    condenser_water_out=factory_fluid_storage_store_find_slot_mutable(
        &a->fluid_storages,11U,FACTORY_FLUID_STORAGE_STEAM_CONDENSER_OUTPUT);
    CHECK(hx_water_in!=NULL && hx_steam_out!=NULL && turbine_steam_in!=NULL
        && turbine_exhaust_out!=NULL && condenser_exhaust_in!=NULL
        && condenser_water_out!=NULL);
    CHECK(factory_fluid_storage_insert(hx_water_in,FACTORY_FLUID_WATER,300U)
        ==FACTORY_RESULT_OK);
    CHECK(factory_fluid_storage_insert(hx_steam_out,FACTORY_FLUID_STEAM,200U)
        ==FACTORY_RESULT_OK);
    CHECK(factory_fluid_storage_insert(
        turbine_steam_in,FACTORY_FLUID_STEAM,400U)==FACTORY_RESULT_OK);
    CHECK(factory_fluid_storage_insert(turbine_exhaust_out,
        FACTORY_FLUID_EXHAUST_STEAM,250U)==FACTORY_RESULT_OK);
    CHECK(factory_fluid_storage_insert(condenser_exhaust_in,
        FACTORY_FLUID_EXHAUST_STEAM,150U)==FACTORY_RESULT_OK);
    CHECK(factory_fluid_storage_insert(
        condenser_water_out,FACTORY_FLUID_WATER,500U)==FACTORY_RESULT_OK);
    a->reactors.items[0].heat_storage.stored_heat=500U;

    CHECK(factory_simulation_create_snapshot(a,&buffer)==FACTORY_RESULT_OK);
    CHECK(factory_simulation_load_snapshot(buffer.data,buffer.size,&b)
        ==FACTORY_RESULT_OK);

    /* Immediately after load: identical fluid quantities and identical raw
     * snapshot bytes. Activity and last-tick fields (completed cycles,
     * consumed/produced amounts) are deliberately NOT compared here: they
     * describe what happened during a tick's processing, and 'b' has not
     * been ticked yet, so they legitimately read as idle/zero on 'b' while
     * 'a' still carries whatever it last computed before the snapshot was
     * even taken -- neither side is wrong, they are just not comparable
     * until both have actually ticked past the load. */
    CHECK(factory_simulation_get_event_count(b)==0U);
    CHECK(hx_water_in->quantity==factory_fluid_storage_store_find_slot(
        &b->fluid_storages,4U,FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_INPUT)
        ->quantity);
    CHECK(hx_steam_out->quantity==factory_fluid_storage_store_find_slot(
        &b->fluid_storages,4U,FACTORY_FLUID_STORAGE_HEAT_EXCHANGER_OUTPUT)
        ->quantity);
    CHECK(turbine_steam_in->quantity==factory_fluid_storage_store_find_slot(
        &b->fluid_storages,6U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT)
        ->quantity);
    CHECK(turbine_exhaust_out->quantity
        ==factory_fluid_storage_store_find_slot(
            &b->fluid_storages,6U,FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT)
            ->quantity);
    CHECK(condenser_exhaust_in->quantity
        ==factory_fluid_storage_store_find_slot(
            &b->fluid_storages,11U,
            FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT)->quantity);
    CHECK(condenser_water_out->quantity
        ==factory_fluid_storage_store_find_slot(
            &b->fluid_storages,11U,
            FACTORY_FLUID_STORAGE_STEAM_CONDENSER_OUTPUT)->quantity);
    CHECK(snapshot_equal(a,b));

    /* Continue both together: from the first tick after the load onward,
     * both sides have actually ticked, so activity and last-tick fields
     * become meaningfully comparable too. Fluid transport, power dispatch,
     * and both machines' recipes must produce bit-identical outcomes tick
     * by tick, not just at the moment of the snapshot. */
    for (tick=0U;tick<20U;++tick) {
        CHECK(factory_simulation_tick(a)==FACTORY_RESULT_OK);
        CHECK(factory_simulation_tick(b)==FACTORY_RESULT_OK);
        assert_chain_matches(a,b);
    }

    factory_snapshot_buffer_destroy(&buffer);
    factory_simulation_destroy(b);
    factory_simulation_destroy(a);
    factory_world_destroy(w);
}

int main(void)
{
    test_definition();
    test_construction_and_occupancy();
    test_demolition();
    test_empty_input_rejects_live_steam();
    test_no_partial_execution_and_exact_accounting();
    test_power_gates_execution();
    test_snapshots_across_states();
    test_determinism();
    test_presentation();
    test_full_thermal_cycle();
    test_closed_loop_backpressure();
    test_snapshot_continuation_across_full_chain();
    if (failures!=0) return 1;
    (void)printf("All steam condenser tests passed.\n");
    return 0;
}
