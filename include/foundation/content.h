#ifndef FOUNDATION_CONTENT_H
#define FOUNDATION_CONTENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/assembler_recipe.h"
#include "foundation/burner.h"
#include "foundation/construction.h"
#include "foundation/fluid.h"
#include "foundation/fluid_machine.h"
#include "foundation/heat_network.h"
#include "foundation/reactor.h"
#include "foundation/recipe.h"
#include "foundation/research.h"
#include "foundation/steam_engine.h"
#include "foundation/steam_turbine.h"
#include "foundation/steam_condenser.h"

typedef enum {
    FACTORY_CONTENT_RECIPE_FAMILY_NONE = 0,
    FACTORY_CONTENT_RECIPE_FAMILY_REFINERY,
    FACTORY_CONTENT_RECIPE_FAMILY_ASSEMBLER,
    FACTORY_CONTENT_RECIPE_FAMILY_FLUID,
    FACTORY_CONTENT_RECIPE_FAMILY_STEAM
} FactoryContentRecipeFamily;

typedef enum {
    FACTORY_CONTENT_POWER_ROLE_NONE = 0,
    FACTORY_CONTENT_POWER_ROLE_CONSUMER = 1U << 0U,
    FACTORY_CONTENT_POWER_ROLE_GENERATOR = 1U << 1U,
    FACTORY_CONTENT_POWER_ROLE_DISTRIBUTION = 1U << 2U,
    FACTORY_CONTENT_POWER_ROLE_STORAGE = 1U << 3U
} FactoryContentPowerRole;

typedef enum {
    FACTORY_CONTENT_FLUID_ROLE_NONE = 0,
    FACTORY_CONTENT_FLUID_ROLE_STORAGE = 1U << 0U,
    FACTORY_CONTENT_FLUID_ROLE_TRANSPORT = 1U << 1U,
    FACTORY_CONTENT_FLUID_ROLE_PRODUCER = 1U << 2U,
    FACTORY_CONTENT_FLUID_ROLE_CONSUMER = 1U << 3U
} FactoryContentFluidRole;

typedef enum {
    FACTORY_CONTENT_HEAT_ROLE_NONE = 0,
    FACTORY_CONTENT_HEAT_ROLE_TRANSPORT = 1U << 0U,
    FACTORY_CONTENT_HEAT_ROLE_PRODUCER = 1U << 1U,
    FACTORY_CONTENT_HEAT_ROLE_CONSUMER = 1U << 2U
} FactoryContentHeatRole;

#define FACTORY_CONTENT_ORIENTATION_NONE (-1)

typedef struct {
    FactoryEntityType entity_type;
    FactoryConstructionMaterial construction_cost;
    uint32_t footprint_width;
    uint32_t footprint_height;
    int32_t default_orientation;
    FactoryUnlockFlags required_unlock;
    FactoryContentRecipeFamily recipe_family;
    uint32_t power_roles;
    uint32_t fluid_roles;
    uint32_t heat_roles;
} FactoryEntityDefinition;

typedef struct {
    FactoryRecipeId recipe_id;
    FactoryRecipe recipe;
} FactoryRefineryRecipeDefinition;

typedef struct {
    const FactoryEntityDefinition *entities;
    size_t entity_count;
    const FactoryRefineryRecipeDefinition *refinery_recipes;
    size_t refinery_recipe_count;
    const FactoryAssemblerRecipe *assembler_recipes;
    size_t assembler_recipe_count;
    const FactoryTechnologyDefinition *technologies;
    size_t technology_count;
    const FactoryFuelDefinition *fuels;
    size_t fuel_count;
    const FactoryFluidDefinition *fluids;
    size_t fluid_count;
    const FactoryNuclearFuelDefinition *nuclear_fuels;
    size_t nuclear_fuel_count;
    const FactorySteamGenerationRecipe *steam_recipes;
    size_t steam_recipe_count;
    const FactoryFluidConversionRecipe *fluid_conversion_recipes;
    size_t fluid_conversion_recipe_count;
    const FactoryHeatExchangeRecipe *heat_exchange_recipes;
    size_t heat_exchange_recipe_count;
    const FactorySteamTurbineDefinition *steam_turbines;
    size_t steam_turbine_count;
    const FactorySteamCondenserDefinition *steam_condensers;
    size_t steam_condenser_count;
} FactoryContentView;

