#ifndef FOUNDATION_STEAM_CONDENSER_INTERNAL_H
#define FOUNDATION_STEAM_CONDENSER_INTERNAL_H

#include "foundation/steam_condenser.h"

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactorySteamCondenserDefinitionId definition_id;
    FactoryFluidQuantity steam_consumed_last_tick;
    FactoryFluidQuantity water_produced_last_tick;
    uint32_t completed_cycles_last_tick;
    FactorySteamCondenserActivity activity;
} FactorySteamCondenser;

typedef struct {
    FactorySteamCondenser *items;
    size_t count;
    size_t capacity;
} FactorySteamCondenserStore;

void factory_steam_condenser_store_destroy(FactorySteamCondenserStore *store);
bool factory_steam_condenser_store_reserve_one(
    FactorySteamCondenserStore *store);
void factory_steam_condenser_store_add(
    FactorySteamCondenserStore *store, FactoryEntityId id, int32_t x, int32_t y);
const FactorySteamCondenser *factory_steam_condenser_store_find(
    const FactorySteamCondenserStore *store, FactoryEntityId id);
FactorySteamCondenser *factory_steam_condenser_store_find_mutable(
    FactorySteamCondenserStore *store, FactoryEntityId id);
bool factory_steam_condenser_store_remove(
    FactorySteamCondenserStore *store, FactoryEntityId id);
void factory_steam_condensers_update(FactorySimulation *simulation);

#endif
