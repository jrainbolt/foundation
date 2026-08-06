#include "steam_condenser_internal.h"
#include "foundation/content.h"

#include "fluid_internal.h"
#include "simulation_internal.h"
#include "foundation/construction.h"

#include <stdlib.h>

size_t factory_steam_condenser_definition_count(void)
{
    return factory_content_steam_condenser_count();
}

const FactorySteamCondenserDefinition *factory_steam_condenser_definition_at(
    size_t index)
{
    return factory_content_steam_condenser_at(index);
}

const FactorySteamCondenserDefinition *factory_steam_condenser_definition_get(
    FactorySteamCondenserDefinitionId id)
{
    return factory_content_steam_condenser_get(id);
}

bool factory_steam_condenser_definition_is_valid(
    const FactorySteamCondenserDefinition *d)
{
    return d != NULL
        && d->definition_id == FACTORY_STEAM_CONDENSER_DEFINITION_BASIC
        && d->input_fluid == FACTORY_FLUID_EXHAUST_STEAM
        && d->output_fluid == FACTORY_FLUID_WATER
        && d->input_fluid != d->output_fluid
        && d->steam_per_cycle != 0U && d->water_per_cycle != 0U
        && d->power_per_cycle != 0U && d->maximum_cycles_per_tick != 0U
        && d->steam_capacity == FACTORY_STEAM_CONDENSER_STEAM_CAPACITY
        && d->water_capacity == FACTORY_STEAM_CONDENSER_WATER_CAPACITY
        && d->construction_cost == FACTORY_CONSTRUCTION_COST_STEAM_CONDENSER;
}

static bool reserve(FactorySteamCondenserStore *store)
{
    size_t next;
    FactorySteamCondenser *items;
    if (store->count < store->capacity) return true;
    next = store->capacity == 0U ? 4U : store->capacity * 2U;
    if (next < store->capacity || next > SIZE_MAX / sizeof(*items))
        return false;
    items = realloc(store->items, next * sizeof(*items));
    if (items == NULL) return false;
    store->items = items; store->capacity = next;
    return true;
}

void factory_steam_condenser_store_destroy(FactorySteamCondenserStore *store)
{
    if (store == NULL) return;
    free(store->items); *store = (FactorySteamCondenserStore){0};
}

bool factory_steam_condenser_store_reserve_one(
    FactorySteamCondenserStore *store)
{
    return store != NULL && reserve(store);
}

void factory_steam_condenser_store_add(
    FactorySteamCondenserStore *store, FactoryEntityId id, int32_t x, int32_t y)
{
    store->items[store->count++] = (FactorySteamCondenser){
        .entity_id=id, .x=x, .y=y,
        .definition_id=FACTORY_STEAM_CONDENSER_DEFINITION_BASIC};
}

const FactorySteamCondenser *factory_steam_condenser_store_find(
    const FactorySteamCondenserStore *store, FactoryEntityId id)
{
    size_t i;
    if (store == NULL) return NULL;
    for (i=0U;i<store->count;++i)
        if (store->items[i].entity_id==id) return &store->items[i];
    return NULL;
}

FactorySteamCondenser *factory_steam_condenser_store_find_mutable(
    FactorySteamCondenserStore *store, FactoryEntityId id)
{
    return (FactorySteamCondenser *)factory_steam_condenser_store_find(
        store, id);
}

bool factory_steam_condenser_store_remove(
    FactorySteamCondenserStore *store, FactoryEntityId id)
{
    size_t i;
    if (store == NULL) return false;
    for (i=0U;i<store->count;++i) if (store->items[i].entity_id==id) {
        --store->count; store->items[i]=store->items[store->count]; return true;
    }
    return false;
}

/*
 * One deterministic recipe cycle per condenser per tick: consumes 100 steam
 * and the fixed power demand (already allocated by the generic power
 * dispatcher; this reads that outcome, it never allocates), and produces
 * 100 water. All validation precedes the atomic, allocation-free commit --
 * no partial or fractional conversion is possible. A complete cycle may
 * fire whenever any of it is committed; there is no reservation of "unused"
 * capacity the way an atomic power generator's quantum works, since a
 * condenser cycle's entire output is always consumer-facing water storage,
 * not a shared network pool.
 */
