#include "steam_turbine_internal.h"
#include "foundation/content.h"

#include "fluid_internal.h"
#include "simulation_internal.h"
#include "foundation/construction.h"

#include <stdlib.h>

size_t factory_steam_turbine_definition_count(void)
{
    return factory_content_steam_turbine_count();
}

const FactorySteamTurbineDefinition *factory_steam_turbine_definition_at(
    size_t index)
{
    return factory_content_steam_turbine_at(index);
}

const FactorySteamTurbineDefinition *factory_steam_turbine_definition_get(
    FactorySteamTurbineDefinitionId id)
{
    return factory_content_steam_turbine_get(id);
}

bool factory_steam_turbine_definition_is_valid(
    const FactorySteamTurbineDefinition *d)
{
    return d != NULL
        && d->definition_id == FACTORY_STEAM_TURBINE_DEFINITION_BASIC
        && d->input_fluid == FACTORY_FLUID_STEAM
        && d->exhaust_fluid == FACTORY_FLUID_EXHAUST_STEAM
        && d->input_fluid != d->exhaust_fluid
        && d->steam_per_cycle != 0U && d->exhaust_per_cycle != 0U
        && d->energy_per_cycle != 0U
        && d->maximum_cycles_per_tick != 0U
        && d->maximum_output_per_tick
            == d->energy_per_cycle * d->maximum_cycles_per_tick
        && d->storage_capacity == FACTORY_STEAM_TURBINE_STORAGE_CAPACITY
        && d->exhaust_capacity == FACTORY_STEAM_TURBINE_STORAGE_CAPACITY
        && d->construction_cost == FACTORY_CONSTRUCTION_COST_STEAM_TURBINE;
}

static bool reserve(FactorySteamTurbineStore *store)
{
    size_t next;
    FactorySteamTurbine *items;
    if (store->count < store->capacity) return true;
    next = store->capacity == 0U ? 4U : store->capacity * 2U;
    if (next < store->capacity || next > SIZE_MAX / sizeof(*items))
        return false;
    items = realloc(store->items, next * sizeof(*items));
    if (items == NULL) return false;
    store->items = items; store->capacity = next;
    return true;
}

void factory_steam_turbine_store_destroy(FactorySteamTurbineStore *store)
{
    if (store == NULL) return;
    free(store->items); *store = (FactorySteamTurbineStore){0};
}

bool factory_steam_turbine_store_reserve_one(FactorySteamTurbineStore *store)
{
    return store != NULL && reserve(store);
}

void factory_steam_turbine_store_add(
    FactorySteamTurbineStore *store, FactoryEntityId id, int32_t x, int32_t y)
{
    store->items[store->count++] = (FactorySteamTurbine){
        .entity_id=id, .x=x, .y=y,
        .definition_id=FACTORY_STEAM_TURBINE_DEFINITION_BASIC};
}

const FactorySteamTurbine *factory_steam_turbine_store_find(
    const FactorySteamTurbineStore *store, FactoryEntityId id)
{
    size_t i;
    if (store == NULL) return NULL;
    for (i=0U;i<store->count;++i)
        if (store->items[i].entity_id==id) return &store->items[i];
    return NULL;
}

FactorySteamTurbine *factory_steam_turbine_store_find_mutable(
    FactorySteamTurbineStore *store, FactoryEntityId id)
{
    return (FactorySteamTurbine *)factory_steam_turbine_store_find(store,id);
}

bool factory_steam_turbine_store_remove(
    FactorySteamTurbineStore *store, FactoryEntityId id)
{
    size_t i;
    if (store == NULL) return false;
    for (i=0U;i<store->count;++i) if (store->items[i].entity_id==id) {
        --store->count; store->items[i]=store->items[store->count]; return true;
    }
    return false;
}

void factory_steam_turbine_begin_tick(FactorySimulation *simulation)
{
    size_t i;
    if (simulation == NULL) return;
    for (i=0U;i<simulation->steam_turbines.count;++i) {
        FactorySteamTurbine *t=&simulation->steam_turbines.items[i];
        t->actual_output=0U; t->steam_consumed_last_tick=0U;
        t->exhaust_produced_last_tick=0U;
        t->completed_cycles_last_tick=0U; t->activity=FACTORY_STEAM_TURBINE_IDLE;
    }
}

