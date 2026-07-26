#ifndef FOUNDATION_ACCUMULATOR_INTERNAL_H
#define FOUNDATION_ACCUMULATOR_INTERNAL_H

#include "foundation/accumulator.h"

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryElectricalEnergy stored_energy;
    FactoryPowerUnits charged_last_tick;
    FactoryPowerUnits discharged_last_tick;
} FactoryAccumulator;

typedef struct {
    FactoryAccumulator *items;
    size_t count;
    size_t capacity;
} FactoryAccumulatorStore;

void factory_accumulator_store_destroy(FactoryAccumulatorStore *store);
bool factory_accumulator_store_reserve_one(FactoryAccumulatorStore *store);
void factory_accumulator_store_add(
    FactoryAccumulatorStore *store,
    FactoryEntityId id, int32_t x, int32_t y);
const FactoryAccumulator *factory_accumulator_store_find(
    const FactoryAccumulatorStore *store, FactoryEntityId id);
FactoryAccumulator *factory_accumulator_store_find_mutable(
    FactoryAccumulatorStore *store, FactoryEntityId id);
bool factory_accumulator_store_remove(
    FactoryAccumulatorStore *store, FactoryEntityId id);
void factory_accumulator_begin_tick(FactorySimulation *simulation);

#endif
