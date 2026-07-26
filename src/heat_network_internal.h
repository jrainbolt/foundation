#ifndef FOUNDATION_HEAT_NETWORK_INTERNAL_H
#define FOUNDATION_HEAT_NETWORK_INTERNAL_H

#include "foundation/heat_network.h"

typedef struct { FactoryEntityId entity_id; int32_t x; int32_t y; }
    FactoryHeatConductor;
typedef struct {
    FactoryHeatConductor *items; size_t count; size_t capacity;
} FactoryHeatConductorStore;

typedef struct {
    FactoryEntityId owner_entity_id;
    FactoryHeatPortSlot slot;
    int32_t x;
    int32_t y;
} FactoryHeatPort;
typedef struct { FactoryHeatPort *items; size_t count; size_t capacity; }
    FactoryHeatPortStore;

typedef struct {
    FactoryEntityId entity_id;
    int32_t x;
    int32_t y;
    FactoryHeatQuantity consumed_heat_last_tick;
    FactoryFluidQuantity consumed_water_last_tick;
    FactoryFluidQuantity produced_steam_last_tick;
    FactoryHeatExchangerActivity activity;
} FactoryHeatExchanger;
typedef struct {
    FactoryHeatExchanger *items; size_t count; size_t capacity;
} FactoryHeatExchangerStore;

typedef struct {
    FactoryHeatConductorInspection *conductors; size_t conductor_count;
    FactoryHeatPortInspection *ports; size_t port_count;
    FactoryHeatNetworkInspection *networks; size_t network_count;
    bool dirty;
} FactoryHeatNetworkState;

void factory_heat_conductor_store_destroy(FactoryHeatConductorStore *store);
bool factory_heat_conductor_store_reserve_one(FactoryHeatConductorStore *store);
void factory_heat_conductor_store_add(
    FactoryHeatConductorStore *store, FactoryEntityId id, int32_t x, int32_t y);
const FactoryHeatConductor *factory_heat_conductor_store_find(
    const FactoryHeatConductorStore *store, FactoryEntityId id);
bool factory_heat_conductor_store_remove(
    FactoryHeatConductorStore *store, FactoryEntityId id);
void factory_heat_port_store_destroy(FactoryHeatPortStore *store);
bool factory_heat_port_store_reserve_one(FactoryHeatPortStore *store);
void factory_heat_port_store_add(
    FactoryHeatPortStore *store, FactoryEntityId owner,
    FactoryHeatPortSlot slot, int32_t x, int32_t y);
bool factory_heat_port_store_remove(
    FactoryHeatPortStore *store, FactoryEntityId owner);
void factory_heat_exchanger_store_destroy(FactoryHeatExchangerStore *store);
bool factory_heat_exchanger_store_reserve_one(FactoryHeatExchangerStore *store);
void factory_heat_exchanger_store_add(
    FactoryHeatExchangerStore *store, FactoryEntityId id, int32_t x, int32_t y);
const FactoryHeatExchanger *factory_heat_exchanger_store_find(
    const FactoryHeatExchangerStore *store, FactoryEntityId id);
FactoryHeatExchanger *factory_heat_exchanger_store_find_mutable(
    FactoryHeatExchangerStore *store, FactoryEntityId id);
bool factory_heat_exchanger_store_remove(
    FactoryHeatExchangerStore *store, FactoryEntityId id);
void factory_heat_network_state_destroy(FactoryHeatNetworkState *state);
FactoryResult factory_heat_network_rebuild(
    FactorySimulation *simulation, bool emit_events);
void factory_heat_exchangers_update(FactorySimulation *simulation);

#endif