void factory_steam_condensers_update(FactorySimulation *simulation)
{
    const FactorySteamCondenserDefinition *d =
        factory_steam_condenser_definition_get(
            FACTORY_STEAM_CONDENSER_DEFINITION_BASIC);
    size_t i;
    if (simulation == NULL || !factory_steam_condenser_definition_is_valid(d))
        return;
    for (i=0U;i<simulation->steam_condensers.count;++i) {
        FactorySteamCondenser *c = &simulation->steam_condensers.items[i];
        FactoryFluidStorage *steam;
        FactoryFluidStorage *water;
        FactoryFluidPortInspection input_port;
        FactoryPowerConsumerInspection power;
        c->steam_consumed_last_tick = 0U;
        c->water_produced_last_tick = 0U;
        c->completed_cycles_last_tick = 0U;
        c->activity = FACTORY_STEAM_CONDENSER_IDLE;
        steam = factory_fluid_storage_store_find_slot_mutable(
            &simulation->fluid_storages, c->entity_id,
            FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT);
        water = factory_fluid_storage_store_find_slot_mutable(
            &simulation->fluid_storages, c->entity_id,
            FACTORY_FLUID_STORAGE_STEAM_CONDENSER_OUTPUT);
        if (steam == NULL || water == NULL) continue;
        if (factory_simulation_get_fluid_port_slot(simulation, c->entity_id,
                FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT, &input_port)
                != FACTORY_RESULT_OK
            || input_port.network_id == FACTORY_FLUID_NETWORK_NONE) {
            c->activity = FACTORY_STEAM_CONDENSER_DISCONNECTED_FLUID;
            continue;
        }
        if (factory_simulation_get_power_consumer(
                simulation, c->entity_id, &power) != FACTORY_RESULT_OK
            || !power.connected) {
            c->activity = FACTORY_STEAM_CONDENSER_DISCONNECTED_POWER;
            continue;
        }
        if (!power.powered) {
            c->activity = FACTORY_STEAM_CONDENSER_NO_POWER;
            continue;
        }
        if (steam->quantity < d->steam_per_cycle
            || (steam->quantity != 0U && steam->fluid_type != d->input_fluid)) {
            c->activity = FACTORY_STEAM_CONDENSER_NO_STEAM;
            continue;
        }
        if ((water->quantity != 0U && water->fluid_type != d->output_fluid)
            || d->water_per_cycle > water->capacity - water->quantity) {
            c->activity = FACTORY_STEAM_CONDENSER_OUTPUT_FULL;
            continue;
        }
        /* All validation precedes this atomic, allocation-free commit. */
        steam->quantity -= d->steam_per_cycle;
        if (steam->quantity == 0U) steam->fluid_type = FACTORY_FLUID_NONE;
        water->fluid_type = d->output_fluid;
        water->quantity += d->water_per_cycle;
        c->steam_consumed_last_tick = d->steam_per_cycle;
        c->water_produced_last_tick = d->water_per_cycle;
        c->completed_cycles_last_tick = 1U;
        c->activity = FACTORY_STEAM_CONDENSER_WORKING;
        factory_simulation_emit_event(simulation, (FactoryEvent){
            .type = FACTORY_EVENT_STEAM_CONDENSER_CYCLE_COMPLETED,
            .entity_id = c->entity_id,
            .fluid_type = d->input_fluid,
            .related_fluid_type = d->output_fluid,
            .quantity = d->steam_per_cycle,
            .related_quantity = d->water_per_cycle});
    }
}

FactoryResult factory_simulation_get_steam_condenser(
    const FactorySimulation *simulation, FactoryEntityId id,
    FactorySteamCondenserInspection *out)
{
    const FactorySteamCondenser *c;
    const FactorySteamCondenserDefinition *d;
    FactoryFluidStorageInspection steam={0};
    FactoryFluidStorageInspection water={0};
    FactoryFluidPortInspection input_port={0};
    FactoryFluidPortInspection output_port={0};
    FactoryPowerConsumerInspection power={0};
    if (simulation==NULL||id==0U||out==NULL)
        return FACTORY_RESULT_INVALID_ARGUMENT;
    c=factory_steam_condenser_store_find(&simulation->steam_condensers,id);
    if (c==NULL) return FACTORY_RESULT_ENTITY_NOT_FOUND;
    d=factory_steam_condenser_definition_get(c->definition_id);
    if (!factory_steam_condenser_definition_is_valid(d)
        || factory_simulation_get_fluid_storage_slot(simulation,id,
            FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT,&steam)
            !=FACTORY_RESULT_OK
        || factory_simulation_get_fluid_storage_slot(simulation,id,
            FACTORY_FLUID_STORAGE_STEAM_CONDENSER_OUTPUT,&water)
            !=FACTORY_RESULT_OK)
        return FACTORY_RESULT_INTERNAL_STATE_MISMATCH;
    (void)factory_simulation_get_fluid_port_slot(simulation,id,
        FACTORY_FLUID_STORAGE_STEAM_CONDENSER_INPUT,&input_port);
    (void)factory_simulation_get_fluid_port_slot(simulation,id,
        FACTORY_FLUID_STORAGE_STEAM_CONDENSER_OUTPUT,&output_port);
    (void)factory_simulation_get_power_consumer(simulation,id,&power);
    *out=(FactorySteamCondenserInspection){
        id,c->definition_id,
        steam.fluid_type,steam.quantity,steam.capacity,
        water.fluid_type,water.quantity,water.capacity,
        input_port.network_id,output_port.network_id,
        power.network_id,
        input_port.network_id!=FACTORY_FLUID_NETWORK_NONE,
        power.connected,power.powered,
        d->power_per_cycle,
        c->steam_consumed_last_tick,c->water_produced_last_tick,
        c->completed_cycles_last_tick,c->activity};
    return FACTORY_RESULT_OK;
}
