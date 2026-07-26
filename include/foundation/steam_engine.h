#ifndef FOUNDATION_STEAM_ENGINE_H
#define FOUNDATION_STEAM_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/fluid.h"
#include "foundation/power.h"

#define FACTORY_STEAM_ENGINE_STORAGE_CAPACITY 1000U
#define FACTORY_STEAM_ENGINE_MAX_OUTPUT 100U

typedef enum {
    FACTORY_STEAM_GENERATION_RECIPE_NONE = 0,
    FACTORY_STEAM_GENERATION_RECIPE_BASIC
} FactorySteamGenerationRecipeId;

typedef struct {
    FactorySteamGenerationRecipeId recipe_id;
    FactoryFluidType input_fluid;
    FactoryFluidQuantity input_quantity;
    FactoryPowerUnits generated_energy;
    FactoryPowerUnits maximum_output_per_tick;
} FactorySteamGenerationRecipe;

typedef struct {
    FactoryEntityId entity_id;
    FactorySteamGenerationRecipeId recipe_id;
    FactoryFluidQuantity stored_steam;
    FactoryFluidQuantity steam_capacity;
    FactoryPowerUnits maximum_output_per_tick;
    FactoryPowerUnits generated_last_tick;
    bool active;
} FactorySteamEngineInspection;

size_t factory_steam_generation_recipe_count(void);
const FactorySteamGenerationRecipe *factory_steam_generation_recipe_get(
    FactorySteamGenerationRecipeId recipe_id
);
bool factory_steam_generation_recipe_is_valid(
    const FactorySteamGenerationRecipe *recipe
);

typedef struct FactorySimulation FactorySimulation;
FactoryResult factory_simulation_get_steam_engine(
    const FactorySimulation *simulation,
    FactoryEntityId entity_id,
    FactorySteamEngineInspection *out_engine
);

#endif
