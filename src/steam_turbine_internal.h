#ifndef FOUNDATION_STEAM_TURBINE_INTERNAL_H
#define FOUNDATION_STEAM_TURBINE_INTERNAL_H

#include "foundation/steam_turbine.h"

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactorySteamTurbineDefinitionId definition_id;
    FactoryPowerUnits actual_output;
    FactoryFluidQuantity steam_consumed_last_tick;
    FactoryFluidQuantity exhaust_produced_last_tick;
    uint32_t completed_cycles_last_tick;
    FactorySteamTurbineActivity activity;
} FactorySteamTurbine;

typedef struct {
    FactorySteamTurbine *items;
    size_t count;
    size_t capacity;
} FactorySteamTurbineStore;

void factory_steam_turbine_store_destroy(FactorySteamTurbineStore *store);
bool factory_steam_turbine_store_reserve_one(FactorySteamTurbineStore *store);
void factory_steam_turbine_store_add(
    FactorySteamTurbineStore *store, FactoryEntityId id, int32_t x, int32_t y);
const FactorySteamTurbine *factory_steam_turbine_store_find(
    const FactorySteamTurbineStore *store, FactoryEntityId id);
FactorySteamTurbine *factory_steam_turbine_store_find_mutable(
    FactorySteamTurbineStore *store, FactoryEntityId id);
bool factory_steam_turbine_store_remove(
    FactorySteamTurbineStore *store, FactoryEntityId id);
void factory_steam_turbine_begin_tick(FactorySimulation *simulation);
FactoryPowerUnits factory_steam_turbine_available_generation(
    const FactorySimulation *simulation, FactoryEntityId id);
bool factory_steam_turbine_consume_for_generation(
    FactorySimulation *simulation, FactoryEntityId id,
    FactoryPowerUnits generated);
void factory_steam_turbine_finish_tick(FactorySimulation *simulation);

#endif
