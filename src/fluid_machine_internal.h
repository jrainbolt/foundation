#ifndef FOUNDATION_FLUID_MACHINE_INTERNAL_H
#define FOUNDATION_FLUID_MACHINE_INTERNAL_H

#include "foundation/fluid_machine.h"

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    uint32_t progress;
} FactoryWaterExtractor;

typedef struct {
    FactoryWaterExtractor *items;
    size_t count;
    size_t capacity;
} FactoryWaterExtractorStore;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryFluidRecipeId recipe_id;
    bool conversion_active;
} FactoryBoiler;

typedef struct {
    FactoryBoiler *items;
    size_t count;
    size_t capacity;
} FactoryBoilerStore;

void factory_water_extractor_store_destroy(FactoryWaterExtractorStore *store);
void factory_boiler_store_destroy(FactoryBoilerStore *store);
bool factory_water_extractor_store_reserve_one(FactoryWaterExtractorStore *store);
bool factory_boiler_store_reserve_one(FactoryBoilerStore *store);
void factory_water_extractor_store_add(
    FactoryWaterExtractorStore *store, FactoryEntityId id, int32_t x, int32_t y
);
void factory_boiler_store_add(
    FactoryBoilerStore *store, FactoryEntityId id, int32_t x, int32_t y
);
const FactoryWaterExtractor *factory_water_extractor_store_find(
    const FactoryWaterExtractorStore *store, FactoryEntityId id
);
const FactoryBoiler *factory_boiler_store_find(
    const FactoryBoilerStore *store, FactoryEntityId id
);
bool factory_water_extractor_store_remove(
    FactoryWaterExtractorStore *store, FactoryEntityId id
);
bool factory_boiler_store_remove(FactoryBoilerStore *store, FactoryEntityId id);
void factory_fluid_machines_update(FactorySimulation *simulation);

#endif
