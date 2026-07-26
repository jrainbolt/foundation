#ifndef FOUNDATION_REACTOR_INTERNAL_H
#define FOUNDATION_REACTOR_INTERNAL_H

#include "foundation/reactor.h"

#include <stdbool.h>

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryNuclearFuelId inventory_fuel_id;
    uint32_t inventory_quantity;
    FactoryNuclearFuelId active_fuel_id;
    FactoryHeatQuantity remaining_heat_yield;
    FactoryHeatStorage heat_storage;
    FactoryHeatQuantity generated_last_tick;
    FactoryReactorActivity activity;
} FactoryReactor;

typedef struct {
    FactoryReactor *items;
    size_t count;
    size_t capacity;
} FactoryReactorStore;

void factory_reactor_store_destroy(FactoryReactorStore *store);
bool factory_reactor_store_reserve_one(FactoryReactorStore *store);
void factory_reactor_store_add(
    FactoryReactorStore *store, FactoryEntityId id, int32_t x, int32_t y);
const FactoryReactor *factory_reactor_store_find(
    const FactoryReactorStore *store, FactoryEntityId id);
FactoryReactor *factory_reactor_store_find_mutable(
    FactoryReactorStore *store, FactoryEntityId id);
bool factory_reactor_store_remove(
    FactoryReactorStore *store, FactoryEntityId id);
FactoryResult factory_reactor_insert_fuel(
    FactoryReactor *reactor, FactoryNuclearFuelId fuel_id);
void factory_reactor_store_update(
    FactoryReactorStore *store, FactorySimulation *simulation);

#endif