FactoryPowerUnits factory_steam_turbine_available_generation(
    const FactorySimulation *simulation, FactoryEntityId id)
{
    const FactorySteamTurbine *t;
    const FactorySteamTurbineDefinition *d;
    const FactoryFluidStorage *storage;
    const FactoryFluidStorage *exhaust;
    uint32_t cycles;
    uint32_t exhaust_room_cycles;
    if (simulation == NULL) return 0U;
    t=factory_steam_turbine_store_find(&simulation->steam_turbines,id);
    if (t==NULL) return 0U;
    d=factory_steam_turbine_definition_get(t->definition_id);
    storage=factory_fluid_storage_store_find_slot(&simulation->fluid_storages,
        id,FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    exhaust=factory_fluid_storage_store_find_slot(&simulation->fluid_storages,
        id,FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT);
    if (!factory_steam_turbine_definition_is_valid(d)
        || storage==NULL || exhaust==NULL
        || (storage->quantity!=0U&&storage->fluid_type!=d->input_fluid)
        || (exhaust->quantity!=0U&&exhaust->fluid_type!=d->exhaust_fluid))
        return 0U;
    cycles=storage->quantity/d->steam_per_cycle;
    exhaust_room_cycles=
        (exhaust->capacity-exhaust->quantity)/d->exhaust_per_cycle;
    if (exhaust_room_cycles<cycles) cycles=exhaust_room_cycles;
    if (cycles>d->maximum_cycles_per_tick) cycles=d->maximum_cycles_per_tick;
    return cycles*d->energy_per_cycle;
}

/*
 * A cycle's exhaust is authoritative fluid, not a byproduct discarded here:
 * turbine exhaust is what a Steam Condenser recovers into water downstream,
 * so a cycle that cannot also place its exhaust is not committed at all --
 * the same all-or-nothing atomicity that already governs steam intake.
 */
bool factory_steam_turbine_consume_for_generation(
    FactorySimulation *simulation, FactoryEntityId id,
    FactoryPowerUnits generated)
{
    FactorySteamTurbine *t;
    const FactorySteamTurbineDefinition *d;
    FactoryFluidStorage *storage;
    FactoryFluidStorage *exhaust;
    uint32_t cycles;
    FactoryFluidQuantity consumed;
    FactoryFluidQuantity produced;
    if (simulation==NULL||generated==0U) return false;
    t=factory_steam_turbine_store_find_mutable(&simulation->steam_turbines,id);
    if (t==NULL) return false;
    d=factory_steam_turbine_definition_get(t->definition_id);
    storage=factory_fluid_storage_store_find_slot_mutable(
        &simulation->fluid_storages,id,
        FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT);
    exhaust=factory_fluid_storage_store_find_slot_mutable(
        &simulation->fluid_storages,id,
        FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT);
    if (!factory_steam_turbine_definition_is_valid(d)
        || generated>d->maximum_output_per_tick
        || generated%d->energy_per_cycle!=0U
        || storage==NULL || exhaust==NULL
        || storage->fluid_type!=d->input_fluid
        || (exhaust->quantity!=0U && exhaust->fluid_type!=d->exhaust_fluid))
        return false;
    cycles=generated/d->energy_per_cycle;
    consumed=cycles*d->steam_per_cycle;
    produced=cycles*d->exhaust_per_cycle;
    if (storage->quantity<consumed
        || produced>exhaust->capacity-exhaust->quantity) return false;
    storage->quantity-=consumed;
    if (storage->quantity==0U) storage->fluid_type=FACTORY_FLUID_NONE;
    exhaust->fluid_type=d->exhaust_fluid;
    exhaust->quantity+=produced;
    t->actual_output=generated; t->steam_consumed_last_tick=consumed;
    t->exhaust_produced_last_tick=produced;
    t->completed_cycles_last_tick=cycles;
    t->activity=FACTORY_STEAM_TURBINE_WORKING;
    factory_simulation_emit_event(simulation,(FactoryEvent){
        .type=FACTORY_EVENT_STEAM_TURBINE_CYCLE_COMPLETED,
        .entity_id=id, .fluid_type=d->input_fluid,
        .related_fluid_type=d->exhaust_fluid,
        .nuclear_fuel_id=(uint32_t)d->definition_id,
        .quantity=cycles, .related_quantity=consumed,
        .third_quantity=generated});
    return true;
}

void factory_steam_turbine_finish_tick(FactorySimulation *simulation)
{
    size_t i;
    if (simulation==NULL) return;
    for (i=0U;i<simulation->steam_turbines.count;++i) {
        FactorySteamTurbine *t=&simulation->steam_turbines.items[i];
        FactoryFluidStorageInspection storage;
        FactoryFluidStorageInspection exhaust;
        FactoryFluidPortInspection port;
        FactoryPowerGeneratorInspection generator;
        const FactorySteamTurbineDefinition *d=
            factory_steam_turbine_definition_get(t->definition_id);
        if (t->actual_output!=0U) continue;
        if (factory_simulation_get_fluid_port_slot(simulation,t->entity_id,
                FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT,&port)
                !=FACTORY_RESULT_OK || port.network_id==FACTORY_FLUID_NETWORK_NONE) {
            t->activity=FACTORY_STEAM_TURBINE_DISCONNECTED_FLUID; continue;
        }
        if (factory_simulation_get_power_generator(simulation,t->entity_id,
                &generator)!=FACTORY_RESULT_OK || !generator.connected) {
            t->activity=FACTORY_STEAM_TURBINE_DISCONNECTED_POWER; continue;
        }
        if (factory_simulation_get_fluid_storage_slot(simulation,t->entity_id,
                FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT,&storage)
                !=FACTORY_RESULT_OK || storage.quantity==0U) {
            t->activity=FACTORY_STEAM_TURBINE_BLOCKED_NO_STEAM; continue;
        }
        if (storage.quantity<d->steam_per_cycle) {
            t->activity=FACTORY_STEAM_TURBINE_BLOCKED_INSUFFICIENT_STEAM;
            continue;
        }
        if (factory_simulation_get_fluid_storage_slot(simulation,t->entity_id,
                FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT,&exhaust)
                !=FACTORY_RESULT_OK
            || d->exhaust_per_cycle>exhaust.capacity-exhaust.quantity
            || (exhaust.quantity!=0U
                && exhaust.fluid_type!=d->exhaust_fluid)) {
            t->activity=FACTORY_STEAM_TURBINE_BLOCKED_EXHAUST_FULL;
            continue;
        }
        t->activity=FACTORY_STEAM_TURBINE_BLOCKED_NO_DEMAND;
    }
}

FactoryResult factory_simulation_get_steam_turbine(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactorySteamTurbineInspection *out)
{
    const FactorySteamTurbine *t;
    const FactorySteamTurbineDefinition *d;
    FactoryFluidStorageInspection storage;
    FactoryFluidStorageInspection exhaust;
    FactoryFluidPortInspection port={0};
    FactoryFluidPortInspection exhaust_port={0};
    FactoryPowerGeneratorInspection generator={0};
    if (simulation==NULL||id==0U||out==NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    t=factory_steam_turbine_store_find(&simulation->steam_turbines,id);
    if (t==NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    d=factory_steam_turbine_definition_get(t->definition_id);
    if (!factory_steam_turbine_definition_is_valid(d)
        || factory_simulation_get_fluid_storage_slot(simulation,id,
            FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT,&storage)
            !=FACTORY_RESULT_OK
        || factory_simulation_get_fluid_storage_slot(simulation,id,
            FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT,&exhaust)
            !=FACTORY_RESULT_OK) return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    (void)factory_simulation_get_fluid_port_slot(simulation,id,
        FACTORY_FLUID_STORAGE_STEAM_TURBINE_INPUT,&port);
    (void)factory_simulation_get_fluid_port_slot(simulation,id,
        FACTORY_FLUID_STORAGE_STEAM_TURBINE_OUTPUT,&exhaust_port);
    (void)factory_simulation_get_power_generator(simulation,id,&generator);
    *out=(FactorySteamTurbineInspection){
        id,t->definition_id,storage.fluid_type,storage.quantity,storage.capacity,
        exhaust.fluid_type,exhaust.quantity,exhaust.capacity,
        port.network_id,exhaust_port.network_id,generator.network_id,
        port.network_id!=FACTORY_FLUID_NETWORK_NONE,
        exhaust_port.network_id!=FACTORY_FLUID_NETWORK_NONE,
        generator.connected,
        d->steam_per_cycle,d->exhaust_per_cycle,
        d->energy_per_cycle,d->maximum_output_per_tick,
        factory_steam_turbine_available_generation(simulation,id),
        t->actual_output,t->steam_consumed_last_tick,
        t->exhaust_produced_last_tick,
        t->completed_cycles_last_tick,t->activity};
    return FACTORY_RESULT_OK;
}
