#ifndef FOUNDATION_FLUID_MACHINE_H
#define FOUNDATION_FLUID_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "foundation/burner.h"
#include "foundation/fluid.h"

#define FACTORY_WATER_EXTRACTOR_CYCLE_TICKS 5U
#define FACTORY_WATER_EXTRACTOR_OUTPUT_QUANTITY 100U
#define FACTORY_WATER_EXTRACTOR_CAPACITY 1000U
#define FACTORY_BOILER_STORAGE_CAPACITY 1000U

typedef enum {
    FACTORY_FLUID_RECIPE_NONE = 0,
    FACTORY_FLUID_RECIPE_BOIL_WATER
} FactoryFluidRecipeId;

typedef struct {
    FactoryFluidRecipeId recipe_id;
    FactoryFluidType input_fluid;
    FactoryFluidQuantity input_quantity;
    FactoryEnergy energy;
    FactoryFluidType output_fluid;
    FactoryFluidQuantity output_quantity;
} FactoryFluidConversionRecipe;

typedef struct {
    FactoryEntityId entity_id;
    uint32_t progress;
    uint32_t duration;
    FactoryFluidQuantity stored_water;
    FactoryFluidQuantity capacity;
} FactoryWaterExtractorInspection;

typedef struct {
    FactoryEntityId entity_id;
    FactoryFluidRecipeId recipe_id;
    FactoryFluidQuantity stored_water;
    FactoryFluidQuantity water_capacity;
    FactoryFluidQuantity stored_steam;
    FactoryFluidQuantity steam_capacity;
    bool conversion_active;
} FactoryBoilerInspection;

size_t factory_fluid_conversion_recipe_count(void);
const FactoryFluidConversionRecipe *factory_fluid_conversion_recipe_get(
    FactoryFluidRecipeId recipe_id
);
bool factory_fluid_conversion_recipe_is_valid(
    const FactoryFluidConversionRecipe *recipe
);

typedef struct FactorySimulation FactorySimulation;
FactoryResult factory_simulation_get_water_extractor(
    const FactorySimulation *simulation, FactoryEntityId entity_id,
    FactoryWaterExtractorInspection *out_extractor
);
FactoryResult factory_simulation_get_boiler(
    const FactorySimulation *simulation, FactoryEntityId entity_id,
    FactoryBoilerInspection *out_boiler
);

#endif