const FactoryContentView *factory_content_get(void);
bool factory_content_validate(void);
bool factory_content_validate_view(const FactoryContentView *view);
bool factory_content_entity_definitions_validate(
    const FactoryEntityDefinition *definitions,size_t count);
bool factory_content_refinery_recipes_validate(
    const FactoryRefineryRecipeDefinition *definitions,size_t count);
bool factory_content_assembler_recipes_validate(
    const FactoryAssemblerRecipe *definitions,size_t count);
bool factory_content_technologies_validate(
    const FactoryTechnologyDefinition *definitions,size_t count);
bool factory_content_fuels_validate(
    const FactoryFuelDefinition *definitions,size_t count);
bool factory_content_fluids_validate(
    const FactoryFluidDefinition *definitions,size_t count);
bool factory_content_nuclear_fuels_validate(
    const FactoryNuclearFuelDefinition *definitions,size_t count);
bool factory_content_steam_recipes_validate(
    const FactorySteamGenerationRecipe *definitions,size_t count);

size_t factory_content_entity_definition_count(void);
const FactoryEntityDefinition *factory_content_entity_definition_at(size_t index);
const FactoryEntityDefinition *factory_content_entity_definition_get(
    FactoryEntityType entity_type);
FactoryUnlockFlags factory_content_entity_unlock_requirement(FactoryEntityType entity_type);
bool factory_simulation_is_entity_unlocked(const FactorySimulation *simulation,FactoryEntityType entity_type);

size_t factory_content_refinery_recipe_count(void);
const FactoryRefineryRecipeDefinition *factory_content_refinery_recipe_at(
    size_t index);
const FactoryRefineryRecipeDefinition *factory_content_refinery_recipe_get(
    FactoryRecipeId id);

size_t factory_content_assembler_recipe_count(void);
const FactoryAssemblerRecipe *factory_content_assembler_recipe_at(size_t index);
const FactoryAssemblerRecipe *factory_content_assembler_recipe_get(
    FactoryAssemblerRecipeId id);
FactoryUnlockFlags factory_content_assembler_recipe_unlock_requirement(FactoryAssemblerRecipeId id);
bool factory_simulation_is_assembler_recipe_unlocked(const FactorySimulation *simulation,FactoryAssemblerRecipeId id);

size_t factory_content_technology_count(void);
const FactoryTechnologyDefinition *factory_content_technology_at(size_t index);
const FactoryTechnologyDefinition *factory_content_technology_get(
    FactoryTechnologyId id);

size_t factory_content_fuel_count(void);
const FactoryFuelDefinition *factory_content_fuel_at(size_t index);
const FactoryFuelDefinition *factory_content_fuel_get(FactoryItemType item);

size_t factory_content_fluid_count(void);
const FactoryFluidDefinition *factory_content_fluid_at(size_t index);
const FactoryFluidDefinition *factory_content_fluid_get(FactoryFluidType type);

size_t factory_content_nuclear_fuel_count(void);
const FactoryNuclearFuelDefinition *factory_content_nuclear_fuel_at(size_t index);
const FactoryNuclearFuelDefinition *factory_content_nuclear_fuel_get(
    FactoryNuclearFuelId id);

size_t factory_content_steam_recipe_count(void);
const FactorySteamGenerationRecipe *factory_content_steam_recipe_at(size_t index);
const FactorySteamGenerationRecipe *factory_content_steam_recipe_get(
    FactorySteamGenerationRecipeId id);

size_t factory_content_fluid_conversion_recipe_count(void);
const FactoryFluidConversionRecipe *factory_content_fluid_conversion_recipe_at(
    size_t index);
const FactoryFluidConversionRecipe *factory_content_fluid_conversion_recipe_get(
    FactoryFluidRecipeId id);
size_t factory_content_heat_exchange_recipe_count(void);
const FactoryHeatExchangeRecipe *factory_content_heat_exchange_recipe_at(
    size_t index);
const FactoryHeatExchangeRecipe *factory_content_heat_exchange_recipe_get(
    uint32_t id);
size_t factory_content_steam_turbine_count(void);
const FactorySteamTurbineDefinition *factory_content_steam_turbine_at(size_t index);
const FactorySteamTurbineDefinition *factory_content_steam_turbine_get(
    FactorySteamTurbineDefinitionId id);
size_t factory_content_steam_condenser_count(void);
const FactorySteamCondenserDefinition *factory_content_steam_condenser_at(
    size_t index);
const FactorySteamCondenserDefinition *factory_content_steam_condenser_get(
    FactorySteamCondenserDefinitionId id);

#endif
