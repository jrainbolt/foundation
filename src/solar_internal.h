#ifndef FOUNDATION_SOLAR_INTERNAL_H
#define FOUNDATION_SOLAR_INTERNAL_H

#include "foundation/solar.h"

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryPowerUnits generated_last_tick;
} FactorySolarGenerator;

typedef struct {
    FactorySolarGenerator *items;
    size_t count;
    size_t capacity;
} FactorySolarGeneratorStore;

void factory_solar_generator_store_destroy(FactorySolarGeneratorStore *store);
bool factory_solar_generator_store_reserve_one(FactorySolarGeneratorStore *store);
void factory_solar_generator_store_add(
    FactorySolarGeneratorStore *store, FactoryEntityId id, int32_t x, int32_t y);
const FactorySolarGenerator *factory_solar_generator_store_find(
    const FactorySolarGeneratorStore *store, FactoryEntityId id);
bool factory_solar_generator_store_remove(
    FactorySolarGeneratorStore *store, FactoryEntityId id);
void factory_solar_generator_begin_tick(FactorySimulation *simulation);
FactoryPowerUnits factory_solar_generator_available(
    const FactorySimulation *simulation, FactoryEntityId id);
bool factory_solar_generator_record_generation(
    FactorySimulation *simulation, FactoryEntityId id,
    FactoryPowerUnits generated);

#endif
