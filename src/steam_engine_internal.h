#ifndef FOUNDATION_STEAM_ENGINE_INTERNAL_H
#define FOUNDATION_STEAM_ENGINE_INTERNAL_H

#include "foundation/steam_engine.h"

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactorySteamGenerationRecipeId recipe_id;
    FactoryPowerUnits generated_last_tick;
} FactorySteamEngine;

typedef struct {
    FactorySteamEngine *items;
    size_t count;
    size_t capacity;
} FactorySteamEngineStore;

void factory_steam_engine_store_destroy(FactorySteamEngineStore *store);
bool factory_steam_engine_store_reserve_one(FactorySteamEngineStore *store);
void factory_steam_engine_store_add(
    FactorySteamEngineStore *store,
    FactoryEntityId id,
    int32_t x,
    int32_t y
);
const FactorySteamEngine *factory_steam_engine_store_find(
    const FactorySteamEngineStore *store, FactoryEntityId id
);
FactorySteamEngine *factory_steam_engine_store_find_mutable(
    FactorySteamEngineStore *store, FactoryEntityId id
);
bool factory_steam_engine_store_remove(
    FactorySteamEngineStore *store, FactoryEntityId id
);
void factory_steam_engine_begin_tick(FactorySimulation *simulation);
FactoryPowerUnits factory_steam_engine_available_generation(
    const FactorySimulation *simulation, FactoryEntityId id
);
bool factory_steam_engine_consume_for_generation(
    FactorySimulation *simulation,
    FactoryEntityId id,
    FactoryPowerUnits generated
);

#endif
